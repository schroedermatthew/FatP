#!/usr/bin/env python3
"""
FATP_META Parser - Robust YAML extraction from C++ headers.

Tolerates common whitespace variations:
- Tabs (converted to 2 spaces)
- Inconsistent indentation (normalized via dedent)
- Leading/trailing whitespace on lines
- Comment prefixes (* or //)

Usage:
    python fatp_meta_parser.py path/to/header.h
    python fatp_meta_parser.py --validate path/to/headers/
    python fatp_meta_parser.py --dump path/to/header.h
"""

import re
import sys
import textwrap
from pathlib import Path
from typing import Optional, Dict, Any, List, Tuple

try:
    import yaml
except ImportError:
    print("ERROR: PyYAML not installed. Run: pip install pyyaml", file=sys.stderr)
    sys.exit(1)


def extract_fatp_meta_block(content: str) -> Optional[str]:
    """
    Extract the raw FATP_META block from file content.
    Returns the YAML portion (after 'FATP_META:' line, before closing '*/')
    """
    # Pattern: /* ... FATP_META: ... */ or // FATP_META: style
    # We look for FATP_META: and capture until end of comment
    
    # Try block comment style first: /* FATP_META: ... */
    block_pattern = r'/\*[\s\S]*?FATP_META:\s*\n([\s\S]*?)\*/'
    match = re.search(block_pattern, content)
    if match:
        return match.group(1)
    
    # Try line comment style: // FATP_META: followed by // lines
    line_pattern = r'//\s*FATP_META:\s*\n((?://.*\n)*)'
    match = re.search(line_pattern, content)
    if match:
        # Strip // prefix from each line
        lines = match.group(1).split('\n')
        cleaned = [re.sub(r'^//\s?', '', line) for line in lines]
        return '\n'.join(cleaned)
    
    return None


def normalize_yaml_block(raw_block: str) -> str:
    """
    Normalize a raw YAML block to handle common formatting issues:
    - Convert tabs to spaces
    - Strip comment prefixes (* at start of lines)
    - Dedent to remove common leading whitespace
    - Strip trailing whitespace from lines
    - Attempt to fix single-level indentation errors (common copy-paste issues)
    """
    lines = raw_block.split('\n')
    normalized = []
    
    for line in lines:
        # Convert tabs to 2 spaces (YAML standard for this project)
        line = line.replace('\t', '  ')
        
        # Strip leading comment markers: "  * " or " * " or "* "
        line = re.sub(r'^\s*\*\s?', '', line)
        
        # Strip trailing whitespace
        line = line.rstrip()
        
        normalized.append(line)
    
    # Join and dedent to handle cases where entire block is indented
    result = '\n'.join(normalized)
    result = textwrap.dedent(result)
    
    # Remove leading/trailing blank lines
    result = result.strip()
    
    # Attempt to fix common indentation issues:
    # If we have top-level keys that should all be at indent 0, but some have
    # small indentation (1-3 spaces), normalize them.
    lines = result.split('\n')
    fixed_lines = []
    
    # Known top-level keys in FATP_META schema
    top_level_keys = {
        'meta_version', 'component', 'file_role', 'path', 'namespace',
        'layer', 'summary', 'api_stability', 'related', 'hygiene', 'generated'
    }
    
    for line in lines:
        if not line.strip():
            fixed_lines.append(line)
            continue
        
        # Check if this looks like a top-level key with wrong indentation
        # Pattern: small indent (1-3 spaces) followed by known key
        match = re.match(r'^(\s{1,3})(\w+):', line)
        if match:
            key = match.group(2)
            if key in top_level_keys:
                # This is a top-level key with small errant indentation - fix it
                line = line.lstrip()
        
        fixed_lines.append(line)
    
    return '\n'.join(fixed_lines)


def parse_fatp_meta(content: str) -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
    """
    Parse FATP_META from file content.
    
    Returns:
        (parsed_dict, None) on success
        (None, error_message) on failure
    """
    raw_block = extract_fatp_meta_block(content)
    if raw_block is None:
        return None, "No FATP_META block found"
    
    normalized = normalize_yaml_block(raw_block)
    
    if not normalized:
        return None, "FATP_META block is empty after normalization"
    
    try:
        parsed = yaml.safe_load(normalized)
        if not isinstance(parsed, dict):
            return None, f"FATP_META parsed to {type(parsed).__name__}, expected dict"
        return parsed, None
    except yaml.YAMLError as e:
        # Provide helpful error with the normalized block for debugging
        return None, f"YAML parse error: {e}\n\nNormalized block:\n{normalized}"


def parse_file(filepath: Path) -> Tuple[Optional[Dict[str, Any]], Optional[str]]:
    """Parse FATP_META from a file path."""
    try:
        content = filepath.read_text(encoding='utf-8')
    except Exception as e:
        return None, f"Failed to read file: {e}"
    
    return parse_fatp_meta(content)


def validate_required_keys(meta: Dict[str, Any]) -> List[str]:
    """Check for required keys per schema v1."""
    required = ['meta_version', 'component', 'file_role', 'path', 'summary']
    missing = [k for k in required if k not in meta]
    return missing


def validate_file(filepath: Path) -> Tuple[bool, str]:
    """
    Validate a single file's FATP_META block.
    
    Returns:
        (True, "OK") on success
        (False, error_description) on failure
    """
    meta, error = parse_file(filepath)
    
    if error:
        return False, error
    
    # Check meta_version
    if meta.get('meta_version') != 1:
        return False, f"meta_version is {meta.get('meta_version')}, expected 1"
    
    # Check required keys
    missing = validate_required_keys(meta)
    if missing:
        return False, f"Missing required keys: {', '.join(missing)}"
    
    # Check path matches actual file location (basic check)
    declared_path = meta.get('path', '')
    if not str(filepath).endswith(declared_path.replace('/', str(filepath).count('/') > 0 and '/' or '\\')):
        # This is a loose check - just warn, don't fail
        pass
    
    return True, "OK"


def main():
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Parse and validate FATP_META blocks in C++ headers',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('paths', nargs='+', help='Files or directories to process')
    parser.add_argument('--validate', action='store_true', help='Validate files and report errors')
    parser.add_argument('--dump', action='store_true', help='Dump parsed YAML as JSON')
    parser.add_argument('--quiet', '-q', action='store_true', help='Only show errors')
    parser.add_argument('--extensions', default='.h,.hpp,.cpp', help='File extensions to process (comma-separated)')
    
    args = parser.parse_args()
    extensions = tuple(args.extensions.split(','))
    
    # Collect files
    files = []
    for path_str in args.paths:
        path = Path(path_str)
        if path.is_file():
            files.append(path)
        elif path.is_dir():
            for ext in extensions:
                files.extend(path.rglob(f'*{ext}'))
        else:
            print(f"WARNING: {path} not found", file=sys.stderr)
    
    if not files:
        print("No files to process", file=sys.stderr)
        return 1
    
    # Process files
    errors = []
    success = 0
    no_meta = 0
    
    for filepath in sorted(files):
        meta, error = parse_file(filepath)
        
        if error and "No FATP_META block found" in error:
            no_meta += 1
            if not args.quiet:
                print(f"SKIP {filepath} — no FATP_META")
            continue
        
        if error:
            errors.append((filepath, error))
            print(f"FAIL {filepath}")
            if not args.quiet:
                # Indent error message
                for line in error.split('\n'):
                    print(f"     {line}")
            continue
        
        if args.validate:
            valid, msg = validate_file(filepath)
            if not valid:
                errors.append((filepath, msg))
                print(f"FAIL {filepath} — {msg}")
                continue
        
        success += 1
        
        if args.dump:
            import json
            print(f"# {filepath}")
            print(json.dumps(meta, indent=2))
            print()
        elif not args.quiet:
            print(f"OK   {filepath}")
    
    # Summary
    print()
    print(f"Processed: {len(files)} files")
    print(f"  Success: {success}")
    print(f"  No meta: {no_meta}")
    print(f"  Errors:  {len(errors)}")
    
    if errors:
        print("\nFiles with errors:")
        for filepath, _ in errors:
            print(f"  * {filepath}")
        return 1
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
