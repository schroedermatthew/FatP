# Compile-Time Error Detection: Overview
## Using the Compiler as Your Safety Net

---

## The Core Principle

Every bug caught at compile time is a bug that:
- Never reaches code review
- Never reaches testing
- Never reaches production
- Never wakes you up at 3am

The C++ type system can act like a proof assistant. When used well, compiling successfully becomes a *partial* proof that certain classes of bugs cannot exist in your program.

This bundle contains **Sessions 1–5** (Strong Typedefs, Enum Exhaustiveness, StateMachine, Type-State Pattern, Const Correctness). The overview also mentions Sessions 6–8 as *optional extensions*; those are **not included** in the current lecture bundle.

---

## Guarantee Legend (used throughout)

| Mark | Meaning |
|------|---------|
| ✅ **Compile-time** | Invalid code does not compile (the compiler rejects it). |
| ⚠ **Runtime fail-fast** | Invalid behavior is detected at runtime early (throw/assert/log), not “silently proceed.” |
| 🛈 **Discipline/tooling** | A convention, review practice, or tooling can help, but the compiler does not enforce it by itself. |

---

## The Techniques

### 1. Strong Typedefs (Session 1)
**Problem:** Semantically different values share the same type.

```cpp
typedef int UserId;
typedef int DocumentId;
// Compiler sees both as `int` — swapping arguments compiles fine
```

**Solution:** Create distinct types that the compiler treats as incompatible.

```cpp
using UserId = StrongId<UserTag>;
using DocumentId = StrongId<DocumentTag>;
// Swapping arguments → compile error
```

**What it catches:**
- Argument order bugs
- Mixing up IDs, handles, indices
- Unit confusion (meters vs. feet, seconds vs. milliseconds)

**Fat-P component:** `StrongId`

---

### 2. Exhaustive Pattern Matching (Session 2)
**Problem:** Adding a new enum value silently falls through existing switches.

```cpp
enum class Status { Pending, Shipped, Delivered };
// Six months later: add `Refunded`
// All switches with `default:` silently misbehave
```

**Solution:** Remove `default` cases; let the compiler enforce exhaustiveness.

```cpp
// With -Werror=switch-enum, adding a new value → compile error
// Every switch must be updated
```

**What it catches:**
- Forgotten cases when enums grow
- Silent fallthrough to wrong behavior
- Incomplete dispatching logic

**Compiler flags:** `-Wswitch-enum`, `-Werror=switch-enum`

---

### 3. Type-Safe State Machines (Session 3)
**Problem:** State machines implemented with switches have no compile-time validation of transitions.

```cpp
enum State { IDLE, CONNECTING, CONNECTED, DISCONNECTING };
State state = IDLE;

void handle(Event e) {
    switch (state) {
        case IDLE:
            if (e == CONNECT) state = CONNECTING;  // Valid
            break;
        case CONNECTING:
            if (e == SUCCESS) state = CONNECTED;
            // What about DISCONNECT during CONNECTING? Forgot to handle.
            break;
        // Entry/exit actions? Scattered. Forgotten.
    }
}
```

**Solution:** Use a type-safe state machine that encodes the *state set* and *transition table* at compile time, then validates each attempted transition from the current runtime state using a policy (fail-fast).

```cpp
// Define states as types
struct Idle     { void on_entry(Ctx&); void on_exit(Ctx&); };
struct Connecting { void on_entry(Ctx&); void on_exit(Ctx&); };
struct Connected  { void on_entry(Ctx&); void on_exit(Ctx&); };

// Define valid transitions
using Transitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connected, Idle>
>;

// StateMachine enforces them
using ConnSM = fat_p::StateMachine<
    Context, Transitions, 
    fat_p::StrictTransitionPolicy,  // Invalid transition → exception
    fat_p::NoExceptActionPolicy,    // Actions must be noexcept
    0, Idle, Connecting, Connected
>;

ConnSM sm(ctx);
sm.transition<Connecting>();  // Calls Idle::on_exit, Connecting::on_entry
sm.transition<Idle>();        // Runtime error: transition not in list!
```

**What it catches:**
- Invalid state transitions (at compile time or runtime, depending on policy)
- Forgotten entry/exit actions (automatic dispatch)
- Duplicate states (compile-time static_assert)
- Missing states in transition list (compile-time error)

**Fat-P component:** `StateMachine`

---

### 4. Type-State Pattern (Advanced)
**Problem:** Objects have invalid operation sequences that compile fine.

```cpp
File f;
f.write("data");  // Bug: file not opened yet
f.open("path");
f.write("data");  // OK
f.close();
f.write("more");  // Bug: file already closed
```

**Solution:** Encode the object's state in its type. Operations return a new type representing the new state.

```cpp
ClosedFile f;
OpenFile of = f.open("path");  // Returns different type
of.write("data");              // Only available on OpenFile
ClosedFile cf = of.close();    // Returns ClosedFile
cf.write("more");              // Compile error: no write() on ClosedFile
```

**What it catches:**
- Use-before-initialize
- Use-after-close/free
- Invalid operation sequences
- Protocol violations (send before connect)

**Note:** This is a more advanced pattern than `StateMachine`. Use `StateMachine` when you need runtime state with entry/exit actions; use the type-state pattern when you want compile-time enforcement of operation sequences.

---

### 5. Const Correctness (Session 5)
**Problem:** Functions modify data they shouldn't; data races in concurrent code.

```cpp
void process(std::vector<int>& data) {
    data.push_back(42);  // Surprise mutation!
}
```

**Solution:** Use `const` to declare intent. Compiler enforces immutability.

```cpp
void process(const std::vector<int>& data) {
    data.push_back(42);  // Compile error
}
```

**What it catches:**
- Accidental mutation
- API contract violations
- Some classes of concurrency mistakes by making read-only intent explicit (see clarification below)

**Important clarification:** `const` helps communicate/read-only intent and can enable safe sharing *when the underlying object is truly immutable*, but `const` alone does **not** make an object thread-safe (e.g., `mutable` caches, internal synchronization, shared ownership, or other side effects).

**Language features:** `const`, `constexpr`, `std::as_const`

---

### 6. Non-Null References (Optional extension; not included in this bundle)
**Problem:** Null pointer dereferences crash at runtime.

```cpp
void process(User* user) {
    user->name;  // Might crash if user is null
}
```

**Solution:** Use references (cannot be null) or `optional` (explicit nullability).

```cpp
void process(User& user) {
    user.name;  // Cannot be null by construction
}

void maybe_process(std::optional<User>& user) {
    if (user) user->name;  // Must check explicitly
}
```

**What it catches:**
- Null pointer dereference
- Forgetting to check for null
- Unclear nullable vs. non-nullable APIs

**Language features:** References, `std::optional`, `std::reference_wrapper`

---

### 7. [[nodiscard]] and Error Handling (Optional extension; not included in this bundle)
**Problem:** Error return values are silently ignored.

```cpp
std::filesystem::remove("file.txt");  // Returns bool, often ignored
// Did it succeed? Who knows.
```

**Solution:** Mark return values that must not be ignored.

```cpp
[[nodiscard]] bool remove(const path& p);

remove("file.txt");  // Warning: ignoring return value
```

**What it catches:**
- Ignored error codes
- Ignored important return values
- "Fire and forget" calls that shouldn't be

**Language features:** `[[nodiscard]]`, `std::expected` (C++23)

**Fat-P component:** `Expected` — a pre-C++23 implementation with additional features

---

### 8. Template Constraints (Optional extension; not included in this bundle)
**Problem:** Template errors are incomprehensible; wrong types produce pages of errors deep in implementation.

```cpp
template<typename T>
void sort(T& container) { ... }

sort(42);  // 50 lines of error messages from deep inside sort()
```

**Solution:** Constrain templates at the interface level — reject invalid types immediately with clear errors.

#### The C++11/17 Way: SFINAE and Type Traits

SFINAE ("Substitution Failure Is Not An Error") lets you enable/disable function overloads based on type properties:

```cpp
#include <type_traits>

// Only enabled for integral types
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
safe_add(T a, T b) {
    // Check for overflow...
    return a + b;
}

safe_add(1, 2);      // OK: int is integral
safe_add(1.0, 2.0);  // Compile error: no matching function
```

**Common type traits:**
- `std::is_integral`, `std::is_floating_point`, `std::is_arithmetic`
- `std::is_pointer`, `std::is_reference`, `std::is_const`
- `std::is_same`, `std::is_base_of`, `std::is_convertible`
- `std::is_default_constructible`, `std::is_copy_assignable`
- `std::has_virtual_destructor`, `std::is_trivially_copyable`

**SFINAE patterns:**

```cpp
// Pattern 1: Return type SFINAE
template<typename T>
std::enable_if_t<std::is_integral_v<T>, T>
process(T value);

// Pattern 2: Template parameter SFINAE
template<typename T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
void process(T value);

// Pattern 3: void_t detection idiom (C++17)
template<typename T, typename = void>
struct has_size : std::false_type {};

template<typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> 
    : std::true_type {};
```

#### The C++20 Way: Concepts

Concepts provide the same constraints with cleaner syntax and better error messages:

```cpp
// Define a concept
template<typename T>
concept Integral = std::is_integral_v<T>;

template<typename T>
concept Sortable = requires(T& t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

// Use it
template<Integral T>
T safe_add(T a, T b);

template<Sortable T>
void sort(T& container);

sort(42);  // Error: int does not satisfy Sortable
           // (clear, one-line error message)
```

#### Migration Path: SFINAE → Concepts

```cpp
// C++17 SFINAE
template<typename T, std::enable_if_t<
    std::is_integral_v<T> && sizeof(T) >= 4, int> = 0>
T safe_multiply(T a, T b);

// C++20 Concepts equivalent
template<typename T>
concept LargeIntegral = std::is_integral_v<T> && sizeof(T) >= 4;

template<LargeIntegral T>
T safe_multiply(T a, T b);
```

**What it catches:**
- Template instantiation with wrong types
- Missing required operations (methods, operators)
- Constraint violations with clear error messages
- Type mismatches at API boundaries (not deep in implementation)

**Language features:** 
- C++11/17: `std::enable_if`, `std::void_t`, type traits (`<type_traits>`)
- C++20: Concepts, `requires` clauses, `concept` definitions

**Fat-P component:** `FatPTypeTraits` — additional type trait utilities

---

## The Common Pattern

Every technique follows the same pattern:

| Without | With |
|---------|------|
| Runtime check (maybe) | Compile-time guarantee |
| Discipline required | Compiler enforced |
| Bug found in testing (maybe) | Bug found at compile time |
| Documentation says "don't do X" | Code makes X impossible |

The goal is always the same: **make illegal states unrepresentable**.

---

## Cost-Benefit Summary

| Technique | Compile-Time Cost | Runtime Cost | Adoption Effort |
|-----------|-------------------|--------------|-----------------|
| Strong Typedefs | Slightly longer compiles | Zero | Moderate (refactor call sites) |
| Exhaustive Switches | Negligible | Zero | Low (add compiler flag, fix warnings) |
| Type-Safe State Machines | Slightly longer compiles | O(1) transition validity check + entry/exit dispatch (often inlined) | Medium (restructure into state types) |
| Type-State Pattern | Longer compiles | Usually zero extra state (but design-dependent) | High (redesign APIs) |
| Const Correctness | Negligible | Zero | Moderate (add const throughout) |
| Non-Null References | Negligible | Zero | Low (prefer references) |
| [[nodiscard]] | Negligible | Zero | Low (add attributes) |
| SFINAE / Type Traits | Longer compiles | Zero | High (tricky syntax) |
| Concepts (C++20) | Longer compiles | Zero | Moderate (cleaner than SFINAE) |

Most techniques have **zero** runtime cost; when there *is* runtime work (e.g., a state machine transition check), it is typically **explicit, O(1), and fail-fast**, and should be benchmarked in the same way you would benchmark any other control-flow refactor.

The main costs are:
- Slightly longer compile times (usually negligible)
- Initial refactoring effort
- Learning curve

---

## When Is This Overkill?

These techniques aren't always worth the effort:

- **Prototypes and throwaway code:** Just get it working
- **Tiny scripts:** The ceremony exceeds the risk
- **Interop with C:** You may need raw types at boundaries
- **Performance-critical inner loops:** Occasionally, the compiler can't see through abstractions (rare, benchmark first)

They're almost always worth it for:

- **Long-lived codebases:** Bugs compound over years
- **Security-sensitive code:** Wrong ID = data breach
- **Safety-critical systems:** Wrong state = physical harm
- **Large teams:** Discipline doesn't scale; types do

---

## Session Roadmap

| Session | Technique | Fat-P Component | Key Compiler Feature |
|---------|-----------|-----------------|---------------------|
| 1 | Strong Typedefs | `StrongId` | Template tag types |
| 2 | Exhaustive Switches | `EnumPlusMap` | `-Werror=switch-enum` |
| 3 | Type-Safe State Machines | `StateMachine` | Template policies |
| 4 | Type-State Pattern | — | Move semantics, type transformation |
| 5 | Const Correctness | — | `const` |
| 6 | Non-Null References (optional) | — | References, `optional` |
| 7 | [[nodiscard]] and fail-fast results (optional) | `Expected` (or `std::expected`) | Attributes |
| 8 | Template Constraints (optional) | `FatPTypeTraits` (or standard traits) | SFINAE (C++17), Concepts (C++20) |

---

## Further Reading

**Books:**
- "Effective Modern C++" (Scott Meyers) — Items on smart pointers, move semantics
- "C++ Core Guidelines" (Stroustrup & Sutter) — Type safety section
- "Programming in Rust" — For comparison: a language that enforces these patterns by default

**Papers:**
- "Type-State Programming" (Strom & Yemini, 1986) — The original type-state paper
- "Typestates for Objects" (DeLine & Fähndrich, 2004) — Application to OOP

**Online:**
- Andrzej Krzemieński's blog on value semantics
- Jonathan Boccara's Fluent C++ on strong types
