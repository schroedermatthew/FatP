# StateMachine Compile-Fail Contract Suite

This directory contains **compile-fail** tests for `fat_p::StateMachine`.

These tests are **supposed to fail compilation**. Their purpose is to lock down
compile-time contracts (static_asserts, hard constraints, and required APIs)
that cannot be validated with runtime unit tests.

## How to run

### GCC/Clang (Linux/macOS)
From the repo root:

```bash
CXX=${CXX:-g++}
CXXFLAGS="-std=c++20 -I./fat_p -I./tests -Wall -Wextra -pedantic"

for f in tests/compile_fail/compile_fail_StateMachine_*.cpp; do
  echo "[COMPILE-FAIL] $f"
  if $CXX $CXXFLAGS -c "$f" -o /tmp/compile_fail.o; then
    echo "ERROR: expected compilation failure but succeeded: $f"
    exit 1
  fi
  echo "PASS: failed as expected"
done
```

### MSVC (Developer Command Prompt)
From the repo root:

```bat
for %f in (tests\compile_fail\compile_fail_StateMachine_*.cpp) do (
  echo [COMPILE-FAIL] %f
  cl /std:c++20 /W4 /permissive- /EHsc /c %f /I fat_p /I tests
  if %errorlevel%==0 (
    echo ERROR: expected compilation failure but succeeded: %f
    exit /b 1
  )
)
```

## Notes

- Do **not** add these files to the normal unit-test executable.
- Treat this directory as a **separate CI job** (expected-fail compilation).
- Each file should fail for a **single primary reason** (one contract at a time).
