#!/usr/bin/env python3
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/validate_layers.py
#   summary: "Recursive validator for layer assignments across Fat-P headers."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false
"""
Validate Fat-P header layer assignments.

Checks that each header only includes headers from its layer or below.
"""

import re
import sys
from pathlib import Path
from collections import defaultdict

# Layer hierarchy (lower index = lower layer)
LAYER_ORDER = ['Foundation', 'Containers', 'Concurrency', 'Domain', 'Integration', 'Testing']
LAYER_RANK = {layer: i for i, layer in enumerate(LAYER_ORDER)}

def extract_layer(content: str) -> str:
    """Extract @layer or layer: from file content."""
    # Try @layer Tag
    match = re.search(r'@layer\s+(\w+)', content)
    if match:
        return match.group(1)
    # Try YAML layer:
    match = re.search(r'layer:\s*(\w+)', content)
    if match:
        return match.group(1)
    return None

def extract_includes(content: str) -> list[str]:
    """Extract all fat_p includes from file content (ignoring comments)."""
    includes = []
    for line in content.split('\n'):
        # Skip comment lines
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('*'):
            continue
        # Match root headers and owned implementation paths.
        match = re.search(r'#include\s*[<"](?:fat_p/)?([A-Za-z0-9_./-]+\.h)[>"]', line)
        if match:
            includes.append(match.group(1))
    return includes


def resolve_include(header: str, included: str, header_layers: dict[str, str]) -> str | None:
    """Resolve an include using quoted-include sibling precedence, then the include root."""
    normalized = included.replace('\\', '/')
    if '/' in normalized and normalized in header_layers:
        return normalized

    parent = Path(header).parent
    sibling = (parent / normalized).as_posix()
    if sibling in header_layers:
        return sibling

    if normalized in header_layers:
        return normalized

    matches = [candidate for candidate in header_layers if Path(candidate).name == normalized]
    return matches[0] if len(matches) == 1 else None

def main():
    # Auto-detect repo root: script is in tools/, headers in include/fat_p/
    script_dir = Path(__file__).resolve().parent
    repo_root = script_dir.parent  # tools/ -> repo root
    headers_dir = repo_root / 'include' / 'fat_p'
    
    if not headers_dir.exists():
        # Fallback: try current directory structure
        headers_dir = Path.cwd() / 'include' / 'fat_p'
    
    if not headers_dir.exists():
        print(f"ERROR: Cannot find headers directory. Tried:")
        print(f"  {repo_root / 'include' / 'fat_p'}")
        print(f"  {Path.cwd() / 'include' / 'fat_p'}")
        return 2
    
    # Build layer map for all headers
    header_layers = {}
    header_contents = {}
    
    for header_path in sorted(headers_dir.rglob('*.h')):
        name = header_path.relative_to(headers_dir).as_posix()
        content = header_path.read_text(encoding='utf-8', errors='replace')
        header_contents[name] = content
        layer = extract_layer(content)
        header_layers[name] = layer
    
    # Report headers with no layer
    no_layer = [h for h, l in header_layers.items() if l is None]
    if no_layer:
        print("MISSING LAYER TAG:")
        for h in no_layer:
            print(f"  {h}")
        print()
    
    # Report layer counts
    layer_counts = defaultdict(list)
    for h, l in header_layers.items():
        layer_counts[l or 'NONE'].append(h)
    
    print("LAYER ASSIGNMENTS:")
    for layer in LAYER_ORDER + ['NONE']:
        if layer in layer_counts:
            print(f"  {layer}: {len(layer_counts[layer])} headers")
    print()
    
    # Validate dependencies
    violations = []
    
    for header, layer in header_layers.items():
        if layer is None:
            continue
        if layer not in LAYER_RANK:
            print(f"WARNING: {header} has unknown layer '{layer}'")
            continue
            
        my_rank = LAYER_RANK[layer]
        content = header_contents[header]
        includes = extract_includes(content)
        
        for inc in includes:
            resolved = resolve_include(header, inc, header_layers)
            if resolved is None:
                # External or std include, skip
                continue
            inc_layer = header_layers[resolved]
            if inc_layer is None:
                continue
            if inc_layer not in LAYER_RANK:
                continue
            inc_rank = LAYER_RANK[inc_layer]
            
            if inc_rank > my_rank:
                violations.append({
                    'header': header,
                    'header_layer': layer,
                    'includes': resolved,
                    'inc_layer': inc_layer
                })
    
    if violations:
        print("LAYER VIOLATIONS (including header from higher layer):")
        for v in violations:
            print(f"  {v['header']} ({v['header_layer']}) includes {v['includes']} ({v['inc_layer']})")
        print()
        print(f"Total violations: {len(violations)}")
        return 1
    else:
        print("NO LAYER VIOLATIONS FOUND")
        return 0

if __name__ == '__main__':
    sys.exit(main())
