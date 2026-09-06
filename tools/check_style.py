# FATP_META:
#   meta_version: 1
#   component: GuidelineTooling
#   file_role: tooling
#   path: tools/check_style.py
#   layer: Infrastructure
#   summary: Checks mandatory formatting, lexical rules, and selected-file Clang naming.
#   api_stability: in_work

"""Check effective house formatting, lexical rules and Clang AST naming; never rewrite files."""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

from guideline_lib import (
    CPP_HEADER_SUFFIXES, CPP_SUFFIXES, console, execute, formatter_integrity, inventory,
    load_component_headers, profile, read, repo_path, report, roots, validate_profile,
)

PASCAL = re.compile(r"[A-Z][A-Za-z0-9]*\Z")
CAMEL = re.compile(r"[a-z][A-Za-z0-9]*\Z")
# Clang spells every real operator function as operator followed by a non-identifier
# character or a space: operator+, operator[], operator=, operator new, operator""_m.
# An ordinary function merely starting with those letters, such as operator_apply,
# is an authored name and stays subject to the camelCase rule.
OPERATOR = re.compile(r'operator(?:\s|[+\-*/%<>=!&|^~,\[\]()"])')
PROTOCOL_NAMES = {
    "push_back", "push_front", "pop_back", "pop_front", "emplace_back", "emplace_front",
    "insert_or_assign", "try_emplace", "equal_range", "lower_bound", "upper_bound",
    "max_size", "bucket_count", "max_bucket_count", "bucket_size", "load_factor",
    "max_load_factor", "get_allocator", "allocate_at_least",
}
PROTOCOL_ALIASES = {
    "value_type", "size_type", "difference_type", "reference", "const_reference",
    "pointer", "const_pointer", "iterator", "const_iterator", "reverse_iterator",
    "const_reverse_iterator", "iterator_category", "iterator_concept", "element_type",
    "key_type", "mapped_type", "allocator_type", "traits_type", "char_type",
}
# A line comment ends at the first newline that is not spliced by a trailing
# backslash: translation phase 2 joins those lines, so the compiler reads the
# continuation as comment text, not as a directive or a declaration.
LEXEME = re.compile(r'R"(?P<rawdelimiter>[^\s()\\]{0,16})\(.*?\)(?P=rawdelimiter)"'
                     r'|//(?:\\\r?\n|[^\n])*|/\*.*?\*/|"(?:\\.|[^"\\])*"'
                     r'|(?<!\w)(?:u8|[uUL])?\'(?:\\.|[^\'\\])*\'', re.S)


def without_comments(text: str, *, literals=False) -> str:
    return LEXEME.sub(lambda m: re.sub(r"[^\n]", " ", m.group()) if literals or m.group().startswith("/")
                      else m.group(), text)


def without_literals(text: str) -> str:
    return LEXEME.sub(lambda m: m.group() if m.group().startswith("/")
                      else re.sub(r"[^\n]", " ", m.group()), text)


def lexical(path: Path, text: str, prefix: str | None, first_header: str | None) -> list[str]:
    errors = []
    cleaned = without_comments(text)
    code = without_comments(text, literals=True)
    if path.suffix.lower() in CPP_HEADER_SUFFIXES:
        first = next((s.strip() for s in cleaned.splitlines() if s.strip()), "")
        if first != "#pragma once":
            errors.append(f"{path.name}: header must begin with #pragma once")
        if re.search(r"\busing\s+namespace\b", code):
            errors.append(f"{path.name}: using namespace is prohibited in headers")
    continued_macro = False
    for number, (line, source_line) in enumerate(zip(text.splitlines(), code.splitlines()), 1):
        macro = re.match(r"\s*#\s*define\s+([A-Za-z_]\w*)", source_line)
        exempt = bool(macro) or continued_macro
        if len(line) > 120 and not exempt:
            errors.append(f"{path.name}:{number}: line exceeds 120 columns")
        continued_macro = exempt and source_line.rstrip().endswith("\\")
        if macro:
            name = macro.group(1)
            if not prefix or not re.fullmatch(re.escape(prefix) + r"_[A-Z0-9_]+", name):
                errors.append(f"{path.name}:{number}: macro {name} needs the project SCREAMING_SNAKE prefix")
    if first_header:
        includes = re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]', cleaned, re.M)
        if not includes or includes[0] != first_header:
            errors.append(f"{path.name}: first include must be {first_header}")
    return errors


def naming_from_ast(ast: dict, path: Path, source: str) -> tuple[list[str], int]:
    errors, checked = [], 0
    main = path.resolve()
    # Clang offsets address original bytes, including a UTF-8 BOM and CRLF.
    # The source argument is retained for callers; read() has already normalized
    # its newlines and removed a BOM, so it cannot recover those byte locations.
    source_bytes = path.read_bytes()
    last_file = main
    seen = set()
    record_contexts = set()

    def walk(node, inherited_file, record=None, member_context=False):
        nonlocal checked, last_file
        if not isinstance(node, dict):
            return
        loc = node.get("loc", {})
        loc = loc.get("expansionLoc", loc)
        explicit = loc.get("file")
        if explicit:
            last_file = Path(explicit).resolve()
        current = Path(explicit).resolve() if explicit else (inherited_file or last_file)
        # Clang marks every location inside an included file with includedFrom, even
        # where it elides the repeated file name. Names declared by an included file
        # are that file's, whichever enclosing declaration or previous location the
        # elided name would otherwise inherit.
        included = "includedFrom" in loc
        kind, name = node.get("kind"), node.get("name", "")
        children = node.get("inner", [])
        next_record = record
        next_member_context = member_context
        if kind in ("CXXRecordDecl", "RecordDecl"):
            if node.get("id") is not None:
                record_contexts.add(node["id"])
            next_member_context = True
        elif kind in ("TranslationUnitDecl", "NamespaceDecl", "FunctionDecl", "CXXMethodDecl",
                      "CXXConstructorDecl", "CXXDestructorDecl", "CXXConversionDecl"):
            next_member_context = False
        if kind in ("CXXRecordDecl", "RecordDecl") and node.get("completeDefinition"):
            aggregate = node.get("definitionData", {}).get("isAggregate", False)
            user_special = any(c.get("kind") in ("CXXConstructorDecl", "CXXDestructorDecl")
                               and not c.get("isImplicit") for c in children)
            next_record = {"aggregate": aggregate and not user_special}
        expected = None
        if name and loc and current == main and not included and not node.get("isImplicit"):
            if kind in ("CXXRecordDecl", "RecordDecl", "EnumDecl", "EnumConstantDecl",
                        "TemplateTypeParmDecl", "NonTypeTemplateParmDecl", "TemplateTemplateParmDecl"):
                expected = ("PascalCase", PASCAL)
            elif kind in ("TypeAliasDecl", "TypedefDecl") and name not in PROTOCOL_ALIASES:
                expected = ("PascalCase alias", PASCAL)
            elif kind in ("FunctionDecl", "CXXMethodDecl"):
                external_override = any(c.get("kind") == "OverrideAttr" for c in children)
                if not OPERATOR.match(name) and name not in PROTOCOL_NAMES and not external_override:
                    expected = ("camelCase function", CAMEL)
            elif kind == "FieldDecl":
                if record and record["aggregate"]:
                    expected = ("unprefixed camelCase aggregate field",
                                re.compile(r"(?!m[A-Z])[a-z][A-Za-z0-9]*\Z"))
                else:
                    expected = ("m plus PascalCase member", re.compile(r"m[A-Z][A-Za-z0-9]*\Z"))
            elif kind in ("VarDecl", "ParmVarDecl", "BindingDecl"):
                if node.get("constexpr"):
                    expected = ("k plus PascalCase constant", re.compile(r"k[A-Z][A-Za-z0-9]*\Z"))
                elif kind == "VarDecl" and (member_context or
                        node.get("parentDeclContextId") in record_contexts):
                    expected = ("s plus PascalCase static data member", re.compile(r"s[A-Z][A-Za-z0-9]*\Z"))
                else:
                    expected = ("camelCase variable", CAMEL)
            elif kind == "NamespaceDecl":
                expected = ("lowercase namespace", re.compile(r"[a-z][a-z0-9_]*\Z"))
            if expected:
                key = (kind, name, loc.get("offset"))
                if key not in seen:
                    seen.add(key)
                    checked += 1
                    if not expected[1].fullmatch(name):
                        line = loc.get("line", source_bytes[:loc.get("offset", 0)].count(b"\n") + 1)
                        errors.append(f"{path.name}:{line}: {name}: expected {expected[0]}")
        for child in children:
            # AST file names are elided between adjacent source locations. At TU level,
            # use Clang's last explicit file; inside a declaration, inherit its file.
            walk(child, None if kind == "TranslationUnitDecl" else current, next_record, next_member_context)

    walk(ast, None)
    return errors, checked


def ast_check(path: Path, source: str, compiler: str, flags: list[str], cwd: Path,
              standard: int = 20) -> tuple[list[str], int]:
    for flag in flags:
        if flag in ("-std", "--std"):
            return ["AST: use an explicit -std=c++20-or-later option"], 0
        if flag.startswith(("-std=", "--std=")):
            if not re.fullmatch(r"--?std=(?:gnu|c)\+\+(?:20|23|26|2a|2b|2c)", flag):
                return ["AST: compile flags lower the C++20 minimum or select an unrecognized mode"], 0
    language = "c++-header" if path.suffix.lower() in CPP_HEADER_SUFFIXES else "c++"
    command = [compiler, f"-std=c++{standard}", "-x", language, "-fsyntax-only",
               "-Xclang", "-ast-dump=json", *flags, str(path)]
    result = execute(command, cwd=cwd)
    if result.returncode:
        return [f"{path.name}: AST syntax check failed: {result.stderr.strip()}"], 0
    try:
        ast = json.loads(result.stdout)
    except ValueError:
        return [f"{path.name}: compiler did not return a complete Clang JSON AST"], 0
    return naming_from_ast(ast, path, source)


def check_file(path: Path, repo: Path, data: dict, *, formatter="clang-format",
               compiler="clang++", flags=None, headers=None) -> tuple[list[str], int]:
    if headers is None:
        headers = load_component_headers(data, repo)
    if path.suffix.lower() not in CPP_SUFFIXES:
        return [f"{path}: unsupported C++ source extension"], 0
    source = read(path)
    errors = []
    formatted = execute([formatter, f"--style=file:{repo / '.clang-format'}",
                         f"--assume-filename={path}"], text=source, cwd=repo)
    if formatted.returncode:
        errors.append(f"{path.name}: formatter failed: {formatted.stderr.strip()}")
    elif formatted.stdout != source:
        errors.append(f"{path.name}: formatting differs from the mandatory root .clang-format")
    rel = path.relative_to(repo).as_posix()
    errors.extend(lexical(path, source, data["identity"]["macro_prefix"],
                          headers.get(rel)))
    ast_errors, count = ast_check(path, source, compiler, flags or [], repo, data["cpp"]["standard"])
    errors.extend(ast_errors)
    return errors, count


def check_files(files, repo: Path, data: dict, *, formatter="clang-format",
                compiler="clang++", flags=None) -> tuple[list[str], int]:
    """Visit the whole inventory; a failed prerequisite remains a failed file."""
    errors, declarations = [], 0
    headers = load_component_headers(data, repo)
    for path in sorted(files):
        try:
            findings, count = check_file(path, repo, data, formatter=formatter,
                                         compiler=compiler, flags=flags, headers=headers)
        except (ValueError, OSError) as exc:
            findings, count = [f"{path.relative_to(repo).as_posix()}: {exc}"], 0
        errors.extend(findings)
        declarations += count
    return errors, declarations


def main():
    console()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", required=True)
    parser.add_argument("--guidelines", default="guidelines")
    parser.add_argument("--file", action="append", default=[])
    parser.add_argument("--inventory", action="store_true")
    parser.add_argument("--clang-format", default="clang-format")
    parser.add_argument("--compiler", default="clang++")
    parser.add_argument("compile_flags", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        g, repo = roots(args.guidelines, args.repo_root)
        data = profile(g)
        errors = validate_profile(data, repo, True)
        errors.extend(formatter_integrity(repo, args.clang_format))
        if "cpp" not in data["languages"]:
            errors.append("style: C++ must be registered in the project profile")
        if errors:
            return report(errors, "style")
        files = {repo_path(repo, name) for name in args.file}
        if args.inventory:
            files |= inventory(repo, data["cpp"]["authored_globs"], data["cpp"]["excluded_globs"])
        if not files:
            return report(["style: no files selected"], "style")
        flags = args.compile_flags
        if flags[:1] == ["--"]:
            flags = flags[1:]
        findings, declarations = check_files(files, repo, data, formatter=args.clang_format,
                                             compiler=args.compiler, flags=flags)
        errors.extend(findings)
        print(f"Checked {len(files)} selected files; {declarations} named declarations.")
        print("Manual review required: protocol/ABI exceptions, meaningful names, inactive preprocessor paths,")
        print("macro semantics, authored inventory completeness, unmapped include order and architectural layering.")
        print("AST checks are syntax-only in the supplied configuration, not a build or execution.")
        return report(errors, "automated style")
    except (ValueError, OSError, TypeError, KeyError) as exc:
        return report([str(exc)], "style")


if __name__ == "__main__":
    raise SystemExit(main())
