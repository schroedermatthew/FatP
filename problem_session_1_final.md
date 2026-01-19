# Problem-Solving Session 1: The Wrong ID
## Facilitator Guide with Answers

---

## The Bug

This code shipped. It passed code review. It has a bug.

```c
typedef int UserId;
typedef int DocumentId;
typedef int PermissionLevel;

bool can_access(UserId user, DocumentId doc, PermissionLevel level) {
    // ... checks permission database ...
}

// Somewhere in the codebase:
UserId requester = get_current_user();
DocumentId target = get_requested_document();
PermissionLevel required = PERMISSION_READ;

if (can_access(target, requester, required)) {   // <-- spot it?
    serve_document(target);
}
```

---

### Question 1: What's the bug? Would the compiler warn?

**Answer:**

The arguments are swapped: `target` (a DocumentId) is passed as the first argument where `user` (a UserId) is expected, and `requester` (a UserId) is passed where `doc` (a DocumentId) is expected.

```c
can_access(target, requester, required)
//         ^^^^^^  ^^^^^^^^^
//         DocumentId passed as UserId
//                  UserId passed as DocumentId
```

**The compiler will not warn.** The `typedef` keyword creates an *alias*, not a new type. To the compiler, `UserId`, `DocumentId`, and `int` are all identical. This code:

```c
typedef int UserId;
typedef int DocumentId;
```

is equivalent to:

```c
// UserId IS int
// DocumentId IS int
// They are interchangeable everywhere
```

So when the compiler sees `can_access(target, requester, required)`, it sees `can_access(int, int, int)` which matches the signature perfectly.

**Key insight:** `typedef` provides documentation for humans, not safety from the compiler.

---

### Question 2: Would any of these help?

**Better variable names:**
- *Partially.* The names `requester` and `target` are already pretty good. The bug exists despite reasonable naming. You could argue for `requester_user_id` and `target_document_id`, but that gets verbose and people stop reading long names.
- **Verdict:** Helps marginally. Doesn't prevent the bug.

**Code review checklist:**
- *In theory.* A checklist item like "verify argument order for all function calls" would catch this—if reviewers actually checked every call site.
- *In practice.* Humans are bad at this. We read what we expect to see. The reviewer's brain autocompletes "of course the user comes first."
- **Verdict:** Unreliable. Doesn't scale.

**Unit tests:**
- *Only if you test the specific wrong case.* A test that passes valid `(user=1, doc=2)` won't catch that `(user=2, doc=1)` also "works" (because both are just ints that happen to exist).
- You'd need a test specifically checking that user 1 can't access doc 1 by passing them backwards—but why would you write that test if you didn't already know about the bug?
- **Verdict:** Unlikely to catch it unless you're already paranoid about this class of bug.

**Static analysis:**
- *Some tools try.* Clang-tidy has `bugprone-argument-comment` which would flag this if you used `/*user=*/` comments:
  ```c
  can_access(/*user=*/target, /*doc=*/requester, /*level=*/required);  // Warning!
  ```
- But this requires discipline to add comments everywhere, and most codebases don't.
- **Verdict:** Possible, but requires opt-in discipline.

**Coverity's SWAPPED_ARGUMENTS checker:**
- *A real near-miss.* MongoDB bug SERVER-30569 was caught by Coverity's `SWAPPED_ARGUMENTS` checker before it shipped. The checker works by comparing variable names at the call site against parameter names in the function declaration:
  ```c
  int update(int value, int number);
  
  int number = 2;
  int value = 0;
  update(number, value);  // Coverity warns: names suggest these are swapped
  ```
- The checker flagged the MongoDB bug because the variable names didn't match the parameter order—a name-matching heuristic, not type analysis.
- **Limitations:** Only works when variable names happen to match parameter names. If your variables are named `x` and `y`, or use different conventions than the function, the checker is blind. It's also prone to false positives when names legitimately differ.
- **Verdict:** Caught a real bug, but relies on naming discipline across the entire codebase. Partial coverage at best.

**The real answer:** The only reliable fix is to make the compiler reject incorrect code. That requires distinct types, not aliases. Every discipline-based approach—comments, naming conventions, checklists, heuristic checkers—requires effort at every call site and fails silently when that discipline lapses.

---

## Practical Adoption Note: Interop Boundaries

Strong types work best when they are **ubiquitous inside your codebase** and **explicit at the boundaries**.

**Typical boundaries where you unwrap/wrap:**
- serialization/deserialization (JSON/CBOR/protobuf)
- DB APIs and ORM layers
- OS / syscalls / file descriptors / socket APIs
- logging and text formatting

**Rule of thumb:** unwrap *once* at the boundary, and re-wrap immediately when you enter type-safe code again. Avoid “raw int plumbing” through application logic.

---

### Question 3: Why does the Android fdsan problem exist?

**The Android fdsan scenario:**

```
Thread 1: fd = open("/dev/null") → gets fd 123
Thread 1: close(fd)              → fd 123 is now free
Thread 2: fd2 = open("log")      → gets fd 123 (reused!)
Thread 1: close(fd)              → closes 123 again (DOUBLE CLOSE)
Thread 2: write(fd2, ...)        → fd2 (123) is now invalid or points to something else
```

**Why this happens:**

File descriptors are just `int`. The kernel sees `123`—it doesn't know whether that integer represents "the /dev/null handle Thread 1 opened" or "the log file Thread 2 opened." The number has no semantic meaning attached to it.

This is the same fundamental issue as the Wrong ID bug, just at a different layer:

| The Wrong ID Bug | The fdsan Bug |
|------------------|---------------|
| `UserId` and `DocumentId` are both `int` | All file descriptors are just `int` |
| Compiler can't distinguish them | Kernel can't distinguish them |
| Swapping arguments compiles fine | Closing the wrong fd "works" |
| Wrong data is accessed | Wrong file is accessed/corrupted |

In both cases:
1. An integer is used to represent a resource
2. The integer carries no semantic information about *which kind* of resource
3. Misuse is silent—no crash, no error, just wrong behavior
4. The consequences can be catastrophic (wrong user gets access / wrong file gets written)

**The deeper point:** Integers are semantically meaningless. The compiler sees `42`—it doesn't know if that's a user ID, a document ID, a file descriptor, or a shoe size. The fdsan problem exists because the POSIX API made file descriptors plain integers decades ago, and now billions of lines of code depend on that decision.

---

## A Broken Fix

Here's one attempt:

```cpp
struct UserId { int value; };
struct DocumentId { int value; };

bool can_access(UserId user, DocumentId doc, PermissionLevel level);

// Now this won't compile:
can_access(target, requester, required);  // ERROR: DocumentId vs UserId
```

---

### Question 4: What's wrong with this fix?

**Answer:**

The struct approach creates distinct types (good!), but it's missing essential operations:

**Problem 1: No comparison operators**
```cpp
UserId a{1};
UserId b{1};
if (a == b) { ... }  // ERROR: no operator==
```

You'd have to write:
```cpp
if (a.value == b.value) { ... }  // Ugly, defeats the purpose
```

**Problem 2: Can't use as map key**
```cpp
std::map<UserId, std::string> userNames;  // ERROR: no operator<
```

**Problem 3: Can't hash it**
```cpp
std::unordered_map<UserId, std::string> userNames;  // ERROR: no std::hash<UserId>
```

**Problem 4: Implicit conversion from braces**
```cpp
UserId id = {42};      // OK, aggregate initialization
UserId id2 = 42;       // Might work depending on compiler/flags (narrowing)
void foo(UserId u);
foo({42});             // Compiles! Implicit conversion from int
```

That last one is subtle but dangerous—you can still accidentally pass a raw integer.

**To make this work properly, you'd need:**
```cpp
struct UserId { 
    int value; 
    
    explicit UserId(int v) : value(v) {}
    
    bool operator==(UserId other) const { return value == other.value; }
    bool operator!=(UserId other) const { return value != other.value; }
    bool operator<(UserId other) const { return value < other.value; }
    // ... and hash specialization ...
};
```

Now multiply this by every ID type in your codebase. This is why templates exist.

---

## The Fix: Strong Typedefs

The pattern is simple: wrap the underlying type in a template parameterized by a tag type. This is the approach used by Fat-P's `StrongId`:

```cpp
template<typename Tag>
class StrongId {
    int value_;
public:
    explicit StrongId(int v) : value_(v) {}
    int raw() const { return value_; }
    
    bool operator==(StrongId other) const { return value_ == other.value_; }
    bool operator<(StrongId other) const { return value_ < other.value_; }
};

struct UserTag {};
struct DocumentTag {};

using UserId = StrongId<UserTag>;
using DocumentId = StrongId<DocumentTag>;
```

The actual Fat-P implementation adds validation policies, overflow checking, and `std::hash` support—but the core idea is this simple.

---

### Question 5: Why `explicit`? What would happen without it?

**Answer:**

The `explicit` keyword prevents implicit conversions from `int` to `StrongId`.

**Without `explicit`:**
```cpp
template<typename Tag>
class StrongId {
    int value_;
public:
    StrongId(int v) : value_(v) {}  // No explicit
    // ...
};

void process(UserId user);

process(42);  // COMPILES! Implicit conversion from int to UserId
```

This defeats the entire purpose. You wanted to prevent accidentally passing the wrong integer, but now any integer silently converts to any StrongId.

**With `explicit`:**
```cpp
void process(UserId user);

process(42);              // ERROR: no implicit conversion
process(UserId{42});      // OK: explicit construction
process(UserId(42));      // OK: explicit construction
```

The programmer must explicitly state "yes, I mean to create a UserId from this integer." This is the point of friction that catches bugs.

**The general rule:** Single-argument constructors should almost always be `explicit` unless you genuinely want implicit conversion (rare).

---

### Question 6: What's the runtime cost?

**Answer: Zero (for basic operations).**

From Fat-P StrongId benchmarks comparing implementations:

```
CONSTRUCTION (from int)
  fat_p::StrongId (Unchecked)   0.10 ns   1.00x vs raw int
  fluent::NamedType             0.10 ns   1.01x
  ts::strong_typedef            0.10 ns   0.99x
  boost::strong_typedef         0.10 ns   1.00x
  Raw int (baseline)            0.10 ns   1.00x

COMPARISON (operator<)
  fat_p::StrongId               0.37 ns   1.00x vs raw int
  All other libraries           0.37 ns   1.00x
  Raw int (baseline)            0.37 ns   1.00x

HASH (std::hash)
  All implementations           0.59 ns   1.00x vs raw int
```

**Why zero overhead?**

1. `StrongId` contains a single `int` member—same size, same alignment as `int`
2. All methods are trivial and inline completely
3. The `Tag` type exists only at compile time—no runtime representation
4. The compiler sees through the abstraction

**This is the zero-overhead abstraction principle:** You get compile-time safety with no runtime cost. The wrapper exists only in the type system; it evaporates during compilation.

**One exception:** Checked arithmetic operations (overflow detection) do add overhead:

```
INCREMENT (prefix ++) with overflow checking
  fat_p::StrongId (Checked)     1.86 ns   10.0x vs raw int
  fat_p::StrongId (Unchecked)   0.19 ns   1.00x
```

This is the cost of runtime safety checks—a tradeoff you choose explicitly.

---

## Strong Typedef Libraries

This course uses Fat-P's `StrongId`, but several alternatives exist. All achieve the core goal—compile-time type distinction with zero overhead for basic operations.

### Feature Comparison

| Feature | Fat-P StrongId | fluent::NamedType | type_safe | Boost.StrongTypedef |
|---------|----------------|-------------------|-----------|---------------------|
| **Style** | Template | Template | Template | Macro |
| **Header-only** | ✓ | ✓ | ✓ | ✓ (Boost headers) |
| **Zero overhead** | ✓ | ✓ | ✓ | ✓ |
| **Validation policies** | Built-in (positive, non-zero, range) | Manual | Manual | None |
| **Overflow checking** | Policy-based | None | None | None |
| **std::hash support** | ✓ | ✓ (opt-in) | ✓ (opt-in) | ✓ |
| **Atomic wrapper** | ✓ | None | None | None |
| **Expected integration** | ✓ | None | None | None |
| **C++ standard** | C++17 | C++14 | C++11 | C++03 |
| **External dependencies** | None | None | None | Boost |

### When to Choose What

**Fat-P StrongId:** You want validation policies (must be positive, non-zero, in range) or overflow-checked arithmetic out of the box. You're already using Fat-P components.

**fluent::NamedType:** Broader feature set including "skills" (Addable, Printable, etc.) and implicit conversion control. Good documentation. Jonathan Boccara's library.

**type_safe (ts::strong_typedef):** Part of a larger type safety toolkit that includes constrained integers and optional references. Also supports dimensional analysis if you need units. Jonathan Müller's library.

**Boost.StrongTypedef:** Macro-based, older style, but battle-tested across many codebases. Good choice if you're already using Boost and want consistency.

### Example Syntax

**Fat-P StrongId:**
```cpp
#include <fat_p/StrongId.h>

struct UserTag {};
struct DocumentTag {};

using UserId = fat_p::StrongId<int, UserTag>;
using DocumentId = fat_p::StrongId<int, DocumentTag>;

// With validation: must be positive
using PositiveId = fat_p::StrongId<int, UserTag, fat_p::strong_id::PositiveCheckPolicy>;
```

**fluent::NamedType:**

```cpp
#include <NamedType/named_type.hpp>

using UserId = fluent::NamedType<int, struct UserTag, 
    fluent::Comparable, fluent::Hashable>;
using DocumentId = fluent::NamedType<int, struct DocumentTag,
    fluent::Comparable, fluent::Hashable>;
```

**type_safe:**

```cpp
#include <type_safe/strong_typedef.hpp>

struct UserId : type_safe::strong_typedef<UserId, int>,
                type_safe::strong_typedef_op::equality_comparison<UserId>,
                type_safe::strong_typedef_op::relational_comparison<UserId>
{
    using strong_typedef::strong_typedef;
};
```

---

## Discussion Points

### "Where else in your codebase do you have `typedef int SomethingId`?"

Common places to look:
- Database row IDs
- API request/response IDs
- Session tokens (if numeric)
- Hardware register addresses
- Array indices into different arrays
- Handles to different subsystems

### "How many argument-order bugs have you seen or written?"

This is a great war-story prompt. Common examples:
- `memcpy(src, dst, size)` vs `memcpy(dst, src, size)` — everyone gets this wrong
- Rectangle constructors: `Rect(x, y, width, height)` vs `Rect(left, top, right, bottom)`
- Date constructors: `Date(year, month, day)` vs `Date(month, day, year)`
- `strcmp(a, b)` return value confusion

### "When is this overkill?"

StrongId might be overkill when:
- The ID is purely local to one function
- You're prototyping and will throw the code away
- You're writing a tiny script, not production code
- The ID type is used in exactly one place (no chance of confusion)
- Interop with C code that can't understand the wrapper

StrongId is worth it when:
- IDs cross API boundaries
- Multiple ID types exist in the same codebase
- The bug would be costly (security, data corruption)
- The codebase will be maintained for years

---

## Summary of Key Points

1. **`typedef` is not type safety** — it's documentation that the compiler ignores

2. **Integer handles are a class of bug** — file descriptors, database IDs, user IDs, they all have the same problem

3. **The fix is zero-cost** — templates + empty tag types give compile-time checking with no runtime overhead

4. **`explicit` is essential** — without it, implicit conversions defeat the purpose

5. **Multiple libraries exist** — Fat-P StrongId, fluent::NamedType, type_safe, Boost.StrongTypedef all solve this problem; choose based on your needs for validation, dependencies, and C++ standard

---

## Further Reading

From your materials:
- `Migration Guide - Integer Handles to StrongId.md` — full implementation details
- `Handbook - Discipline_of_Class_Design.md`, Chapter 7: "Making Misuse Impossible"

External:
- Android fdsan documentation: https://android.googlesource.com/platform/bionic/+/master/docs/fdsan.md
- Jonathan Boccara's "Strong Types" series: https://www.fluentcpp.com/2016/12/08/strong-types-for-strong-interfaces/
- Jonathan Müller's type_safe library: https://github.com/foonathan/type_safe
