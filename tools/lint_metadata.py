# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/lint_metadata.py
#   layer: Infrastructure
#   summary: Validates adopted comment-only metadata without rewriting source.
#   api_stability: in_work

"""Validate adopted comment-only file metadata. No source files are rewritten."""
from __future__ import annotations

import argparse
import io
import re
import tokenize
from pathlib import Path

import yaml

from guideline_lib import (
    CPP_HEADER_SUFFIXES, CPP_SOURCE_SUFFIXES, CPP_SUFFIXES, console, inventory, profile,
    read, repo_path, report, roots, validate_profile, yaml_value,
)
from check_style import LEXEME, without_comments, without_literals

KEY_ORDER = ["meta_version", "component", "file_role", "path", "namespace", "layer", "summary",
             "api_stability", "related", "hygiene", "generated"]
REQUIRED = {"meta_version", "component", "file_role", "path", "layer", "summary"}
MARKER = re.compile(r"^\s*([A-Z][A-Z0-9_]*_META):\s*$", re.M)
# Used only for .sh/.ps1. Python uses tokenize; CMake uses its own argument scanner.
# A quote inside a # comment is comment text, not a string that swallows following comments.
SCRIPT_LEXEME = re.compile(
    r"'''.*?'''|\"\"\".*?\"\"\"|'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"|#[^\n]*", re.S)
PYTHON_STRINGISH = {tokenize.STRING}
for _name in ("FSTRING_START", "FSTRING_MIDDLE", "FSTRING_END",
              "TSTRING_START", "TSTRING_MIDDLE", "TSTRING_END"):
    _value = getattr(tokenize, _name, None)
    if _value is not None:
        PYTHON_STRINGISH.add(_value)


def _line_offsets(text: str) -> list[int]:
    offsets = [0]
    start = 0
    for line in text.splitlines(keepends=True):
        start += len(line)
        offsets.append(start)
    return offsets


def _loc_offset(offsets: list[int], loc, length: int) -> int:
    line, column = loc
    if line < 1:
        return 0
    if line > len(offsets):
        return length
    return min(length, offsets[line - 1] + column)


def python_literal_spans(text: str) -> list[tuple[int, int]]:
    offsets = _line_offsets(text)
    spans = []
    try:
        for token in tokenize.generate_tokens(io.StringIO(text).readline):
            if token.type in PYTHON_STRINGISH:
                spans.append((_loc_offset(offsets, token.start, len(text)),
                              _loc_offset(offsets, token.end, len(text))))
    except tokenize.TokenError as exc:
        loc = exc.args[1] if len(exc.args) > 1 and isinstance(exc.args[1], tuple) else None
        if loc:
            spans.append((_loc_offset(offsets, loc, len(text)), len(text)))
    except IndentationError:
        pass
    return spans


def _cmake_bracket(text: str, open_at: int):
    # Caller has already seen '[' at open_at. Bracket form is [={0,}[.
    equals = 0
    i = open_at + 1
    while i < len(text) and text[i] == "=":
        equals += 1
        i += 1
    if i >= len(text) or text[i] != "[":
        return None
    closer = "]" + ("=" * equals) + "]"
    end = text.find(closer, i + 1)
    if end < 0:
        return open_at, len(text)
    return open_at, end + len(closer)


def cmake_spans(text: str) -> tuple[list[tuple[int, int]], list[tuple[int, int]]]:
    literals, comments = [], []
    i, n = 0, len(text)
    while i < n:
        char = text[i]
        if char == "#":
            if i + 1 < n and text[i + 1] == "[":
                region = _cmake_bracket(text, i + 1)
                if region is not None:
                    comments.append((i, region[1]))
                    i = region[1]
                    continue
            while i < n and text[i] != "\n":
                i += 1
            continue
        if char == '"':
            start = i
            i += 1
            while i < n:
                if text[i] == "\\" and i + 1 < n:
                    i += 2
                    continue
                if text[i] == '"':
                    i += 1
                    break
                i += 1
            literals.append((start, i))
            continue
        if char == "[":
            region = _cmake_bracket(text, i)
            if region is not None:
                literals.append(region)
                i = region[1]
                continue
        i += 1
    return literals, comments


def generic_literal_spans(text: str) -> list[tuple[int, int]]:
    return [m.span() for m in SCRIPT_LEXEME.finditer(text) if not m.group().startswith("#")]


def is_cmake_path(path: Path) -> bool:
    return path.suffix.lower() == ".cmake" or path.name == "CMakeLists.txt"


def literal_spans(text: str, path: Path) -> list[tuple[int, int]]:
    suffix = path.suffix.lower()
    if suffix == ".py":
        return python_literal_spans(text)
    if is_cmake_path(path):
        return cmake_spans(text)[0]
    return generic_literal_spans(text)


def extract_excluded_spans(text: str, path: Path) -> list[tuple[int, int]]:
    if is_cmake_path(path):
        literals, comments = cmake_spans(text)
        return literals + comments
    return literal_spans(text, path)


def mask_spans(text: str, spans: list[tuple[int, int]]) -> str:
    if not spans:
        return text
    chars = list(text)
    length = len(chars)
    for start, end in spans:
        for index in range(max(0, start), min(end, length)):
            if chars[index] != "\n":
                chars[index] = " "
    return "".join(chars)


def in_span(pos: int, spans: list[tuple[int, int]]) -> bool:
    return any(start <= pos < end for start, end in spans)


def extract(text: str, path: Path):
    blocks = []
    sentinel_text = text
    if path.suffix.lower() in CPP_SUFFIXES:
        sentinel_text = without_literals(text)
        for match in LEXEME.finditer(text):
            if not match.group().startswith("/*"):
                continue
            payload = match.group()[2:-2].strip()
            if MARKER.match(payload):
                closing_line = text[text.rfind("\n", 0, match.end() - 2) + 1:match.end()]
                if closing_line.strip() != "*/":
                    raise ValueError("metadata terminator must be alone; possible embedded */")
                blocks.append((match.start(), match.end(), payload))
    elif path.suffix.lower() in (".py", ".sh", ".ps1", ".cmake") or path.name == "CMakeLists.txt":
        sentinel_text = mask_spans(text, literal_spans(text, path))
        excluded = extract_excluded_spans(text, path)
        lines = text.splitlines(keepends=True)
        offset, i = 0, 0
        while i < len(lines):
            if (not in_span(offset, excluded)
                    and re.match(r"^\s*# [A-Z][A-Z0-9_]*_META:\s*$", lines[i].rstrip())):
                start, payload = offset, []
                while (i < len(lines) and not in_span(offset, excluded)
                       and re.match(r"^\s*#(?: |$)", lines[i])):
                    payload.append(re.sub(r"^\s*# ?", "", lines[i]).rstrip("\r\n"))
                    offset += len(lines[i])
                    i += 1
                # End before the last line's newline; a block that ends the file
                # without one has no following content to separate.
                end = offset - 1 if text[offset - 1:offset] == "\n" else offset
                blocks.append((start, end, "\n".join(payload)))
            else:
                offset += len(lines[i])
                i += 1
    else:
        raise ValueError(f"unsupported metadata comment wrapper: {path.name}")
    if len(blocks) > 1:
        raise ValueError("multiple metadata blocks")
    if not blocks:
        if re.search(r"\b[A-Z][A-Z0-9_]*_META:", sentinel_text):
            raise ValueError("metadata sentinel is not in a supported comment block")
        return None
    return blocks[0]


def string_list(value):
    return isinstance(value, list) and bool(value) and all(isinstance(v, str) and v.strip() for v in value)


def check_metadata(path: Path, repo: Path, config: dict, prefix: str, *, required=True) -> list[str]:
    try:
        text = read(path)
        block = extract(text, path)
        if block is None:
            return [f"{path.name}: required metadata block missing"] if required else []
        start, end, payload = block
        if "*/" in payload:
            raise ValueError("comment terminator inside metadata")
        value = yaml_value(payload)
        expected_sentinel = prefix + "_META"
        if not isinstance(value, dict) or list(value) != [expected_sentinel]:
            raise ValueError(f"metadata sentinel must be {expected_sentinel}")
        data = value[expected_sentinel]
        if not isinstance(data, dict) or not REQUIRED <= set(data) or set(data) - set(KEY_ORDER):
            raise ValueError("missing required or unknown schema keys")
        if list(data) != [k for k in KEY_ORDER if k in data]:
            raise ValueError("metadata keys are not in canonical order")
        if type(data["meta_version"]) is not int or data["meta_version"] != 1:
            raise ValueError("meta_version must be integer 1")
        for key in ("file_role", "path", "layer", "summary"):
            if not isinstance(data[key], str) or not data[key].strip():
                raise ValueError(f"{key} must be a nonempty string")
        for key in ("component", "namespace"):
            if key in data and not ((isinstance(data[key], str) and data[key].strip()) or string_list(data[key])):
                raise ValueError(f"{key} must be a string or nonempty string list")
        if data["file_role"] not in config["file_roles"] or data["layer"] not in config["layers"]:
            raise ValueError("file_role/layer not in project's closed lists")
        actual = path.relative_to(repo).as_posix()
        if data["path"] != actual or repo_path(repo, data["path"]) != path.resolve():
            raise ValueError(f"metadata path must match actual path: {actual}")
        if data["file_role"].endswith("_header") and path.suffix.lower() not in CPP_HEADER_SUFFIXES:
            raise ValueError("header role on a non-header file")
        if "api_stability" in data and data["api_stability"] not in ("in_work", "experimental", "candidate", "stable"):
            raise ValueError("invalid api_stability")
        before = text[:start]
        if path.suffix.lower() in CPP_HEADER_SUFFIXES:
            if before.strip() != "#pragma once":
                raise ValueError("header metadata must immediately follow #pragma once")
        elif path.suffix.lower() in CPP_SOURCE_SUFFIXES:
            if without_comments(before).strip():
                raise ValueError("source metadata must precede includes and code")
        else:
            allowed = r"\s*(?:#![^\n]*\n)?(?:#.*coding[:=][^\n]*\n)?"
            if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
                allowed = r"\s*(?:cmake_minimum_required\([^)]*\)\s*)?"
            if not re.fullmatch(allowed, before):
                raise ValueError("script/build metadata must precede executable statements")
        after = text[end:]
        if after.strip() and not after.startswith("\n\n"):
            raise ValueError("metadata needs a blank line before following content")
        if any(len(line) > 120 for line in payload.splitlines()):
            raise ValueError("metadata line exceeds 120 columns")
        related = data.get("related", {})
        if not isinstance(related, dict):
            raise ValueError("related must be a map")
        for key, values in related.items():
            if key.endswith("_search"):
                if not isinstance(values, str) or not values.strip() or data.get("api_stability") != "in_work":
                    raise ValueError("search hints require in_work status and plain text")
            else:
                if not string_list(values):
                    raise ValueError("related paths must be nonempty lists")
                for target in values:
                    if not repo_path(repo, target).is_file():
                        raise ValueError(f"related target missing: {target}")
        hygiene = data.get("hygiene", {})
        if not isinstance(hygiene, dict):
            raise ValueError("hygiene must be a map")
        code = without_comments(text, literals=True)
        defined = re.findall(r"^\s*#\s*define\s+(\w+)", code, re.M)
        undefined = re.findall(r"^\s*#\s*undef\s+(\w+)", code, re.M)
        computed = {
            "pragma_once": bool(re.search(r"^\s*#pragma\s+once\b", code, re.M)),
            "include_guard": bool(re.search(r"^\s*#ifndef\s+(\w+)\s*\n\s*#define\s+\1\b", code, re.M)),
            "defines_total": len(defined),
            "defines_unprefixed": sum(not name.startswith(prefix + "_") and name not in undefined for name in defined),
            "undefs_total": len(undefined),
        }
        for key, count in hygiene.items():
            if key == "includes_platform_header":
                raise ValueError("platform-header classification needs a project-specific checker; omit uncomputed values")
            if key not in computed or type(count) is not type(computed[key]) or count != computed[key]:
                raise ValueError(f"hygiene value not confirmed by recomputation: {key}")
        if "generated" in data:
            generated = data["generated"]
            if not isinstance(generated, dict) or set(generated) != {"by", "mode"} or generated["mode"] != "autogen" or not generated["by"]:
                raise ValueError("generated requires by and mode=autogen")
        return []
    except (ValueError, TypeError, KeyError, OSError, yaml.YAMLError) as exc:
        return [f"{path.name}: {exc}"]


def main():
    console()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--guidelines", default="guidelines")
    parser.add_argument("--file", action="append", default=[])
    parser.add_argument("--selected-only", action="store_true",
                        help="check only explicit --file paths; does not replace the full inventory gate")
    args = parser.parse_args()
    if args.selected_only and not args.file:
        parser.error("--selected-only requires at least one --file")
    try:
        g, repo = roots(args.guidelines, args.repo_root)
        data = profile(g)
        errors = validate_profile(data, repo, True)
        if errors:
            return report(errors, "metadata")
        config, prefix = data["metadata"], data["identity"]["macro_prefix"]
        selected = {repo_path(repo, value) for value in args.file}
        covered = inventory(repo, config["authored_globs"], config["excluded_globs"]) if config["enabled"] else set()
        if not args.selected_only:
            selected |= covered
        for path in sorted(selected):
            errors.extend(check_metadata(path, repo, config, prefix or "", required=path in covered))
        print(f"Metadata adoption: {config['enabled']}; checked {len(selected)} selected files.")
        if args.selected_only:
            print("Explicit subset only; the complete metadata inventory was not checked.")
        print("Scope: schema, placement, paths and known hygiene counts; taxonomy meaning and generator provenance need review.")
        return report(errors, "metadata")
    except (ValueError, TypeError, KeyError, OSError) as exc:
        return report([str(exc)], "metadata")


if __name__ == "__main__":
    raise SystemExit(main())
