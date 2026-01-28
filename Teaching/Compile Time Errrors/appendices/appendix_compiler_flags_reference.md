# Appendix: Compiler Flags Reference

## Complete Guide to Compile-Time Safety Flags

---

## Quick Reference Table

| Category | GCC/Clang | MSVC | Effect |
|----------|-----------|------|--------|
| Missing return | `-Werror=return-type` | `/we4715` | Error on missing return |
| Missing enum case | `-Werror=switch-enum` | `/we4062` | Error on unhandled enum |
| Narrowing | `-Wconversion` | `/W4` | Warn on narrowing |
| Sign conversion | `-Wsign-conversion` | `/W4` | Warn on sign mismatch |
| Non-virtual dtor | `-Wnon-virtual-dtor` | C4265 | Warn on missing virtual dtor |
| Ignored nodiscard | `-Wunused-result` | C4834 | Warn on ignored [[nodiscard]] |
| Null dereference | `-Wnull-dereference` | `/analyze` | Warn on null deref |
| Uninitialized | `-Wuninitialized` | `/W4` | Warn on uninitialized use |

---

## GCC/Clang Detailed Reference

### Essential Flags (Always Enable)

```bash
# Baseline warnings
-Wall                    # Common warnings
-Wextra                  # Extra warnings
-Wpedantic               # Strict ISO compliance

# Critical errors
-Werror=return-type      # Missing return in non-void function
-Werror=switch-enum      # Missing enum case (even with default)
```

### Enum and Switch Warnings

```bash
-Wswitch                 # Warn if enum value not handled (no default)
-Wswitch-enum            # Warn if enum value not handled (even with default)
-Wswitch-default         # Warn if no default case
-Wswitch-bool            # Warn on switch over boolean

# Recommended: make missing enum case an error
-Werror=switch-enum
```

**Example:**
```cpp
enum class Color { Red, Green, Blue };
void use(Color c) {
    switch (c) {
        case Color::Red: break;
        case Color::Green: break;
        // Missing Blue!
    }
}
// -Wswitch-enum: warning: enumeration value 'Blue' not handled
```

### Type Conversion Warnings

```bash
-Wconversion             # Implicit conversions that may change value
-Wsign-conversion        # Signed/unsigned conversion
-Wfloat-conversion       # Float to integer conversion
-Wnarrowing              # Narrowing in brace initialization (error by default)
-Wfloat-equal            # Direct floating-point equality comparison
-Wdouble-promotion       # Float implicitly promoted to double
```

**Example:**
```cpp
int x = 3.14;           // -Wconversion: warning: conversion from 'double' to 'int'
unsigned u = -1;        // -Wsign-conversion: warning: negative value
```

### Class Design Warnings

```bash
-Wnon-virtual-dtor       # Base class without virtual destructor
-Wdelete-non-virtual-dtor # Delete via non-virtual dtor
-Woverloaded-virtual     # Overload hides base class virtual
-Wsuggest-override       # Missing 'override' keyword
-Weffc++                 # Effective C++ violations (verbose)
```

**Example:**
```cpp
class Base {
public:
    virtual void foo();
    ~Base();  // -Wnon-virtual-dtor: should be virtual
};

class Derived : public Base {
    void foo();  // -Wsuggest-override: should have 'override'
};
```

### Return Value Warnings

```bash
-Wreturn-type            # Missing return in non-void function
-Wunused-result          # Ignoring [[nodiscard]] return value

# Make missing return an error
-Werror=return-type
```

### Initialization Warnings

```bash
-Wuninitialized          # Use of uninitialized variable
-Wmaybe-uninitialized    # Possibly uninitialized (GCC)
-Wsometimes-uninitialized # Possibly uninitialized (Clang)
-Wconditional-uninitialized # Conditionally uninitialized (Clang)
```

### Null Pointer Warnings

```bash
-Wnull-dereference       # Warn on null pointer dereference
-Wnonnull                # Warn on null passed to nonnull parameter
-Wnonnull-compare        # Warn on comparing nonnull parameter to null
```

### Unused Entity Warnings

```bash
-Wunused                 # All unused warnings
-Wunused-parameter       # Unused function parameter
-Wunused-variable        # Unused local variable
-Wunused-function        # Unused static function
-Wunused-but-set-variable # Variable set but never used

# Often too noisy; enable selectively
```

### Shadow Warnings

```bash
-Wshadow                 # Local shadows another variable
-Wshadow=local           # Only local shadowing (less noisy)
-Wshadow=compatible-local # Shadow with compatible type
```

### Format String Warnings

```bash
-Wformat                 # Check printf/scanf format strings
-Wformat=2               # More format checking
-Wformat-security        # Format string security issues
-Wformat-nonliteral      # Non-literal format string
```

### Miscellaneous Important Warnings

```bash
-Wcast-align             # Cast increases alignment requirement
-Wcast-qual              # Cast removes const/volatile
-Wold-style-cast         # C-style casts (prefer static_cast etc.)
-Wzero-as-null-pointer-constant # Using 0 instead of nullptr
-Wextra-semi             # Extra semicolons
-Wimplicit-fallthrough   # Missing [[fallthrough]] in switch
```

---

## MSVC Detailed Reference

### Warning Levels

```
/W0     # No warnings (don't use)
/W1     # Severe warnings only
/W2     # Significant warnings
/W3     # Production quality (default)
/W4     # Informational (recommended)
/Wall   # All warnings (very noisy, not recommended)
```

### Specific Warnings as Errors

```
/we<number>   # Treat warning <number> as error

# Essential
/we4715      # Not all paths return value
/we4062      # Missing enum case in switch
/we4834      # Discarding [[nodiscard]] return

# Recommended
/we4265      # Class has virtual functions but dtor not virtual
/we4702      # Unreachable code
/we4389      # Signed/unsigned mismatch in comparison
```

### Disable Specific Warnings

```
/wd<number>   # Disable warning <number>

# Common suppressions
/wd4100      # Unreferenced formal parameter
/wd4127      # Conditional expression is constant
/wd4201      # Nonstandard extension: nameless struct/union
/wd4324      # Structure padded due to alignment specifier
```

### Analysis Flags

```
/analyze             # Enable static analysis
/analyze:WX-         # Analysis warnings not as errors
/analyze:stacksize<n> # Check stack usage
```

### Conformance Flags

```
/permissive-         # Strict standards conformance
/Zc:__cplusplus      # Report correct __cplusplus value
/Zc:inline           # Remove unreferenced COMDAT
/Zc:throwingNew      # Assume operator new throws
```

---

## Recommended Configurations

### Development (Maximum Checking)

**GCC/Clang:**
```bash
-Wall -Wextra -Wpedantic -Werror
-Wconversion -Wsign-conversion
-Wnon-virtual-dtor -Woverloaded-virtual
-Werror=return-type -Werror=switch-enum
-fsanitize=address,undefined
-fno-omit-frame-pointer
```

**MSVC:**
```
/W4 /WX /permissive-
/we4715 /we4062 /we4834 /we4265
/analyze
```

### CI/Release (Balanced)

**GCC/Clang:**
```bash
-Wall -Wextra -Wpedantic
-Werror=return-type -Werror=switch-enum
-Wconversion -Wsign-conversion
```

**MSVC:**
```
/W4 /permissive-
/we4715 /we4062
```

### Legacy Code (Gradual Improvement)

**GCC/Clang:**
```bash
-Wall
# Add specific -Werror flags incrementally as code is fixed
```

**MSVC:**
```
/W3
# Gradually increase to /W4
```

---

## CMake Configuration

### Basic Setup

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Werror=switch-enum
        -Wconversion
        -Wsign-conversion
        -Wnon-virtual-dtor
    )
    
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(
            -Wno-c++98-compat
            -Wno-c++98-compat-pedantic
        )
    endif()
    
elseif(MSVC)
    add_compile_options(
        /W4
        /permissive-
        /we4715
        /we4062
        /we4834
        /Zc:__cplusplus
    )
endif()
```

### Debug vs Release

```cmake
# Debug: more checking
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address,undefined")

# Release: optimize
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
```

### Per-Target Warnings

```cmake
target_compile_options(my_library PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Werror>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
)

# Suppress warnings for third-party code
target_compile_options(third_party PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
    $<$<CXX_COMPILER_ID:MSVC>:/W0>
)
```

---

## Sanitizers

### Address Sanitizer (ASan)

Detects memory errors at runtime:

```bash
# GCC/Clang
-fsanitize=address -fno-omit-frame-pointer

# Detects:
# - Use after free
# - Heap buffer overflow
# - Stack buffer overflow
# - Global buffer overflow
# - Memory leaks (with leak sanitizer)
```

### Undefined Behavior Sanitizer (UBSan)

Detects undefined behavior at runtime:

```bash
-fsanitize=undefined

# Detects:
# - Signed integer overflow
# - Null pointer dereference
# - Division by zero
# - Invalid shift
# - Out-of-bounds array access (some)
```

### Thread Sanitizer (TSan)

Detects data races:

```bash
-fsanitize=thread

# Cannot combine with ASan
```

### Combined (Development)

```bash
-fsanitize=address,undefined -fno-omit-frame-pointer
```

---

## Static Analyzers

### Clang Static Analyzer

```bash
# Direct invocation
scan-build cmake ..
scan-build make

# Specific checkers
-enable-checker core.NullDereference
-enable-checker deadcode.DeadStores
```

### Clang-Tidy

```bash
# Run all checks
clang-tidy source.cpp

# Specific checks
clang-tidy -checks='bugprone-*,modernize-*' source.cpp

# With compile database
clang-tidy -p build/ source.cpp
```

**.clang-tidy configuration:**
```yaml
Checks: >
  -*,
  bugprone-*,
  modernize-*,
  performance-*,
  readability-*,
  -modernize-use-trailing-return-type

WarningsAsErrors: '*'
```

### Cppcheck

```bash
cppcheck --enable=all --error-exitcode=1 src/
```

### PVS-Studio

```bash
pvs-studio-analyzer analyze -o report.log
plog-converter -t errorfile report.log
```

---

## Flag Reference by Session

| Session | Key Flags |
|---------|-----------|
| 1: Strong Typedefs | (No specific flags—type system) |
| 2: Enum Exhaustiveness | `-Werror=switch-enum`, `/we4062` |
| 3: State Machine | (No specific flags—runtime) |
| 4: Type-State | (No specific flags—type system) |
| 5: Const Correctness | `-Wcast-qual` |
| 6: Non-Null References | `-Wnull-dereference`, `-Wnonnull` |
| 7: [[nodiscard]] | `-Wunused-result`, `/we4834` |
| 8: Template Constraints | (No specific flags—concepts/SFINAE) |
| Mini 2: Narrowing | `-Wconversion`, `-Wnarrowing` |
| Mini 3: static_assert | (No flags—language feature) |
| Mini 4: noexcept | (No specific flags) |

---

## Summary

### Absolute Minimum

```bash
# GCC/Clang
-Wall -Werror=return-type -Werror=switch-enum

# MSVC
/W4 /we4715 /we4062
```

### Recommended

```bash
# GCC/Clang
-Wall -Wextra -Wpedantic
-Werror=return-type -Werror=switch-enum
-Wconversion -Wsign-conversion
-Wnon-virtual-dtor

# MSVC
/W4 /permissive- /we4715 /we4062 /we4834 /we4265
```

### Maximum Safety (Dev/CI)

```bash
# GCC/Clang
-Wall -Wextra -Wpedantic -Werror
-Wconversion -Wsign-conversion -Wold-style-cast
-fsanitize=address,undefined

# MSVC
/W4 /WX /permissive- /analyze
```

---

## Further Reading

- [GCC Warning Options](https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html)
- [Clang Diagnostics Reference](https://clang.llvm.org/docs/DiagnosticsReference.html)
- [MSVC Warning Reference](https://docs.microsoft.com/en-us/cpp/error-messages/compiler-warnings/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
