#!/usr/bin/env python3
"""
check_header_self_containment.py

CI script to verify all Fat-P headers are self-contained.
Each header must compile standalone without requiring other headers to be included first.

Usage:
    python check_header_self_containment.py [--compiler CXX] [--std STD] [--include-dir DIR]

Example:
    python check_header_self_containment.py --compiler g++ --std c++20 --include-dir fat_p
    python check_header_self_containment.py --compiler clang++ --std c++20 --include-dir fat_p

Exit codes:
    0 - All headers are self-contained
    1 - One or more headers failed to compile standalone
"""

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import List, Tuple

# Headers that are intentionally not self-contained (internal/detail headers)
EXCLUDED_HEADERS = {
    # Add any headers that are intentionally not self-contained
}

# Headers that require special compiler flags
SPECIAL_FLAGS = {
    # Example: 'CoroutineTask.h': ['-fcoroutines'],
}


def find_headers(include_dir: str) -> List[Path]:
    """Find all .h and .hpp files in the include directory."""
    headers = []
    include_path = Path(include_dir)
    
    if not include_path.exists():
        print(f"Error: Include directory '{include_dir}' not found", file=sys.stderr)
        sys.exit(1)
    
    for ext in ['*.h', '*.hpp']:
        headers.extend(include_path.glob(ext))
    
    # Filter out excluded headers
    headers = [h for h in headers if h.name not in EXCLUDED_HEADERS]
    
    return sorted(headers)


def check_header(header: Path, compiler: str, std: str, include_dir: str) -> Tuple[bool, str]:
    """
    Check if a single header compiles standalone.
    
    Returns:
        (success: bool, error_message: str)
    """
    header_name = header.name
    
    # Create a temporary source file that just includes the header
    test_code = f'#include "{header_name}"\n'
    
    # Get any special flags for this header
    extra_flags = SPECIAL_FLAGS.get(header_name, [])
    
    # Build the compiler command
    cmd = [
        compiler,
        f'-std={std}',
        '-fsyntax-only',      # Don't generate output, just check syntax
        '-Wall',
        '-Wextra',
        '-Werror',            # Treat warnings as errors
        f'-I{include_dir}',
        '-x', 'c++',          # Treat input as C++
        '-',                  # Read from stdin
    ] + extra_flags
    
    try:
        result = subprocess.run(
            cmd,
            input=test_code,
            capture_output=True,
            text=True,
            timeout=30
        )
        
        if result.returncode == 0:
            return True, ""
        else:
            error_msg = result.stderr.strip() if result.stderr else result.stdout.strip()
            return False, error_msg
            
    except subprocess.TimeoutExpired:
        return False, "Compilation timed out"
    except FileNotFoundError:
        return False, f"Compiler '{compiler}' not found"
    except Exception as e:
        return False, str(e)


def main():
    parser = argparse.ArgumentParser(
        description='Check that all Fat-P headers are self-contained'
    )
    parser.add_argument(
        '--compiler', '-c',
        default='g++',
        help='C++ compiler to use (default: g++)'
    )
    parser.add_argument(
        '--std', '-s',
        default='c++20',
        help='C++ standard (default: c++20)'
    )
    parser.add_argument(
        '--include-dir', '-I',
        default='fat_p',
        help='Include directory containing headers (default: fat_p)'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Print verbose output'
    )
    parser.add_argument(
        '--continue-on-error',
        action='store_true',
        help='Continue checking remaining headers after a failure'
    )
    
    args = parser.parse_args()
    
    print(f"Checking header self-containment")
    print(f"  Compiler: {args.compiler}")
    print(f"  Standard: {args.std}")
    print(f"  Include dir: {args.include_dir}")
    print()
    
    headers = find_headers(args.include_dir)
    print(f"Found {len(headers)} headers to check")
    print()
    
    failed_headers = []
    passed_count = 0
    
    for header in headers:
        if args.verbose:
            print(f"Checking {header.name}...", end=' ', flush=True)
        
        success, error_msg = check_header(
            header,
            args.compiler,
            args.std,
            args.include_dir
        )
        
        if success:
            passed_count += 1
            if args.verbose:
                print("OK")
        else:
            failed_headers.append((header, error_msg))
            if args.verbose:
                print("FAILED")
                print(f"  Error: {error_msg[:200]}...")
            
            if not args.continue_on_error:
                break
    
    print()
    print("=" * 60)
    print(f"Results: {passed_count}/{len(headers)} headers passed")
    
    if failed_headers:
        print()
        print("FAILED HEADERS:")
        for header, error_msg in failed_headers:
            print(f"  - {header.name}")
            # Print first few lines of error
            error_lines = error_msg.split('\n')[:5]
            for line in error_lines:
                print(f"      {line[:100]}")
        print()
        print("FAILED: Not all headers are self-contained")
        return 1
    else:
        print()
        print("PASSED: All headers are self-contained")
        return 0


if __name__ == '__main__':
    sys.exit(main())
