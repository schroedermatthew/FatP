# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/tests/test_guideline_tools.py
#   layer: Infrastructure
#   summary: Valid and invalid controls for the adopted guideline validators.
#   api_stability: in_work

"""Receiving-project controls ported from GuidelinesTemplate4's validator tests.

Run from the repository root with python -m unittest discover -s tools/tests -v.
Gates use actual formatter/Clang processes; unavailable tools fail, never skip.
"""
from __future__ import annotations

import copy
import json
import os
import re
import shutil
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO / "tools"))

from check_style import ast_check, check_file, check_files, lexical
from guideline_lib import (
    ONBOARDING, REQUIRED_DOCS, execute, formatter_integrity, inventory,
    load_component_headers, read, validate_profile,
)
from lint_guidelines import inherited_identity_errors, ledger, ledger_values
from lint_metadata import check_metadata

FORMATTER = os.environ.get("FATP_GUIDELINE_FORMATTER", "clang-format")
CLANG = os.environ.get("FATP_GUIDELINE_CLANG", "clang++")

def ledger_fixture():
    """Synthetic counts exercise retention independently of project awards."""
    rows = ["| ID | Violation | Claude | ChatGPT | Gemini | Grok |",
            "|---|---|---:|---:|---:|---:|"]
    for number in range(1, 25):
        rows.append(f"| D{number:02} | Synthetic category {number} | 0 | {2 if number == 8 else 0} | 0 | 0 |")
    rows.append("| **Total** | | **0** | **2** | **0** | **0** |")
    return "\n".join(rows) + "\n"


def configured_profile():
    return {
        "schema_version": 2,
        "status": "instantiated",
        "identity": {"name": "Validator fixture", "purpose": "Test checks", "owner": "Test fixture",
                     "namespace": "example", "macro_prefix": "EXAMPLE"},
        "languages": ["cpp"],
        "cpp": {"standard": 20, "layout": "header-only", "harness": "unittest compiler controls",
                "exception_policy": "throwing", "toolchains": ["Clang C++20"],
                "authored_globs": ["include/*.h"], "excluded_globs": [],
                "exclusion_reason": None, "component_headers_file": "guidelines/data/component_headers.json"},
        "build": {"ci": "none", "support_contract": "Synthetic local fixture",
                  "commands": [{"name": "control", "command": "python -m unittest", "cwd": ".",
                                "property": "validator controls", "when": "tool changes",
                                "evidence": "unittest result"}]},
        "compatibility": {"stage": "pre-release", "policy": "no-shims"},
        "metadata": {"enabled": True, "reason": "Metadata control", "authored_globs": ["include/*.h"],
                     "excluded_globs": [], "exclusion_reason": None,
                     "file_roles": ["public_header"], "layers": ["api"]},
        "peer_routes": [], "local_rules": [],
    }


VALID_HEADER = """#pragma once
/*
EXAMPLE_META:
  meta_version: 1
  component: Widget
  file_role: public_header
  path: include/Widget.h
  namespace: example
  layer: api
  summary: A validator control type.
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
*/

namespace example
{
struct Widget
{
    int count = 0;
};
} // namespace example
"""


class GuidelineToolTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="fatp-guideline-test-")
        self.addCleanup(self.temporary.cleanup)
        self.repo = Path(self.temporary.name).resolve()
        (self.repo / "include").mkdir()
        self.header = self.repo / "include/Widget.h"
        self.header.write_text(VALID_HEADER, encoding="utf-8")
        shutil.copyfile(REPO / ".clang-format", self.repo / ".clang-format")
        self.data = configured_profile()
        self.mapping = self.repo / self.data["cpp"]["component_headers_file"]
        self.mapping.parent.mkdir(parents=True)
        self.mapping.write_text("{}\n", encoding="utf-8")

    def metadata(self, text):
        self.header.write_text(text, encoding="utf-8")
        return check_metadata(self.header, self.repo, self.data["metadata"], "EXAMPLE")

    def write_profile(self):
        guidelines = self.repo / "guidelines"
        guidelines.mkdir(exist_ok=True)
        (guidelines / "PROJECT_PROFILE.md").write_text(
            "<!-- project-profile -->\n```json\n" + json.dumps(self.data) +
            "\n```\n<!-- /project-profile -->\n", encoding="utf-8")

    def metadata_cli(self, *arguments):
        self.write_profile()
        return execute([sys.executable, str(REPO / "tools/lint_metadata.py"),
                        "--repo-root", str(self.repo), "--guidelines", "guidelines", *arguments], cwd=self.repo)

    def mapping_control(self):
        source = self.repo / "control.cpp"
        source.write_text('#include "Widget.h"\n', encoding="utf-8")
        self.data["cpp"]["authored_globs"].append("control.cpp")
        self.data["metadata"]["authored_globs"].append("control.cpp")
        self.mapping.write_text(json.dumps({"control.cpp": "Widget.h"}), encoding="utf-8")
        return source

    def style_cli(self):
        self.write_profile()
        return execute([sys.executable, str(REPO / "tools/check_style.py"),
                        "--repo-root", str(self.repo), "--guidelines", "guidelines",
                        "--clang-format", FORMATTER, "--compiler", CLANG,
                        "--file", "control.cpp", "--", "-I", "include"], cwd=self.repo)

    def test_actual_receiving_prefix_is_not_inherited_identity(self):
        fatp = copy.deepcopy(self.data)
        fatp["identity"]["macro_prefix"] = "FATP"
        text = "The FATP_TEST harness uses FATP_ASSERT and FATP_CHECK; metadata uses FATP_META."
        self.assertEqual([], inherited_identity_errors(text, fatp, True))
        self.assertIn("inherited harness or metadata identity",
                      inherited_identity_errors(text, self.data, True))

    def test_template_identity_guard_cannot_be_disabled_by_prefix_alone(self):
        fatp = copy.deepcopy(self.data)
        fatp["identity"]["macro_prefix"] = "FATP"
        self.assertTrue(inherited_identity_errors("FATP_META", fatp, False))
        fatp["status"] = "unconfigured"
        self.assertTrue(inherited_identity_errors("FATP_META", fatp, True))
        self.assertEqual([], inherited_identity_errors("EXAMPLE_META", self.data, False))

    def test_obsolete_configuration_form_is_rejected_for_receiving_project(self):
        self.data["identity"]["macro_prefix"] = "FATP"
        self.assertEqual(["obsolete project-options reference"],
                         inherited_identity_errors("PROJECT_OPTIONS.md", self.data, True))

    def test_malformed_identity_does_not_crash_foreign_identity_check(self):
        self.data["identity"] = []
        self.assertIn("profile.identity: missing or unknown keys", validate_profile(self.data, self.repo, True))
        self.assertEqual(["inherited harness or metadata identity"],
                         inherited_identity_errors("FATP_META", self.data, True))

    def test_profile_valid_control_and_unknown_key(self):
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        invalid = copy.deepcopy(self.data)
        invalid["cpp"]["style_enabled"] = False
        self.assertIn("profile.cpp: missing or unknown keys", validate_profile(invalid, self.repo, True))

    def test_profile_cpp_minimum_and_metadata_coverage(self):
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        invalid = copy.deepcopy(self.data)
        invalid["cpp"]["standard"] = 17
        self.assertTrue(any("C++20 minimum" in value for value in validate_profile(invalid, self.repo, True)))
        extra = self.repo / "include/Extra.h"
        extra.write_text("#pragma once\n", encoding="utf-8")
        self.data["metadata"]["authored_globs"] = ["include/Widget.h"]
        self.assertIn("profile.metadata: authored C++ files fall outside metadata coverage",
                      validate_profile(self.data, self.repo, True))

    def test_external_component_headers_load_authored_suffix_and_sibling_spellings(self):
        source = self.mapping_control()
        self.assertEqual({"control.cpp": "Widget.h"}, load_component_headers(self.data, self.repo))
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        self.mapping.write_text(json.dumps({"control.cpp": "include/Widget.h"}), encoding="utf-8")
        self.assertEqual({"control.cpp": "include/Widget.h"}, load_component_headers(self.data, self.repo))
        sibling = source.with_suffix(".h")
        sibling.write_text("#pragma once\n", encoding="utf-8")
        self.data["cpp"]["authored_globs"].append("control.h")
        self.data["metadata"]["authored_globs"].append("control.h")
        self.mapping.write_text(json.dumps({"control.cpp": "control.h"}), encoding="utf-8")
        self.assertEqual({"control.cpp": "control.h"}, load_component_headers(self.data, self.repo))
        self.assertEqual([], validate_profile(self.data, self.repo, True))

    def test_external_component_headers_require_current_schema_and_single_authority(self):
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        for version in (1, 3, True, "2"):
            with self.subTest(version=version):
                invalid = copy.deepcopy(self.data)
                invalid["schema_version"] = version
                self.assertTrue(any("schema_version" in finding
                                    for finding in validate_profile(invalid, self.repo, True)))
        for retain_reference in (False, True):
            with self.subTest(retain_reference=retain_reference):
                invalid = copy.deepcopy(self.data)
                invalid["cpp"]["component_headers"] = {}
                if not retain_reference:
                    del invalid["cpp"]["component_headers_file"]
                self.assertIn("profile.cpp: missing or unknown keys", validate_profile(invalid, self.repo, True))

    def test_external_component_headers_resolve_parent_relative_includes_from_source_directory(self):
        source = self.repo / "src/control.cpp"
        source.parent.mkdir()
        source.write_text('#include "../include/Widget.h"\n\nint readCount()\n{\n'
                          '    return example::Widget{}.count;\n}\n', encoding="utf-8")
        self.data["cpp"]["authored_globs"].append("src/*.cpp")
        self.data["metadata"]["authored_globs"].append("src/*.cpp")
        mapping = {"src/control.cpp": "../include/Widget.h"}
        self.mapping.write_text(json.dumps(mapping), encoding="utf-8")
        self.assertEqual(mapping, load_component_headers(self.data, self.repo))
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        errors, count = check_file(source, self.repo, self.data, formatter=FORMATTER, compiler=CLANG)
        self.assertEqual([], errors)
        self.assertGreater(count, 0)
        self.mapping.write_text(json.dumps({"src/control.cpp": "../../outside.h"}), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "path escapes repository"):
            load_component_headers(self.data, self.repo)
        self.assertTrue(any("path escapes repository" in finding
                            for finding in validate_profile(self.data, self.repo, True)))

    def test_external_component_headers_reference_cannot_be_missing_or_escape_repository(self):
        self.assertEqual({}, load_component_headers(self.data, self.repo))
        for reference in (None, "", " ", [], 17, "guidelines/data/missing.json", "guidelines/data",
                          "../outside.json", "/outside.json", "C:/outside.json", "guidelines\\data\\map.json"):
            with self.subTest(reference=reference):
                invalid = copy.deepcopy(self.data)
                invalid["cpp"]["component_headers_file"] = reference
                self.assertTrue(validate_profile(invalid, self.repo, True))
                with self.assertRaises((ValueError, OSError)):
                    load_component_headers(invalid, self.repo)

    def test_external_component_headers_null_is_allowed_only_without_configured_cpp(self):
        self.data["cpp"]["component_headers_file"] = None
        self.assertTrue(validate_profile(self.data, self.repo, True))
        self.data["languages"] = ["python"]
        self.assertEqual({}, load_component_headers(self.data, self.repo))
        self.assertEqual([], validate_profile(self.data, self.repo, True))
        self.data["languages"] = []
        self.data["status"] = "unconfigured"
        self.assertEqual({}, load_component_headers(self.data, self.repo))
        self.assertEqual([], validate_profile(self.data, self.repo, False))

    def test_external_component_headers_reject_malformed_duplicate_and_non_object_json(self):
        self.mapping_control()
        invalid_json = ('{"control.cpp":',
                        '{"control.cpp": "Widget.h", "control.cpp": "Widget.h"}',
                        '[]', 'null', '"Widget.h"', '2')
        for text in invalid_json:
            with self.subTest(text=text):
                self.mapping.write_text(text, encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_component_headers(self.data, self.repo)
                self.assertTrue(validate_profile(self.data, self.repo, True))

    def test_external_component_headers_require_authored_sources_and_real_headers(self):
        source = self.mapping_control()
        outside_inventory = self.repo / "unowned.cpp"
        outside_inventory.write_text("int count = 0;\n", encoding="utf-8")
        (self.repo / "Unowned.h").write_text("#pragma once\n", encoding="utf-8")
        invalid_maps = (
            {"missing.cpp": "Widget.h"}, {"unowned.cpp": "Widget.h"},
            {"../control.cpp": "Widget.h"}, {source.as_posix(): "Widget.h"},
            {"control.cpp": "Missing.h"}, {"control.cpp": "Unowned.h"},
            {"control.cpp": "control.cpp"}, {"control.cpp": "../Widget.h"},
            {"control.cpp": "/Widget.h"}, {"control.cpp": "C:/Widget.h"},
            {"control.cpp": "include\\Widget.h"},
            *({"control.cpp": value} for value in (None, "", " ", 1, [], {}, '"Widget.h"', "<Widget.h>")),
        )
        for mapping in invalid_maps:
            with self.subTest(mapping=mapping):
                self.mapping.write_text(json.dumps(mapping), encoding="utf-8")
                with self.assertRaises(ValueError):
                    load_component_headers(self.data, self.repo)
                self.assertTrue(validate_profile(self.data, self.repo, True))
        self.mapping.write_text(json.dumps({"control.cpp": "Widget.h"}), encoding="utf-8")
        self.data["cpp"]["excluded_globs"] = ["control.cpp"]
        self.data["cpp"]["exclusion_reason"] = "Authored-boundary negative control"
        with self.assertRaises(ValueError):
            load_component_headers(self.data, self.repo)

    def test_external_component_headers_drive_real_style_cli_and_cannot_silently_disappear(self):
        source = self.mapping_control()
        (self.repo / "include/Other.h").write_text("#pragma once\n", encoding="utf-8")
        source_text = '#include "Widget.h"\n\n#include "Other.h"\n\nint readCount()\n{\n    return 1;\n}\n'
        formatted = execute([FORMATTER, f"--style=file:{self.repo / '.clang-format'}"], text=source_text)
        self.assertEqual(0, formatted.returncode, formatted.stderr)
        source.write_text(formatted.stdout, encoding="utf-8")
        valid = self.style_cli()
        self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        self.assertIn("Checked 1 selected files", valid.stdout)
        self.mapping.write_text(json.dumps({"control.cpp": "Other.h"}), encoding="utf-8")
        invalid = self.style_cli()
        self.assertEqual(1, invalid.returncode, invalid.stdout + invalid.stderr)
        self.assertIn("control.cpp: first include must be Other.h", invalid.stdout)
        self.assertNotIn("syntax check failed", invalid.stdout)
        self.assertNotIn("formatting differs", invalid.stdout)
        source.write_text(formatted.stdout.replace('"Widget.h"', '"Temporary.h"')
                          .replace('"Other.h"', '"Widget.h"').replace('"Temporary.h"', '"Other.h"'),
                          encoding="utf-8")
        valid = self.style_cli()
        self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        self.mapping.unlink()
        missing = self.style_cli()
        self.assertEqual(1, missing.returncode, missing.stdout + missing.stderr)
        self.assertIn("component_headers", missing.stdout)
        self.assertNotIn("Checked 1 selected files", missing.stdout)
        self.assertNotIn("Traceback", missing.stdout + missing.stderr)

    def test_metadata_cli_rejects_missing_external_mapping_even_for_selected_subset(self):
        valid = self.metadata_cli("--selected-only", "--file", "include/Widget.h")
        self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        self.mapping.unlink()
        invalid = self.metadata_cli("--selected-only", "--file", "include/Widget.h")
        self.assertEqual(1, invalid.returncode, invalid.stdout + invalid.stderr)
        self.assertIn("component_headers", invalid.stdout)
        self.assertNotIn("Traceback", invalid.stdout + invalid.stderr)

    def test_corpus_cli_rejects_malformed_external_mapping_after_valid_control(self):
        guidelines = self.repo / "guidelines"
        for name in REQUIRED_DOCS:
            document = guidelines / name
            document.parent.mkdir(parents=True, exist_ok=True)
            document.write_text("# Validator fixture\n", encoding="utf-8")
        (guidelines / "README.md").write_text(
            "# Fixture index\n\n" + "\n".join(f"- [{name}]({name})" for name in REQUIRED_DOCS),
            encoding="utf-8")
        (guidelines / "CORE.md").write_text(
            "# Fixture core\n\nDemerits are mandatory.\nGoogle C++ Style Guide is prohibited.\n"
            "C++20 is the minimum for C++ work.\n\n<!-- onboarding -->\n" +
            "\n".join(f"- [{name}]({name})" for name in ONBOARDING) + "\n<!-- /onboarding -->\n\n" +
            "\n".join(f"- [{name}]({name})" for name in REQUIRED_DOCS if "/" in name), encoding="utf-8")
        (guidelines / "cpp/STYLE.md").write_text(
            "# Fixture style\n\nThe house style is mandatory. Google style is prohibited.\n", encoding="utf-8")
        (guidelines / "DEMERITS.md").write_text(
            "# Fixture ledger\n\n| ID | Violation | Fixture |\n|---|---|---:|\n" +
            "\n".join(f"| D{number:02} | Fixture category | 0 |" for number in range(1, 25)) +
            "\n| **Total** | | **0** |\n", encoding="utf-8")
        self.write_profile()
        command = [sys.executable, str(REPO / "tools/lint_guidelines.py"), "guidelines",
                   "--repo-root", str(self.repo), "--instantiated", "--clang-format", FORMATTER]
        valid = execute(command, cwd=self.repo)
        self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        self.mapping.write_text("[]", encoding="utf-8")
        invalid = execute(command, cwd=self.repo)
        self.assertEqual(1, invalid.returncode, invalid.stdout + invalid.stderr)
        self.assertIn("component_headers_file must contain a JSON object", invalid.stdout)
        self.assertNotIn("Traceback", invalid.stdout + invalid.stderr)

    def test_inventory_reports_empty_patterns(self):
        self.assertEqual({self.header}, inventory(self.repo, ["include/*.h"], []))
        with self.assertRaisesRegex(ValueError, "matches no files"):
            inventory(self.repo, ["missing/*.h"], [])

    def test_effective_formatter_and_authored_include_order(self):
        self.assertEqual([], formatter_integrity(self.repo, FORMATTER))
        config = self.repo / ".clang-format"
        text = config.read_text(encoding="utf-8")
        self.assertIn("SortIncludes: Never", text)
        config.write_text(text.replace("SortIncludes: Never", "SortIncludes: CaseInsensitive"), encoding="utf-8")
        self.assertIn("formatter: SortIncludes must preserve authored order", formatter_integrity(self.repo, FORMATTER))

    def test_clang_naming_has_compilable_positive_and_negative_controls(self):
        source = "namespace example { class Widget { public: int count() const { return mCount; } private: int mCount = 0; }; }\n"
        path = self.repo / "control.cpp"
        path.write_text(source, encoding="utf-8")
        errors, count = ast_check(path, source, CLANG, [], self.repo)
        self.assertEqual([], errors)
        self.assertGreater(count, 0)
        invalid = source.replace("mCount", "bad_count")
        path.write_text(invalid, encoding="utf-8")
        errors, count = ast_check(path, invalid, CLANG, [], self.repo)
        self.assertTrue(any("bad_count: expected m plus PascalCase member" in value for value in errors), errors)
        self.assertFalse(any("syntax check failed" in value for value in errors), errors)

    def test_format_check_positive_and_negative_controls(self):
        path = self.repo / "control.cpp"
        source = "namespace example{int readCount(){return 1;}}\n"
        result = execute([FORMATTER, f"--style=file:{self.repo / '.clang-format'}"], text=source)
        self.assertEqual(0, result.returncode, result.stderr)
        path.write_text(result.stdout, encoding="utf-8")
        errors, count = check_file(path, self.repo, self.data, formatter=FORMATTER, compiler=CLANG)
        self.assertEqual([], errors)
        self.assertGreater(count, 0)
        path.write_text(source, encoding="utf-8")
        errors, _ = check_file(path, self.repo, self.data, formatter=FORMATTER, compiler=CLANG)
        self.assertEqual(["control.cpp: formatting differs from the mandatory root .clang-format"], errors)

    def test_static_members_and_local_statics_follow_their_declaration_context(self):
        source = """class Widget
{
public:
    static int sCount;
    static constexpr int kLimit = 4;
    template<class Value> inline static int sValue = 0;
    static int readCount()
    {
        static int count = 0;
        static constexpr int kLocalLimit = 2;
        int localCount = 1;
        return count + kLocalLimit + localCount;
    }
};
int Widget::sCount = 0;
static int count = 0;
"""
        path = self.repo / "statics.cpp"
        path.write_text(source, encoding="utf-8")
        errors, count = ast_check(path, source, CLANG, [], self.repo)
        self.assertEqual([], errors)
        self.assertGreater(count, 0)
        invalid = re.sub(r"\bcount\b", "bad_count", source.replace("sCount", "badCount"))
        path.write_text(invalid, encoding="utf-8")
        errors, count = ast_check(path, invalid, CLANG, [], self.repo)
        self.assertTrue(any("badCount: expected s plus PascalCase static data member" in value for value in errors), errors)
        self.assertTrue(any("bad_count: expected camelCase variable" in value for value in errors), errors)
        self.assertFalse(any("syntax check failed" in value for value in errors), errors)

    def test_clang_diagnostic_lines_use_original_lf_crlf_and_utf8_bytes(self):
        path = self.repo / "lines.cpp"
        configurations = (
            ("LF", "\n", b"", "// ASCII"),
            ("CRLF", "\r\n", b"", "// ASCII"),
            ("UTF-8 BOM", "\n", b"\xef\xbb\xbf", "// café π 😀"),
            ("UTF-8 BOM and CRLF", "\r\n", b"\xef\xbb\xbf", "// café π 😀"),
        )
        for label, newline, bom, comment in configurations:
            with self.subTest(encoding=label):
                lines = [comment] + ["// padding"] * 31 + ["int okay = 0, x;", ""]
                path.write_bytes(bom + newline.join(lines).encode("utf-8"))
                errors, count = ast_check(path, read(path), CLANG, [], self.repo)
                self.assertEqual([], errors)
                self.assertEqual(2, count)
                # The second declarator's Clang location elides its repeated line.
                # Its byte offset must still resolve to physical line 33.
                lines[32] = "int okay = 0, X;"
                path.write_bytes(bom + newline.join(lines).encode("utf-8"))
                errors, count = ast_check(path, read(path), CLANG, [], self.repo)
                self.assertEqual(["lines.cpp:33: X: expected camelCase variable"], errors)
                self.assertEqual(2, count)

    def test_component_header_first(self):
        path = self.repo / "control.cpp"
        self.assertEqual([], lexical(path, '#include "Widget.h"\n#include <vector>\n', "EXAMPLE", "Widget.h"))
        self.assertEqual(["control.cpp: first include must be Widget.h"],
                         lexical(path, '#include <vector>\n#include "Widget.h"\n', "EXAMPLE", "Widget.h"))

    def test_metadata_valid_and_canonical_order(self):
        self.assertEqual([], self.metadata(VALID_HEADER))
        invalid = VALID_HEADER.replace("  layer: api\n  summary: A validator control type.",
                                       "  summary: A validator control type.\n  layer: api")
        self.assertEqual(["Widget.h: metadata keys are not in canonical order"], self.metadata(invalid))

    def test_metadata_duplicate_and_alias_are_rejected(self):
        self.assertEqual([], self.metadata(VALID_HEADER))
        duplicate = VALID_HEADER.replace("  component: Widget", "  component: Widget\n  component: Other")
        self.assertTrue(any("duplicate key: component" in value for value in self.metadata(duplicate)))
        alias = VALID_HEADER.replace("  component: Widget", "  component: &alias Widget")
        self.assertTrue(any("aliases/anchors" in value for value in self.metadata(alias)))

    def test_metadata_hygiene_recomputes_and_rejects_unknown_counts(self):
        self.assertEqual([], self.metadata(VALID_HEADER))
        invalid = VALID_HEADER.replace("defines_total: 0", "defines_total: 1")
        self.assertTrue(any("recomputation: defines_total" in value for value in self.metadata(invalid)))
        self.assertEqual([], self.metadata(invalid + "\n#define EXAMPLE_REAL 1\n"))
        invalid = VALID_HEADER.replace("    defines_total: 0", "    includes_windows_h: false\n    defines_total: 0")
        self.assertTrue(any("recomputation: includes_windows_h" in value for value in self.metadata(invalid)))

    def test_metadata_literal_specimen_cannot_supply_missing_block(self):
        specimen = '#pragma once\ninline constexpr const char* kSpecimen = R"sample(\n/*\nEXAMPLE_META:\n  meta_version: 1\n*/\n)sample";\n'
        self.assertEqual(["Widget.h: required metadata block missing"], self.metadata(specimen))
        self.assertEqual([], self.metadata(VALID_HEADER + specimen.split("\n", 1)[1]))

    def test_metadata_selected_only_requires_an_explicit_file(self):
        valid = self.metadata_cli("--selected-only", "--file", "include/Widget.h")
        self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        invalid = self.metadata_cli("--selected-only")
        self.assertEqual(2, invalid.returncode, invalid.stdout + invalid.stderr)
        self.assertIn("--selected-only requires at least one --file", invalid.stderr)

    def test_metadata_subset_is_explicit_and_default_still_checks_all_covered_files(self):
        (self.repo / "include/Bad.h").write_text("#pragma once\n", encoding="utf-8")
        selected = self.metadata_cli("--selected-only", "--file", "include/Widget.h")
        self.assertEqual(0, selected.returncode, selected.stdout + selected.stderr)
        self.assertIn("Explicit subset only", selected.stdout)
        invalid = self.metadata_cli("--selected-only", "--file", "include/Bad.h")
        self.assertEqual(1, invalid.returncode, invalid.stdout + invalid.stderr)
        self.assertIn("Bad.h: required metadata block missing", invalid.stdout)
        for arguments in ((), ("--file", "include/Widget.h")):
            with self.subTest(arguments=arguments):
                complete = self.metadata_cli(*arguments)
                self.assertEqual(1, complete.returncode, complete.stdout + complete.stderr)
                self.assertIn("Bad.h: required metadata block missing", complete.stdout)
                self.assertIn("checked 2 selected files", complete.stdout)

    def test_metadata_outside_coverage_remains_optional_but_existing_blocks_are_checked(self):
        outside = self.repo / "Outside.cpp"
        outside.write_text("int main() {}\n", encoding="utf-8")
        for arguments in (("--file", "Outside.cpp"), ("--selected-only", "--file", "Outside.cpp")):
            valid = self.metadata_cli(*arguments)
            self.assertEqual(0, valid.returncode, valid.stdout + valid.stderr)
        outside.write_text("/*\nFATP_META:\n  meta_version: 1\n*/\n\nint main() {}\n", encoding="utf-8")
        for arguments in (("--file", "Outside.cpp"), ("--selected-only", "--file", "Outside.cpp")):
            invalid = self.metadata_cli(*arguments)
            self.assertEqual(1, invalid.returncode, invalid.stdout + invalid.stderr)
            self.assertIn("metadata sentinel must be EXAMPLE_META", invalid.stdout)

    def test_failed_file_does_not_truncate_inventory_or_become_a_pass(self):
        first, second = self.repo / "a.cpp", self.repo / "b.cpp"
        with patch("check_style.check_file", side_effect=[ValueError("tool could not run: timed out"), ([], 3)]) as check:
            errors, count = check_files([second, first], self.repo, self.data)
        self.assertEqual(2, check.call_count)
        self.assertEqual(["a.cpp: tool could not run: timed out"], errors)
        self.assertEqual(3, count)
        with patch("check_style.check_file", return_value=([], 3)):
            self.assertEqual(([], 6), check_files([first, second], self.repo, self.data))

    def test_actual_process_timeout_is_a_failed_prerequisite(self):
        valid = execute([sys.executable, "-c", "print('ready')"])
        self.assertEqual(0, valid.returncode, valid.stderr)
        self.assertEqual("ready", valid.stdout.strip())
        with self.assertRaisesRegex(ValueError, "tool could not run"):
            execute([sys.executable, "-c", "import time; time.sleep(2)"], timeout=0.01)

    def test_ledger_retains_counts_when_assistant_columns_are_reordered(self):
        original = ledger_fixture()
        reordered = []
        for line in original.splitlines():
            cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
            cells[2], cells[3] = cells[3], cells[2]
            reordered.append("| " + " | ".join(cells) + " |")
        self.assertEqual([], ledger("\n".join(reordered), original))

    def test_ledger_rejects_removed_categories_and_assistant_attribution(self):
        original = ledger_fixture()
        missing_category = "\n".join(line for line in original.splitlines()
                                     if not line.startswith("| D01 |"))
        self.assertTrue(any("category" in finding for finding in ledger(missing_category, original)))
        renamed = original.replace("| ChatGPT |", "| Replacement |")
        self.assertIn("DEMERITS: original assistant column removed: ChatGPT", ledger(renamed, original))

    def test_reconciled_total_does_not_hide_an_undirected_decrease(self):
        baseline_text = ledger_fixture()
        self.assertEqual([], ledger(baseline_text, baseline_text))
        assistants, counts = ledger_values(baseline_text)
        chatgpt = assistants.index("ChatGPT") + 2
        lines = baseline_text.splitlines()
        for index, line in enumerate(lines):
            if line.startswith("| D08 |") or line.startswith("| **Total** |"):
                cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
                value = int(cells[chatgpt].strip("*")) - 1
                cells[chatgpt] = f"**{value}**" if cells[0] == "**Total**" else str(value)
                lines[index] = "| " + " | ".join(cells) + " |"
        reduced = "\n".join(lines)
        self.assertIn("DEMERITS: count decreased without matching directed correction: D08/ChatGPT",
                      ledger(reduced, baseline_text))


if __name__ == "__main__":
    unittest.main()
