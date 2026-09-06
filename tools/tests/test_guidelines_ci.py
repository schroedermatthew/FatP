# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/tests/test_guidelines_ci.py
#   layer: Infrastructure
#   summary: Exercises the actual guideline workflow ledger resolver in isolated Git histories.
#   api_stability: in_work

"""Durable CI command controls; temporary commits never touch the receiving repository."""
from __future__ import annotations

import os
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))

from guideline_lib import yaml_value
from lint_guidelines import ledger_values
from test_guideline_tools import ledger_fixture

LEDGER_PATH = "guidelines/DEMERITS.md"


def workflow():
    return yaml_value((REPO / ".github/workflows/guidelines.yml").read_text(encoding="utf-8-sig"))


def workflow_python(step_name):
    steps = workflow()["jobs"]["guidelines"]["steps"]
    matches = [step["run"] for step in steps if step.get("name") == step_name]
    if len(matches) != 1:
        raise ValueError(f"exactly one {step_name} step required")
    lines = matches[0].splitlines()
    if lines[0].strip() != "python - <<'PY'" or lines[-1].strip() != "PY":
        raise ValueError("expected standalone Python heredoc")
    code = "\n".join(lines[1:-1])
    compile(code, f"guidelines.yml {step_name}", "exec")
    return code


def resolver_source():
    return workflow_python("Resolve previous ledger")


def incremented_ledger(baseline, increase):
    assistants, _ = ledger_values(baseline)
    column = assistants.index("ChatGPT") + 2
    lines = baseline.splitlines()
    for index, line in enumerate(lines):
        if line.startswith("| D08 |") or line.startswith("| **Total** |"):
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            count = int(cells[column].strip("*")) + increase
            cells[column] = f"**{count}**" if cells[0] == "**Total**" else str(count)
            lines[index] = "| " + " | ".join(cells) + " |"
    text = "\n".join(lines) + "\n"
    ledger_values(text)
    return text


class GuidelinesCiTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory(prefix="fatp-guidelines-ci-")
        self.addCleanup(temporary.cleanup)
        self.repo = Path(temporary.name).resolve()
        self.env = os.environ.copy()
        for name in ("GIT_DIR", "GIT_WORK_TREE", "GIT_COMMON_DIR", "GIT_INDEX_FILE",
                     "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES"):
            self.env.pop(name, None)
        self.env.update({"GIT_CONFIG_GLOBAL": os.devnull, "GIT_CONFIG_NOSYSTEM": "1"})
        self.hooks = self.repo / "empty-hooks"
        self.hooks.mkdir()
        self.git("init", "--initial-branch=main")
        self.baseline = ledger_fixture()
        self.initial = self.commit({"README.md": "Temporary Git fixture.\n"})

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-c", "user.name=Guideline CI fixture", "-c", "user.email=fixture@example.invalid",
             "-c", "commit.gpgSign=false", "-c", "core.autocrlf=false", "-c", "core.safecrlf=false",
             "-c", f"core.hooksPath={self.hooks}", *arguments],
            cwd=self.repo, env=self.env, check=True, capture_output=True, text=True, timeout=30,
        ).stdout.strip()

    def commit(self, contents):
        for relative, text in contents.items():
            path = self.repo / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8", newline="\n")
        self.git("add", "--", *contents)
        self.git("commit", "-m", "Isolated ledger resolver fixture")
        return self.git("rev-parse", "HEAD")

    def run_resolver(self, revision="", *, fail_show=False):
        code = resolver_source()
        if fail_show:
            code = """import subprocess
real_run = subprocess.run
def controlled_run(command, *args, **kwargs):
    if command[:2] == ['git', 'show']:
        raise subprocess.CalledProcessError(128, command, stderr=b'controlled existing-blob read failure')
    return real_run(command, *args, **kwargs)
subprocess.run = controlled_run
""" + code
        return subprocess.run([sys.executable, "-c", code], cwd=self.repo,
                              env=dict(self.env, BASE_REV=revision), capture_output=True,
                              text=True, timeout=30)

    def assert_resolved(self, result, expected):
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        output = self.repo / "build/previous-guideline-ledger.md"
        self.assertEqual(expected.encode("utf-8"), output.read_bytes())

    def test_workflow_trigger_parity_read_permissions_and_checkout_history(self):
        data = workflow()
        self.assertEqual(data["on"]["push"]["paths"], data["on"]["pull_request"]["paths"])
        self.assertTrue(data["on"]["push"]["paths"])
        self.assertEqual({"contents": "read"}, data["permissions"])
        steps = data["jobs"]["guidelines"]["steps"]
        checkout = [step for step in steps if step.get("uses", "").startswith("actions/checkout@")]
        self.assertEqual(1, len(checkout))
        self.assertEqual(0, checkout[0]["with"]["fetch-depth"])
        self.assertIn("FATP_GUIDELINE_FORMATTER", data["jobs"]["guidelines"]["env"])
        self.assertIn("FATP_GUIDELINE_CLANG", data["jobs"]["guidelines"]["env"])
        self.assertTrue(resolver_source())

    def test_base_without_ledger_reports_no_prior_comparison(self):
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 5)})
        result = self.run_resolver(self.initial)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("without a prior comparison", result.stdout)
        self.assertFalse((self.repo / "build/previous-guideline-ledger.md").exists())

    def test_explicit_existing_base_uses_its_counts(self):
        previous = incremented_ledger(self.baseline, 4)
        base = self.commit({LEDGER_PATH: previous})
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 5)})
        self.assert_resolved(self.run_resolver(base), previous)

    def test_invalid_revision_and_missing_commit_cannot_fall_back(self):
        for revision in ("not-a-sha", "f" * 40):
            with self.subTest(revision=revision):
                result = self.run_resolver(revision)
                self.assertNotEqual(0, result.returncode)
                self.assertFalse((self.repo / "build/previous-guideline-ledger.md").exists())
                if revision == "not-a-sha":
                    self.assertIn("Invalid base revision", result.stderr)
                else:
                    self.assertIn("CalledProcessError", result.stderr)
                    self.assertIn("cat-file", result.stderr)

    def test_no_event_or_zero_before_uses_nearest_prior_new_ledger(self):
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 2)})
        previous = incremented_ledger(self.baseline, 4)
        self.commit({LEDGER_PATH: previous})
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 5)})
        for revision in ("", "0" * 40):
            with self.subTest(revision=revision):
                self.assert_resolved(self.run_resolver(revision), previous)

    def test_ancestor_search_reaches_ledger_before_an_intermediate_deletion(self):
        previous = incremented_ledger(self.baseline, 4)
        self.commit({LEDGER_PATH: previous})
        self.git("rm", "--", LEDGER_PATH)
        self.git("commit", "-m", "Isolated ledger deletion fixture")
        deleted_base = self.git("rev-parse", "HEAD")
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 5)})
        for revision in ("", deleted_base):
            with self.subTest(revision=revision):
                self.assert_resolved(self.run_resolver(revision), previous)

    def test_validation_step_uses_available_baseline_and_propagates_failure(self):
        checker = self.repo / "tools/lint_guidelines.py"
        checker.parent.mkdir()
        checker.write_text(
            "import json, sys\nfrom pathlib import Path\n"
            "Path('received-args.json').write_text(json.dumps(sys.argv[1:]))\n"
            "raise SystemExit(7 if '--previous-ledger' in sys.argv else 0)\n", encoding="utf-8")
        previous = self.repo / "build/previous-guideline-ledger.md"
        for available in (False, True):
            with self.subTest(baseline=available):
                if available:
                    previous.parent.mkdir()
                    previous.write_text(self.baseline, encoding="utf-8")
                result = subprocess.run(
                    [sys.executable, "-c", workflow_python("Validate corpus and ledger retention")],
                    cwd=self.repo, env=self.env, capture_output=True, text=True, timeout=30)
                self.assertEqual(7 if available else 0, result.returncode, result.stdout + result.stderr)
                arguments = json.loads((self.repo / "received-args.json").read_text(encoding="utf-8"))
                self.assertEqual(["guidelines", "--repo-root", ".", "--instantiated"], arguments[:4])
                self.assertEqual(available, "--previous-ledger" in arguments)
                if available:
                    self.assertEqual(previous, self.repo / arguments[-1])

    def test_no_prior_ledger_reports_no_comparison_and_discards_stale_output(self):
        result = self.run_resolver()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("without a prior comparison", result.stdout)
        self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 5)})
        output = self.repo / "build/previous-guideline-ledger.md"
        output.parent.mkdir()
        output.write_text("Stale prior attempt", encoding="utf-8")
        result = self.run_resolver()
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        self.assertIn("without a prior comparison", result.stdout)
        self.assertFalse(output.exists())

    def test_present_blob_read_failure_cannot_silently_fall_back(self):
        base = self.commit({LEDGER_PATH: incremented_ledger(self.baseline, 4)})
        result = self.run_resolver(base, fail_show=True)
        self.assertNotEqual(0, result.returncode)
        self.assertIn("CalledProcessError", result.stderr)
        self.assertIn("show", result.stderr)
        self.assertNotIn("without a prior comparison", result.stdout)
        self.assertFalse((self.repo / "build/previous-guideline-ledger.md").exists())


if __name__ == "__main__":
    unittest.main()
