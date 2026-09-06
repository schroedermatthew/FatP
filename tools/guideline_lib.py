# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/guideline_lib.py
#   layer: Infrastructure
#   summary: Shared parsing and validation utilities for the adopted guideline tools.
#   api_stability: in_work

"""Shared read-only validation utilities. See requirements.txt for dependencies."""
from __future__ import annotations

import json
import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

try:
    import yaml
    from markdown_it import MarkdownIt
except ImportError as exc:
    raise SystemExit("Missing validator dependency. Run: python -m pip install -r tools/requirements.txt") from exc

REQUIRED_DOCS = (
    "AI_GUIDELINES_CRASH_COURSE.md", "PROJECT_PHILOSOPHY.md",
    "README.md", "CORE.md", "DEMERITS.md", "PROJECT_PROFILE.md", "ARCHITECTURE.md",
    "CURRENT_VERIFICATION.md", "LESSONS.md",
    "modules/ENGINEERING.md", "modules/TESTING.md", "modules/WORKFLOW.md",
    "modules/REVIEW_AND_DELIVERY.md", "modules/DOCUMENTATION.md", "modules/GOVERNANCE.md",
    "modules/HANDOFFS.md", "modules/TEACHING.md", "modules/BENCHMARKING.md",
    "modules/METADATA.md", "modules/PEER_BRIDGE.md", "modules/FRESH_CONTEXT_REVIEW.md",
    "cpp/README.md", "cpp/STYLE.md", "cpp/AUTHORING.md", "cpp/HEADERS_AND_LINKAGE.md",
    "cpp/TESTING.md", "cpp/BUILD_AND_CI.md", "cpp/DOCUMENTATION.md",
)
ONBOARDING = (
    "AI_GUIDELINES_CRASH_COURSE.md", "DEMERITS.md", "CORE.md",
    "PROJECT_PHILOSOPHY.md", "PROJECT_PROFILE.md",
)
CPP_HEADER_SUFFIXES = frozenset({".h", ".hpp", ".hh"})
CPP_SOURCE_SUFFIXES = frozenset({".cpp", ".cc", ".cxx", ".cppm", ".ixx"})
CPP_SUFFIXES = CPP_HEADER_SUFFIXES | CPP_SOURCE_SUFFIXES
PROFILE_FIELDS = {
    "schema_version": None, "status": None, "languages": None,
    "identity": {"name", "purpose", "owner", "namespace", "macro_prefix"},
    "cpp": {"standard", "layout", "harness", "exception_policy", "toolchains",
            "authored_globs", "excluded_globs", "exclusion_reason", "component_headers_file"},
    "build": {"ci", "support_contract", "commands"},
    "compatibility": {"stage", "policy"},
    "metadata": {"enabled", "reason", "authored_globs", "excluded_globs", "exclusion_reason",
                 "file_roles", "layers"},
    "peer_routes": None, "local_rules": None,
}


def console() -> None:
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="backslashreplace")


def report(findings: list[str], label: str) -> int:
    for finding in findings:
        print(finding)
    print(f"{label}: {'FAIL' if findings else 'PASS'}")
    return 1 if findings else 0


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig")


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate key: {key}")
        result[key] = value
    return result


def json_value(text: str):
    return json.loads(text, object_pairs_hook=unique_pairs)


class UniqueLoader(yaml.SafeLoader):
    # YAML 1.2 booleans: formatter enum values such as Yes are strings.
    # Copy resolver lists so this does not change PyYAML's global SafeLoader.
    yaml_implicit_resolvers = {
        key: [(tag, pattern) for tag, pattern in rules
              if tag != "tag:yaml.org,2002:bool"]
        for key, rules in yaml.SafeLoader.yaml_implicit_resolvers.items()
    }


UniqueLoader.add_implicit_resolver(
    "tag:yaml.org,2002:bool", re.compile(r"^(?:true|True|TRUE|false|False|FALSE)$"), list("tTfF"))


def unique_mapping(loader, node, deep=False):
    pairs = [(loader.construct_object(k, deep=deep), loader.construct_object(v, deep=deep))
             for k, v in node.value]
    return unique_pairs(pairs)


UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, unique_mapping)


def yaml_value(text: str):
    if any(isinstance(token, (yaml.tokens.AliasToken, yaml.tokens.AnchorToken))
           for token in yaml.scan(text)):
        raise ValueError("YAML aliases/anchors are not supported")
    return yaml.load(text, Loader=UniqueLoader)


def profile(guidelines: Path) -> dict:
    text = read(guidelines / "PROJECT_PROFILE.md")
    sections = re.findall(r"<!-- project-profile -->\s*\x60{3}json\s*\n(.*?)\n\x60{3}\s*<!-- /project-profile -->",
                          text, re.S)
    if len(sections) != 1 or text.count("<!-- project-profile -->") != 1:
        raise ValueError("PROJECT_PROFILE.md: exactly one canonical project-profile JSON block required")
    data = json_value(sections[0])
    if not isinstance(data, dict):
        raise ValueError("profile must be an object")
    return data


def roots(guidelines: str, repo_root: str | None) -> tuple[Path, Path]:
    repo = Path(repo_root).resolve() if repo_root else None
    given = Path(guidelines)
    g = (repo / given).resolve() if repo and not given.is_absolute() else given.resolve()
    if repo is None:
        # These are the two documented layouts. Never guess from the first formatter found.
        if g.name != "guidelines":
            raise ValueError("nonstandard guidelines directory requires --repo-root")
        repo = g.parent.parent if g.parent.name == "docs" else g.parent
    if not g.is_dir() or not g.is_relative_to(repo):
        raise ValueError("guidelines must be an existing directory inside --repo-root")
    return g, repo


def repo_path(repo: Path, value: str) -> Path:
    if not isinstance(value, str) or not value or "\\" in value:
        raise ValueError(f"expected forward-slash repository-relative path: {value!r}")
    if Path(value).is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise ValueError(f"absolute path is not portable: {value}")
    result = (repo / value).resolve()
    if not result.is_relative_to(repo):
        raise ValueError(f"path escapes repository: {value}")
    return result


def inventory(repo: Path, patterns: list, excluded: list, *, allow_empty=False) -> set[Path]:
    if not isinstance(patterns, list) or not isinstance(excluded, list):
        raise ValueError("inventory patterns/exclusions must be lists")
    files: set[Path] = set()
    for pattern in patterns:
        repo_path(repo, pattern)
        found = {p.resolve() for p in repo.glob(pattern) if p.is_file()}
        if any(not p.is_relative_to(repo) for p in found):
            raise ValueError(f"inventory follows a path outside the repository: {pattern}")
        if not found and not allow_empty:
            raise ValueError(f"inventory pattern matches no files: {pattern}")
        files |= found
    for pattern in excluded:
        repo_path(repo, pattern)
        files -= {p.resolve() for p in repo.glob(pattern) if p.is_file()}
    if patterns and not files and not allow_empty:
        raise ValueError("authored inventory is empty after exclusions")
    return files


def load_component_headers(data: dict, repo: Path) -> dict[str, str]:
    """Load the one referenced mapping and validate its owned source/header paths."""
    cpp = data["cpp"]
    reference = cpp["component_headers_file"]
    configured_cpp = data["status"] == "instantiated" and "cpp" in data["languages"]
    if reference is None:
        if configured_cpp:
            raise ValueError("component_headers_file is required for configured C++")
        return {}
    if not isinstance(reference, str) or not reference.strip() or reference != reference.strip():
        raise ValueError("component_headers_file must be a nonblank repository-relative JSON path")
    mapping_path = repo_path(repo, reference)
    if mapping_path.suffix != ".json" or not mapping_path.is_file():
        raise ValueError(f"component_headers_file must name an existing JSON file: {reference}")
    mappings = json_value(read(mapping_path))
    if not isinstance(mappings, dict):
        raise ValueError("component_headers_file must contain a JSON object")
    authored = inventory(repo, cpp["authored_globs"], cpp["excluded_globs"],
                         allow_empty=not configured_cpp)
    headers = {p for p in authored if p.suffix.lower() in CPP_HEADER_SUFFIXES}
    for source_name, header in mappings.items():
        source = repo_path(repo, source_name)
        if (source not in authored or source.suffix.lower() not in CPP_SOURCE_SUFFIXES
                or source.relative_to(repo).as_posix() != source_name):
            raise ValueError(f"component_headers_file: source is not a canonical authored C++ source: {source_name}")
        if (not isinstance(header, str) or not header.strip() or header != header.strip()
                or any(c in header for c in '\\"<>\r\n')):
            raise ValueError(f"component_headers_file: invalid header spelling for {source_name}")
        if Path(header).is_absolute() or re.match(r"^[A-Za-z]:", header):
            raise ValueError(f"component_headers_file: absolute header spelling is not portable: {header}")
        sibling = repo_path(repo, f"{source.parent.relative_to(repo).as_posix()}/{header}")
        if sibling not in headers and not any(
                candidate.relative_to(repo).as_posix().endswith("/" + header)
                or candidate.relative_to(repo).as_posix() == header for candidate in headers):
            raise ValueError(f"component_headers_file: header not found in authored inventory: {header}")
    return mappings


def validate_profile(data: dict, repo: Path, instantiated: bool) -> list[str]:
    errors: list[str] = []
    if set(data) != set(PROFILE_FIELDS):
        return ["profile: missing or unknown top-level keys"]
    for key, expected in PROFILE_FIELDS.items():
        if expected is not None and (not isinstance(data[key], dict) or set(data[key]) != expected):
            errors.append(f"profile.{key}: missing or unknown keys")
    if errors:
        return errors
    if type(data["schema_version"]) is not int or data["schema_version"] != 2:
        errors.append("profile: schema_version must be 2")
    if data["status"] not in ("unconfigured", "instantiated"):
        errors.append("profile: invalid status")
    if instantiated and data["status"] != "instantiated":
        errors.append("profile: --instantiated requires status=instantiated")
    if not isinstance(data["languages"], list) or any(not isinstance(x, str) or not x for x in data["languages"]):
        errors.append("profile.languages: expected nonempty language names")
        return errors
    if any(x != x.lower() or any(c.isspace() for c in x) or x in ("c++", "cxx", "cplusplus")
           for x in data["languages"]):
        errors.append("profile.languages: use lowercase names without whitespace; C++ must be named cpp")
    cpp = data["cpp"]
    if type(cpp["standard"]) is not int or cpp["standard"] not in (20, 23, 26):
        errors.append("profile.cpp.standard: C++20 minimum; supported modes are 20, 23, 26")
    for obj, fields in ((cpp, ("authored_globs", "excluded_globs", "toolchains")),
                        (data["metadata"], ("authored_globs", "excluded_globs", "file_roles", "layers"))):
        for key in fields:
            if not isinstance(obj[key], list) or any(not isinstance(x, str) or not x for x in obj[key]):
                errors.append(f"profile.{key}: expected list of nonempty strings")
    if cpp["component_headers_file"] is not None and not isinstance(cpp["component_headers_file"], str):
        errors.append("profile.cpp.component_headers_file: expected JSON path or unresolved null")
    meta = data["metadata"]
    if meta["enabled"] is not None and type(meta["enabled"]) is not bool:
        errors.append("profile.metadata.enabled: expected true, false or unresolved null")
    for key in ("peer_routes", "local_rules"):
        if not isinstance(data[key], list):
            errors.append(f"profile.{key}: expected list")
    if not isinstance(data["build"]["commands"], list):
        errors.append("profile.build.commands: expected list")
    if errors:
        return errors
    for route in data["peer_routes"]:
        if (not isinstance(route, dict) or set(route) != {"name", "tool", "read_only", "timeout_seconds"}
                or route.get("read_only") is not True or type(route.get("timeout_seconds")) is not int
                or route.get("timeout_seconds", 0) <= 0 or not route.get("name") or not route.get("tool")):
            errors.append("profile.peer_routes: invalid or non-read-only route")
    for rule in data["local_rules"]:
        try:
            if not isinstance(rule, dict) or set(rule) != {"path", "scope", "owner"} or not all(rule.values()):
                raise ValueError("expected path, scope and owner")
            if not repo_path(repo, rule["path"]).is_file():
                raise ValueError("local rule does not exist")
        except (ValueError, TypeError) as exc:
            errors.append(f"profile.local_rules: {exc}")
    command_fields = ("name", "command", "cwd", "property", "when", "evidence")
    for index, command in enumerate(data["build"]["commands"]):
        if not isinstance(command, dict) or set(command) != set(command_fields):
            errors.append("profile.build.commands: each record needs name/command/cwd/property/when/evidence")
            continue
        for field in command_fields:
            if not isinstance(command[field], str) or not command[field].strip():
                errors.append(f"profile.build.commands[{index}].{field}: expected a nonblank string")
    try:
        load_component_headers(data, repo)
    except (ValueError, OSError) as exc:
        errors.append(f"profile.cpp: {exc}")
    if not instantiated:
        return errors
    for key in ("name", "purpose", "owner"):
        if not isinstance(data["identity"][key], str) or not data["identity"][key].strip():
            errors.append(f"profile.identity.{key}: unresolved")
    if not data["languages"]:
        errors.append("profile.languages: unresolved")
    if data["build"]["ci"] not in ("live", "planned", "none") or not data["build"]["support_contract"]:
        errors.append("profile.build: CI/support contract unresolved")
    if not data["build"]["commands"]:
        errors.append("profile.build.commands: no acceptance procedure")
    compat = data["compatibility"]
    if compat["stage"] not in ("pre-release", "released") or not compat["policy"]:
        errors.append("profile.compatibility: stage/policy unresolved")
    if compat["stage"] == "pre-release" and compat["policy"] != "no-shims":
        errors.append("profile.compatibility: pre-release requires no-shims")
    if type(meta["enabled"]) is not bool or not meta["reason"]:
        errors.append("profile.metadata: explicit adoption decision and reason required")
    if "cpp" in data["languages"]:
        if cpp["layout"] not in ("header-only", "compiled", "mixed"):
            errors.append("profile.cpp.layout: unresolved")
        for key in ("harness", "exception_policy", "toolchains", "authored_globs"):
            if not cpp[key]:
                errors.append(f"profile.cpp.{key}: unresolved")
        identity = data["identity"]
        if not isinstance(identity["namespace"], str) or not re.fullmatch(r"[a-z][a-z0-9_]*(?:::[a-z][a-z0-9_]*)*", identity["namespace"]):
            errors.append("profile.identity.namespace: invalid")
        if not isinstance(identity["macro_prefix"], str) or not re.fullmatch(r"[A-Z][A-Z0-9_]*", identity["macro_prefix"]):
            errors.append("profile.identity.macro_prefix: invalid")
        try:
            inventory(repo, cpp["authored_globs"], cpp["excluded_globs"])
            if cpp["excluded_globs"] and not cpp["exclusion_reason"]:
                errors.append("profile.cpp: excluded code requires a reason")
        except (ValueError, OSError) as exc:
            errors.append(f"profile.cpp: {exc}")
    if meta["enabled"]:
        if not data["identity"]["macro_prefix"] or not meta["file_roles"] or not meta["layers"] or not meta["authored_globs"]:
            errors.append("profile.metadata: enabled metadata needs prefix, roles, layers and coverage")
        if meta["excluded_globs"] and not meta["exclusion_reason"]:
            errors.append("profile.metadata: exclusions require a reason")
        try:
            covered = inventory(repo, meta["authored_globs"], meta["excluded_globs"])
            if "cpp" in data["languages"]:
                authored = inventory(repo, cpp["authored_globs"], cpp["excluded_globs"])
                explicitly_excluded = set()
                for pattern in meta["excluded_globs"]:
                    explicitly_excluded |= {p.resolve() for p in repo.glob(pattern) if p.is_file()}
                if authored - covered - explicitly_excluded:
                    errors.append("profile.metadata: authored C++ files fall outside metadata coverage")
        except (ValueError, OSError) as exc:
            errors.append(f"profile.metadata: {exc}")
    return errors


def execute(args: list[str], *, text: str | None = None, cwd: Path | None = None, timeout=60):
    try:
        return subprocess.run(args, input=text, text=True, encoding="utf-8", errors="replace",
                              capture_output=True, cwd=cwd, timeout=timeout)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ValueError(f"tool could not run: {args[0]}: {exc}") from exc


def formatter_integrity(repo: Path, executable: str) -> list[str]:
    config = repo / ".clang-format"
    if not config.is_file():
        return ["formatter: missing repository-root .clang-format"]
    try:
        raw = yaml_value(read(config))
        if not isinstance(raw, dict) or raw.get("BasedOnStyle") != "LLVM":
            return ["formatter: BasedOnStyle must be LLVM; Google is prohibited"]
        result = execute([executable, f"--style=file:{config}", "--dump-config"], cwd=repo)
        if result.returncode:
            return ["formatter: invalid/unsupported configuration: " + result.stderr.strip()]
        effective = yaml_value(result.stdout)
        expected = {"IndentWidth": 4, "UseTab": "Never", "BreakBeforeBraces": "Allman",
                    "ColumnLimit": 120, "PointerAlignment": "Left", "NamespaceIndentation": "None",
                    "InsertBraces": True, "IncludeBlocks": "Preserve",
                    "AllowShortBlocksOnASingleLine": "Never",
                    "AllowShortFunctionsOnASingleLine": "None",
                    "AllowShortIfStatementsOnASingleLine": "Never",
                    "AllowShortLoopsOnASingleLine": False,
                    "AllowShortLambdasOnASingleLine": "Empty",
                    "BinPackArguments": False, "BinPackParameters": "OnePerLine",
                    "BreakConstructorInitializers": "BeforeComma",
                    "PackConstructorInitializers": "Never",
                    "BreakTemplateDeclarations": "Yes", "AccessModifierOffset": -4}
        errors = [f"formatter: {key} must be {value!r}" for key, value in expected.items()
                  if effective.get(key) != value]
        sorting = effective.get("SortIncludes")
        if sorting not in (False, "Never") and not (isinstance(sorting, dict) and sorting.get("Enabled") is False):
            errors.append("formatter: SortIncludes must preserve authored order")
        if effective.get("ReferenceAlignment") not in ("Left", "Pointer"):
            errors.append("formatter: references must align left")
        return errors
    except (ValueError, yaml.YAMLError, OSError) as exc:
        return [f"formatter: {exc}"]


MARKDOWN = MarkdownIt("commonmark").enable(["table", "strikethrough"])


def links(text: str):
    # Parse rendered inlines, excluding fenced/indented/inline code and resolving
    # reference destinations. The line identifies the containing inline block.
    for token in MARKDOWN.parse(text):
        if token.type != "inline":
            continue
        line_number = token.map[0] + 1 if token.map else 1
        for child in token.children or []:
            attribute = {"link_open": "href", "image": "src"}.get(child.type)
            if attribute:
                destination = child.attrGet(attribute)
                if destination is not None:
                    yield line_number, destination


def link_target(doc: Path, destination: str, repo: Path) -> tuple[Path | None, str]:
    if urlsplit(destination).scheme or destination.startswith("//"):
        return None, ""
    raw, _, anchor = destination.partition("#")
    target = (doc.parent / unquote(raw)).resolve() if raw else doc
    if not target.is_relative_to(repo):
        raise ValueError(f"link escapes repository: {destination}")
    return target, unquote(anchor)


def plain(children) -> str:
    return "".join(child.content if child.type in ("text", "code_inline")
                   else " " if child.type in ("softbreak", "hardbreak")
                   else plain(child.children or []) if child.type == "image"
                   else "" for child in children)


def prose(text: str) -> str:
    # Keep policy prose separate from code and image alt text. A boundary prevents
    # omitted specimens from joining surrounding words into a required statement.
    # Heading rendering still uses plain(), including its inline code spellings.
    segments = []
    for token in MARKDOWN.parse(text):
        if token.type != "inline":
            continue
        runs = [""]
        for child in token.children or []:
            if child.type in ("code_inline", "image"):
                runs.append("")
            else:
                runs[-1] += plain([child])
        segments.extend(" ".join(run.split()) for run in runs if run.strip())
    return "\n".join(segments)


def heading_ids(text: str) -> set[str]:
    # Use actual headings and rendered inline text, never specimen code. These
    # are GitHub-like slugs; custom renderer IDs and raw HTML anchors need review.
    result = set()
    tokens = MARKDOWN.parse(text)
    for index, token in enumerate(tokens):
        if token.type != "heading_open":
            continue
        title = plain(tokens[index + 1].children or [])
        slug = re.sub(r"[^\w\- ]", "", title.lower()).replace(" ", "-")
        identifier, suffix = slug, 0
        while identifier in result:
            suffix += 1
            identifier = f"{slug}-{suffix}"
        result.add(identifier)
    return result
