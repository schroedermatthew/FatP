# Compile-Time Safety Quick Reference Card

## A One-Page Guide to C++ Type System Safety

---

## Technique Selection Matrix

| If you need to prevent... | Use this technique | Session | Guarantee |
|---------------------------|-------------------|---------|-----------|
| Argument swapping (IDs, handles) | `StrongId<Tag>` | 1 | ✅ Compile-time |
| Missing enum case | `-Werror=switch-enum` | 2 | ✅ Compile-time |
| Missing variant handler | `overloaded{}` visitor | 9 | ✅ Compile-time |
| Invalid state transition | `StateMachine<>` | 3 | ⚠ Runtime policy |
| Wrong operation sequence | Type-State pattern | 4 | ✅ Compile-time |
| Accidental mutation | `const` | 5 | ✅ Compile-time |
| Null pointer dereference | `T&` / `std::optional` | 6 | ✅ Compile-time |
| Ignored return value | `[[nodiscard]]` | 7 | ✅ Compile-time |
| Wrong template type | Concepts / SFINAE | 8 | ✅ Compile-time |
| Incomplete builder | Type accumulation | 10 | ✅ Compile-time |
| Unit confusion | `mp-units` / Boost.Units | 11 | ✅ Compile-time |

---

## Quick Patterns

### Strong Typedef
```cpp
using UserId = fat_p::StrongId<struct UserTag>;
using DocId = fat_p::StrongId<struct DocTag>;
void access(UserId u, DocId d);  // Can't swap arguments
```

### Exhaustive Enum
```cpp
enum class Status { Pending, Done, Failed, COUNT_ };
// Compile with -Werror=switch-enum
switch (status) {
    case Status::Pending: ...
    case Status::Done: ...
    case Status::Failed: ...
    // No default! Adding enum value → compile error
}
```

### Non-Null Parameter
```cpp
void process(const Config& cfg);  // Reference: can't be null
void process(gsl::not_null<Config*> cfg);  // Pointer: enforced non-null
```

### Optional Return
```cpp
std::optional<User> find_user(int id);  // Might not find
Expected<User, Error> get_user(int id); // Success or error
```

### Nodiscard Function
```cpp
[[nodiscard]] bool save(const std::string& path);
save("file.txt");  // Warning: ignoring return value
```

### Nodiscard Type
```cpp
class [[nodiscard]] ErrorCode { int code_; };
ErrorCode validate();  // Automatically [[nodiscard]]
```

### Concept Constraint
```cpp
template<std::integral T>  // C++20
T safe_add(T a, T b);

safe_add(1.0, 2.0);  // Error: double doesn't satisfy integral
```

### Const Correctness
```cpp
void read_data(const std::vector<int>& v);   // Won't modify
void modify_data(std::vector<int>& v);       // Will modify
double get_value() const;                     // Won't modify *this
```

---

## Compiler Flags Cheat Sheet

### GCC/Clang — Essential

```bash
-Werror=return-type     # Missing return → error
-Werror=switch-enum     # Missing enum case → error
-Wconversion            # Narrowing conversion → warning
-Wsign-conversion       # Sign mismatch → warning
```

### GCC/Clang — Recommended

```bash
-Wall -Wextra -Wpedantic
-Wnon-virtual-dtor
-Woverloaded-virtual
-Wnull-dereference
-Wunused-result         # Ignoring [[nodiscard]]
```

### MSVC

```
/W4                     # High warning level
/WX                     # Warnings as errors
/we4062                 # Missing enum case → error
/we4715                 # Missing return → error
/we4834                 # Ignoring [[nodiscard]] → error
```

### CMake

```cmake
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Werror=return-type -Werror=switch-enum)
elseif(MSVC)
    add_compile_options(/W4 /we4062 /we4715)
endif()
```

---

## FAT-P Component Quick Reference

| Component | Purpose | Header |
|-----------|---------|--------|
| `StrongId<Tag, T>` | Type-safe IDs | `StrongId.h` |
| `EnumPlusMap<E, T>` | Compile-time enum-to-value map | `EnumPlus.h` |
| `StateMachine<...>` | Type-safe state machine | `StateMachine.h` |
| `Expected<T, E>` | Error-or-value result | `Expected.h` |
| `EnforcedInit<T>` | Must-initialize wrapper | `EnforcedInit.h` |
| `CheckedArithmetic<T>` | Overflow-safe math | `CheckedArithmetic.h` |

---

## Decision Quick Guide

**"Should I use a pointer or reference?"**
- Reference `T&` if null is never valid
- `gsl::not_null<T*>` if you need pointer semantics but never null
- `T*` only if null is a valid, documented state

**"Should I use optional or Expected?"**
- `std::optional<T>` if absence is normal (not an error)
- `Expected<T, E>` if absence means something went wrong

**"Should I use exceptions or Expected?"**
- Exceptions if errors propagate multiple levels
- Expected if caller should handle immediately
- Expected in async/coroutine code

**"Should I add [[nodiscard]]?"**
- Yes: error indicators, factory functions, computed values
- No: chainable setters, side-effect functions

**"Should I use SFINAE or concepts?"**
- Concepts if you're on C++20+
- SFINAE if you need C++17 compatibility
- Concepts are more readable and give better errors

---

## One-Liners to Remember

> **"Make illegal states unrepresentable."**  
> — The core principle of type-safe design

> **"If it compiles, it's (more likely) correct."**  
> — The goal of compile-time checking

> **"Types > tests > discipline."**  
> — The hierarchy of bug prevention

> **"The compiler never forgets; humans always do."**  
> — Why type safety beats code review

> **"Push validation to the boundary."**  
> — Check once at entry, trust internally

> **"If null is not valid, use a type that can't be null."**  
> — The reference principle

> **"If ignoring is a bug, mark it [[nodiscard]]."**  
> — The return value principle

---

## Guarantee Legend

| Mark | Meaning |
|------|---------|
| ✅ **Compile-time** | Invalid code does not compile |
| ⚠ **Runtime fail-fast** | Error detected early (throw/assert) |
| 🛈 **Discipline** | Convention helps, compiler doesn't enforce |

---

## See Also

| Document | Content |
|----------|---------|
| `problem_session_01.md` – `problem_session_11.md` | Full technique tutorials |
| `compile_time_error_detection_overview.md` | Philosophy and roadmap |
| `compiler_flags_reference.md` | Complete flag documentation |
| `Handbook_-_Discipline_of_Class_Design.md` | Design principles |

---

*FAT-P Library — Compile-Time Safety Quick Reference v1.0*
