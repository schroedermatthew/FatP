# Stringify Compile-Fail Contract Suite

This directory contains **compile-fail** tests for `fat_p::Stringify.h`.

These tests are **supposed to fail compilation**. Their purpose is to lock down
compile-time contracts (static_asserts, concept constraints) that cannot be
validated with runtime unit tests.

## How to run

### GCC/Clang (Linux/macOS)
From the repo root:

```bash
CXX=${CXX:-g++}
CXXFLAGS="-std=c++20 -I./fat_p -I./tests -Wall -Wextra -pedantic"

for f in tests/compile_fail/compile_fail_Stringify_*.cpp; do
  echo "[COMPILE-FAIL] $f"
  if $CXX $CXXFLAGS -c "$f" -o /tmp/compile_fail.o 2>/dev/null; then
    echo "ERROR: expected compilation failure but succeeded: $f"
    exit 1
  fi
  echo "PASS: failed as expected"
done
```

### MSVC (Developer Command Prompt)
From the repo root:

```bat
for %f in (tests\compile_fail\compile_fail_Stringify_*.cpp) do (
  echo [COMPILE-FAIL] %f
  cl /std:c++20 /W4 /permissive- /EHsc /c %f /I fat_p /I tests 2>nul
  if %errorlevel%==0 (
    echo ERROR: expected compilation failure but succeeded: %f
    exit /b 1
  )
  echo PASS: failed as expected
)
```

## Tests

| File | Contract Tested |
|------|-----------------|
| `compile_fail_Stringify_ConceptStreamableReject.cpp` | `concepts::streamable` rejects types without `operator<<` |
| `compile_fail_Stringify_ConceptHasToStringMethodReject.cpp` | `concepts::has_to_string_method` rejects wrong return type |
| `compile_fail_Stringify_ConceptPrintableRangeRejectString.cpp` | `concepts::printable_range` excludes string types |
| `compile_fail_Stringify_ConceptStringifiableReject.cpp` | `concepts::stringifiable` rejects non-stringifiable types |

## Notes

- Do **not** add these files to the normal unit-test executable.
- Treat this directory as a **separate CI job** (expected-fail compilation).
- Each file should fail for a **single primary reason** (one contract at a time).
