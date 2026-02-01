#!/usr/bin/env python3
# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/validate_layers.py
#   summary: "Validator for layer assignments across public headers."
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

def extract_includes(content: str) -> list:
    """Extract all fat_p includes from file content (ignoring comments)."""
    includes = []
    for line in content.split('\n'):
        # Skip comment lines
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('*'):
            continue
        # Match #include "Something.h" or #include "fat_p/Something.h"
        match = re.search(r'#include\s*[<"](?:fat_p/)?(\w+\.h)[>"]', line)
        if match:
            includes.append(match.group(1))
    return includes

def main():
    headers_dir = Path('/home/claude/fat_p_code/fat_p')
    
    # Build layer map for all headers
    header_layers = {}
    header_contents = {}
    
    for header_path in sorted(headers_dir.glob('*.h')):
        name = header_path.name
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
            if inc not in header_layers:
                # External or std include, skip
                continue
            inc_layer = header_layers[inc]
            if inc_layer is None:
                continue
            if inc_layer not in LAYER_RANK:
                continue
            inc_rank = LAYER_RANK[inc_layer]
            
            if inc_rank > my_rank:
                violations.append({
                    'header': header,
                    'header_layer': layer,
                    'includes': inc,
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
