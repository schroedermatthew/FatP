# Fat-P Tools - Build and Benchmark Scripts

Utility scripts for building and benchmarking the Fat-P library. **Run from project root.**

## Quick Start

```bash
# Linux/macOS
./tools/build.sh                    # Incremental build + test
./tools/rebuild.sh                  # Clean rebuild + test
./tools/run_all_benchmarks.sh       # Run all benchmarks

# Windows (use .bat wrappers to avoid execution policy issues)
.\tools\build.bat                   # Incremental build + test
.\tools\rebuild.bat                 # Clean rebuild + test
.\tools\run_all_benchmarks.bat      # Run all benchmarks
```

## Windows Execution Policy

PowerShell may block `.ps1` scripts with: *"cannot be loaded...not digitally signed"*

**Solution 1: Use the `.bat` wrappers (recommended)**
```cmd
.\tools\rebuild.bat
```

**Solution 2: Run PowerShell with bypass**
```powershell
powershell -ExecutionPolicy Bypass -File .\tools\rebuild.ps1
```

**Solution 3: Change policy for your user (one-time)**
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

## Build Scripts

| Script | Description |
|--------|-------------|
| `build.sh` / `build.bat` | Incremental build + run tests |
| `rebuild.sh` / `rebuild.bat` | Clean build directory, configure, build + test |

## Benchmark Runner

### Basic Usage

```bash
# Linux/macOS
./tools/run_all_benchmarks.sh

# Windows
.\tools\run_all_benchmarks.bat
```

### Options

| Option | Description |
|--------|-------------|
| `-d, --dir DIR` | Benchmark directory (default: `build/release`) |
| `-o, --output DIR` | Output directory (default: `benchmark_results`) |
| `-f, --filter GLOB` | Only run benchmarks matching pattern |
| `-q, --quick` | Quick mode (reduced iterations) |
| `-v, --verbose` | Show full output during execution |
| `-c, --continue` | Continue on error |
| `--dry-run` | Show what would run |

### Examples

```bash
# Run only hash-related benchmarks
./tools/run_all_benchmarks.sh -f "*Hash*"

# Quick mode for development iteration
./tools/run_all_benchmarks.sh -q

# Verbose output, continue on failures
./tools/run_all_benchmarks.sh -v -c

# Custom build directory
./tools/run_all_benchmarks.sh -d build/debug
```

### Output

Results are saved to `benchmark_results/YYYYMMDD_HHMMSS/`:
- `benchmark_ComponentName.txt` - Full console output
- `benchmark_ComponentName.csv` - CSV data (if supported)

## Environment Variables

Passed through to individual benchmarks:

| Variable | Default | Description |
|----------|---------|-------------|
| `FATP_BENCH_WARMUP_RUNS` | 3 | Warmup iterations |
| `FATP_BENCH_BATCHES` | 15 | Measured batches |
| `FATP_BENCH_SEED` | 12345 | RNG seed |
| `FATP_BENCH_NO_STABILIZE` | 0 | Skip CPU stabilization |
| `FATP_BENCH_NO_COOLDOWN` | 0 | Skip cooling delays |
| `FATP_BENCH_OUTPUT_CSV` | - | CSV output path |
| `FATP_BENCH_OUTPUT_JSON` | - | JSON output path |

## Quick Mode

Quick mode (`-q` / `-Quick`) sets:
- `FATP_BENCH_WARMUP_RUNS=1`
- `FATP_BENCH_BATCHES=5`
- `FATP_BENCH_NO_STABILIZE=1`

Use for fast iteration during development. **Not suitable for performance comparisons.**

## File Summary

```
tools/
├── build.sh / build.bat / build.ps1           # Incremental build
├── rebuild.sh / rebuild.bat / rebuild.ps1     # Clean rebuild
├── run_all_benchmarks.sh / .bat / .ps1        # Benchmark runner
└── BENCHMARK_RUNNER_README.md                 # This file
```
