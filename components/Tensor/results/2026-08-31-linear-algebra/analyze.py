"""Validate baseline exports, or run isolated randomized process-pair comparisons.

No third-party Python packages. Run without arguments to validate adjacent full
baseline JSON/CSV files. Use --help for a before/after experiment. Each fresh
process's API median is one replicate; internal batches are NOT independent pairs.
"""

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import tempfile


def validate(data):
    assert data["schema_version"] == 1
    series = data["cases"]
    assert len({(row["name"], row["implementation"]) for row in series}) == len(series)
    assert len(series) > 0 and len(series) % 3 == 0
    short = 0
    for row in series:
        assert row["result_allocations"] == 1 and row["result_bytes"] > 0
        samples = row["samples"]
        assert len(samples) == data["batches"]
        for sample in samples:
            assert sample["iterations"] > 0 and sample["elapsed_ns"] > 0
            assert math.isclose(sample["ns_per_call"], sample["elapsed_ns"] / sample["iterations"])
            short += sample["elapsed_ns"] < data["min_batch_ms"] * 1e6
        values = [sample["ns_per_call"] for sample in samples]
        assert all(math.isfinite(value) and value > 0 for value in values)
        assert math.isclose(row["median"], statistics.median(values), rel_tol=1e-12)
        assert math.isclose(row["mean"], statistics.mean(values), rel_tol=1e-12)
        assert math.isclose(row["stddev"], statistics.stdev(values), rel_tol=1e-10, abs_tol=1e-10)
        count = len(values)
        critical = 1.96 if count >= 120 else 2.00 if count >= 60 else 2.04 if count >= 30 else (
            2.14 if count >= 15 else 2.26 if count >= 10 else 2.78)
        margin = critical * row["stddev"] / math.sqrt(count)
        assert math.isclose(row["ci95_mean_low"], row["mean"] - margin, rel_tol=1e-10, abs_tol=1e-10)
        assert math.isclose(row["ci95_mean_high"], row["mean"] + margin, rel_tol=1e-10, abs_tol=1e-10)
    return {"series": len(series), "samples": sum(len(row["samples"]) for row in series),
            "short_batches": short,
            "min_batch_ms": min(s["elapsed_ns"] for row in series for s in row["samples"]) / 1e6,
            "median_cv_percent": statistics.median(row["stddev"] / row["mean"] * 100 for row in series),
            "max_cv_percent": max(row["stddev"] / row["mean"] * 100 for row in series)}


def validate_baselines(directory):
    for compiler in ("msvc", "gcc"):
        path = directory / f"{compiler}-baseline.json"
        data = json.loads(path.read_text())
        summary = validate(data)
        assert summary["series"] == 378 and summary["samples"] == 5670
        with path.with_suffix(".csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        assert len(rows) == summary["samples"]
        indexed = {(row["name"], row["implementation"]): row for row in data["cases"]}
        seen = set()
        for row in rows:
            key = (row["case"], row["implementation"])
            sample_index = int(row["sample"])
            assert (*key, sample_index) not in seen
            seen.add((*key, sample_index))
            original = indexed[key]
            assert int(row["result_bytes"]) == original["result_bytes"]
            for field in ("median", "mean", "stddev", "ci95_mean_low", "ci95_mean_high"):
                assert float(row[field]) == original[field]
            for field in ("iterations", "elapsed_ns", "ns_per_call", "cpu_mhz"):
                assert float(row[field]) == original["samples"][sample_index][field]
        print(compiler, json.dumps(summary))
        for name in ("dot/float/large/contiguous", "dot/double/large/contiguous"):
            print(name, {impl: indexed[name, impl]["median"]
                         for impl in ("fat_p", "scalar_prevalidated", "allocation_only")})


def bootstrap_improvement(before, after, seed):
    # Resample whole adjacent process pairs, never the nested timed batches.
    improvements = [1 - b / a for a, b in zip(before, after)]
    rng = random.Random(seed)
    draws = sorted(statistics.median(rng.choices(improvements, k=len(improvements))) for _ in range(10000))
    return {"before_median_ns": statistics.median(before), "after_median_ns": statistics.median(after),
            "paired_median_improvement_percent": statistics.median(improvements) * 100,
            "paired_bootstrap_ci95_percent": [draws[249] * 100, draws[9749] * 100]}


def compare(args):
    binaries = {label: Path(getattr(args, label)).resolve(strict=True) for label in ("before", "after")}
    if args.pairs < 7 or args.pairs > 100:
        raise ValueError("Use 7..100 fresh process pairs")
    destination = Path(args.output).resolve()
    if destination.exists():
        raise FileExistsError(f"Refusing to overwrite experiment: {destination}")
    env = {key: value for key, value in os.environ.items() if not key.startswith("FATP_BENCH_")}
    env.update(FATP_BENCH_WARMUP_RUNS="3", FATP_BENCH_BATCHES="7", FATP_BENCH_MIN_BATCH_MS="20",
               FATP_BENCH_TARGET_WORK="10000", FATP_BENCH_NO_STABILIZE="1", FATP_BENCH_NO_COOLDOWN="1")
    rng = random.Random(args.seed)
    records = []
    cases = args.case or [f"dot/{dtype}/large/contiguous" for dtype in ("float", "double")]
    if len(cases) != len(set(cases)):
        raise ValueError("Duplicate confirmation cases")
    with tempfile.TemporaryDirectory(prefix="fatp-dot-pairs-") as temporary:
        raw = Path(temporary) / "run.json"
        env["FATP_BENCH_OUTPUT_JSON"] = str(raw)
        for pair in range(args.pairs):
            rng.shuffle(cases)
            for case in cases:
                order = ["before", "after"]
                rng.shuffle(order)
                record = {"pair": pair, "case": case, "order": order, "runs": {}}
                env["FATP_BENCH_SEED"] = str(args.seed + pair)
                for label in order:
                    # The only child is the benchmark, synchronously awaited; no shell, builds, or overlapping runs.
                    result = subprocess.run([str(binaries[label]), "--filter", case], env=env,
                                            capture_output=True, text=True, check=True, timeout=60)
                    data = json.loads(raw.read_text())
                    quality = validate(data)
                    assert len(data["cases"]) == 3 and data["case_filter"] == case
                    record["runs"][label] = {"data": data, "quality": quality,
                                             "stdout": result.stdout, "stderr": result.stderr}
                records.append(record)
                print(args.compiler, "pair", pair + 1, case, "->".join(order), flush=True)
    summaries = {}
    for case in cases:
        matched = [record for record in records if record["case"] == case]
        before, after = [], []
        for label, values in (("before", before), ("after", after)):
            for record in matched:
                api = next(row for row in record["runs"][label]["data"]["cases"]
                           if row["implementation"] == "fat_p")
                values.append(api["median"])
        summaries[case] = bootstrap_improvement(before, after, args.seed)
    output = {"experiment_schema": 1, "compiler": args.compiler, "pairs_per_case": args.pairs,
              "randomization_seed": args.seed, "replicate": "median of one fresh process's seven API batches",
              "interval": "10000 whole-pair percentile bootstrap draws of median relative improvement",
              "binary_sha256": {key: hashlib.sha256(path.read_bytes()).hexdigest() for key, path in binaries.items()},
              "summaries": summaries, "records": records}
    destination.write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps(summaries, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before", help="Unmodified production-header benchmark executable")
    parser.add_argument("--after", help="Candidate benchmark executable; identical harness and compiler flags")
    parser.add_argument("--compiler", help="Recorded compiler label")
    parser.add_argument("--output", help="New consolidated comparison JSON path (no overwrite)")
    parser.add_argument("--pairs", type=int, default=15)
    parser.add_argument("--seed", type=int, default=98765)
    parser.add_argument("--case", action="append", help="Exact case name; repeatable; default: both large dots")
    args = parser.parse_args()
    if any((args.before, args.after, args.compiler, args.output, args.case)):
        if not all((args.before, args.after, args.compiler, args.output)):
            parser.error("Comparison requires --before, --after, --compiler, and --output")
        compare(args)
    else:
        validate_baselines(Path(__file__).resolve().parent)


if __name__ == "__main__":
    main()
