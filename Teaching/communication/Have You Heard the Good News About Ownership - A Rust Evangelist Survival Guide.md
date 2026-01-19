# Have You Heard the Good News About Ownership?
## A Survival Guide for C++ Teams with Rust Evangelists

*"This would be a compile error in Rust."*

---

## Know Your Adversary

A new engineer has joined your C++ team. Let's call him Ferris. He's talented, enthusiastic, and mass: completely insufferable about Rust. He learned Rust two years ago, wrote a hobby project in it, and experienced what he describes as a "spiritual awakening." He now views all other languages through the lens of Rust's ownership model, and he finds them wanting.

Ferris doesn't just prefer Rust. He *believes* in Rust. The way some people believe in religion, Ferris believes in memory safety. He has seen the light of the borrow checker, and he has come to spread the gospel.

Ferris exhibits the following clinical symptoms:

**The Ownership Obsession.** Every code review becomes a lecture on ownership semantics. "Who owns this pointer? What's its lifetime? In Rust, the compiler would enforce this." The fact that your C++ code has worked correctly for 15 years is irrelevant. It's not *provably* safe. It could have bugs. The borrow checker would know.

```cpp
// Your code
Widget* get_widget(int id) {
    return &widgets_[id];
}

// Ferris's review comment:
// "What's the lifetime of the returned pointer? What if widgets_ is 
// resized? What if the Widget is deleted? In Rust, the borrow checker 
// would prevent use-after-free at compile time. Here we're just 
// hoping the caller knows what they're doing."
//
// (He's not wrong. He's also not helpful.)
```

**The `unsafe` Horror.** Ferris views C++ as one giant `unsafe` block. Every pointer dereference is a potential segfault. Every cast is undefined behavior waiting to happen. Every concurrent access is a data race. He walks through the codebase like a health inspector through a failing restaurant, shaking his head and making notes.

```cpp
// Your code: a perfectly reasonable C++ pattern
void process(const std::vector<int>& data) {
    for (size_t i = 0; i < data.size(); ++i) {
        handle(data[i]);
    }
}

// Ferris, muttering: "No bounds checking. data[i] could panic— 
// I mean, segfault. The iterator could be invalidated if handle() 
// modifies data. This is all unsafe. All of it."
```

**The Rewrite Reflex.** Every problem Ferris encounters, his solution is the same: "We should rewrite this in Rust." Memory leak? "Rust's ownership model prevents this." Data race? "Fearless concurrency." Confusing API? "Rust's type system would make this clearer." The Rust rewrite is always the answer, regardless of the question.

```
Ferris: "This module has a memory leak."
You: "Can you fix it?"
Ferris: "We should rewrite it in Rust."
You: "Can you fix the leak in C++?"
Ferris: "The fundamental problem is that C++ allows memory leaks."
You: "So that's a no?"
Ferris: "I'm saying if we used Rust—"
```

**The Result<T, E> Requirement.** Ferris has implemented `Result<T, E>` in C++ (or insists on using `std::expected`). He believes exceptions are "invisible control flow" and error codes are "forgettable." Every function must return a Result. Every caller must handle the error. The codebase must be a monument to explicit error handling.

```cpp
// Ferris's code
Result<Widget, Error> get_widget(int id) {
    if (id < 0 || id >= widgets_.size()) {
        return Err(Error::InvalidId);
    }
    return Ok(widgets_[id]);
}

// Caller
auto result = get_widget(id);
if (result.is_err()) {
    // Handle error
}
auto widget = result.unwrap();

// Your code (what he's replacing)
Widget& get_widget(int id) {
    return widgets_.at(id);  // Throws on invalid id
}

// "Exceptions are invisible GOTO. The caller can just... ignore them."
// (He's not wrong. But the code was three lines instead of twelve.)
```

**The Trait Testimony.** Ferris wants traits. C++ has concepts, but they're not the same. C++ has inheritance, but that's "the wrong kind of polymorphism." Ferris spends hours designing trait-based abstractions that could be a simple virtual function, because trait objects are "more explicit" than vtables.

**The Fearless Concurrency Fixation.** Any multithreaded code is an opportunity for Ferris to explain Rust's `Send` and `Sync` traits. "In Rust, the compiler prevents data races. Here, we're just hoping our mutexes are correct." He has opinions about `std::shared_ptr` (atomic reference counting overhead) and `std::mutex` (doesn't prevent deadlocks) and `std::atomic` (memory ordering is easy to get wrong).

```cpp
// Your code
std::mutex mtx;
std::vector<int> shared_data;

void worker() {
    std::lock_guard<std::mutex> lock(mtx);
    shared_data.push_back(42);
}

// Ferris: "What if someone accesses shared_data without the lock? 
// The compiler doesn't prevent it. In Rust, you literally can't 
// access the data without holding the lock. The API makes it 
// impossible to forget."
//
// (He's right. This is actually a good point.)
```

**The Cargo Comparison.** Ferris complains about CMake. He complains about header files. He complains about the lack of a standard package manager. "In Rust, I type `cargo build` and everything just works. Why do I need to configure three different build systems and manually resolve dependencies?"

This is the one area where everyone agrees with him. CMake is terrible. Nobody will defend CMake.

---

## The Uncomfortable Truth

Here's the thing about Ferris: **he's often right.**

Rust's ownership model really does prevent entire categories of bugs at compile time. The borrow checker really does catch use-after-free, double-free, and dangling references that C++ happily compiles. `Send` and `Sync` really do prevent data races in ways that C++ cannot.

The friction with Ferris isn't that he's wrong. It's that:

1. He presents these as reasons to rewrite everything, not as problems to address
2. He ignores the costs of migration (time, risk, training)
3. He dismisses modern C++ features that address many of his concerns
4. He's evangelical rather than collaborative

Your job is to:
- Acknowledge his valid concerns
- Show him how C++ addresses them (when it does)
- Explain why migration isn't always practical
- Learn from his perspective (because some of it is valuable)
- Maintain your sanity

---

## The Battles

### Battle #1: The Ownership Argument

**Ferris's Position**

"C++ has no ownership model. Any pointer could be owning or borrowing. Any reference could dangle. The compiler doesn't know and can't help. You're relying on documentation and discipline. Documentation lies and discipline fails."

```cpp
// Ferris's nightmare: C++ pointer soup
void process(Widget* w);           // Takes ownership? Borrows? Who knows!
Widget* create();                  // Caller owns? Shared? Leaked?
Widget* get(int id);              // Valid for how long?
void update(Widget* w, Data* d);  // Which outlives which?
```

**The Valid Kernel**

He's right that raw pointers have no ownership semantics. He's right that the compiler can't enforce lifetime rules. He's right that documentation can be wrong or ignored.

**The C++ Response**

Modern C++ encodes ownership in types:

```cpp
// Modern C++: ownership in the type system
void process(std::unique_ptr<Widget> w);    // Takes ownership
void process(Widget& w);                     // Borrows, must outlive call
void process(std::shared_ptr<Widget> w);    // Shares ownership
Widget* get(int id);                         // Borrows (raw ptr = non-owning)

std::unique_ptr<Widget> create();            // Caller owns (it's in the type!)
std::optional<Widget> maybe_create();        // Might not exist
std::shared_ptr<Widget> get_shared(int id);  // Shared lifetime
```

**The Honest Limitations**

C++ doesn't enforce borrow rules at compile time. You can still:
- Create dangling pointers
- Use-after-move
- Return references to locals
- Invalidate iterators

The compiler won't stop you. Sanitizers will catch some of these. Code review catches others. But Rust's compile-time guarantees are genuinely stronger.

**How to Respond to Ferris**

"You're right that Rust's ownership model is more rigorous. Here's our approach in C++:

1. We use smart pointers for ownership, raw pointers for borrowing
2. We follow the Core Guidelines (static analysis enforces many rules)
3. We run AddressSanitizer in CI to catch violations
4. We have ownership conventions documented in CONTRIBUTING.md

It's not as strong as Rust's compile-time checking, but it catches most bugs. Where specifically in our codebase are you concerned?"

**When Ferris Has a Point**

If your codebase has:
- Raw owning pointers everywhere
- Unclear lifetime documentation
- No static analysis
- History of use-after-free bugs

Then Ferris's concerns are valid. Fix those problems—with better C++, not necessarily with Rust.

---

### Battle #2: The Error Handling Debate

**Ferris's Position**

"Exceptions are invisible control flow. Any function can throw, and callers can just ignore it. Error codes get forgotten. `Result<T, E>` makes errors explicit—you can't use the value without handling the error."

```rust
// Rust: errors are explicit
fn get_widget(id: i32) -> Result<Widget, Error> {
    if id < 0 {
        return Err(Error::InvalidId);
    }
    Ok(widgets[id])
}

// Caller MUST handle the Result
let widget = get_widget(id)?;  // Propagates error
// or
let widget = get_widget(id).unwrap();  // Explicitly ignoring error
// or
match get_widget(id) {
    Ok(w) => { /* use w */ },
    Err(e) => { /* handle e */ },
}
```

**The Valid Kernel**

He's right that exceptions can be ignored. He's right that they create non-local control flow. He's right that error codes can be forgotten.

**The C++ Response**

C++23 `std::expected` gives you Rust-style error handling:

```cpp
// C++23: explicit errors
std::expected<Widget, Error> get_widget(int id) {
    if (id < 0) {
        return std::unexpected(Error::InvalidId);
    }
    return widgets_[id];
}

// Caller must handle
auto result = get_widget(id);
if (!result) {
    handle_error(result.error());
    return;
}
Widget& widget = *result;
```

Or use `[[nodiscard]]` to force handling:

```cpp
[[nodiscard]] ErrorCode process();

process();  // Warning: ignoring return value of function with 'nodiscard' attribute
```

**The Honest Limitations**

- `std::expected` is C++23 (not everyone has it yet)
- The standard library uses exceptions, so you'll have mixed styles
- Nothing forces callers to check `expected` values (unlike Rust's `#[must_use]`)
- Error propagation is verbose compared to Rust's `?` operator

**How to Respond to Ferris**

"We agree that explicit error handling is valuable. Our approach:

1. New code uses `std::expected` (or our `Expected<T, E>` backport)
2. `[[nodiscard]]` on functions whose errors matter
3. Exceptions at API boundaries, `expected` internally
4. Error handling documented in API docs

What specific code is concerning you? Let's see if `expected` would help."

---

### Battle #3: The Concurrency Confrontation

**Ferris's Position**

"C++ has no compile-time data race prevention. You can share mutable state between threads and the compiler won't stop you. In Rust, `Send` and `Sync` traits make thread safety part of the type system. You literally cannot send non-thread-safe data across threads."

```rust
// Rust: thread safety in the type system
use std::rc::Rc;  // Not thread-safe
use std::sync::Arc;  // Thread-safe

let rc = Rc::new(42);
std::thread::spawn(move || {
    println!("{}", rc);  // COMPILE ERROR: Rc doesn't implement Send
});

let arc = Arc::new(42);
std::thread::spawn(move || {
    println!("{}", arc);  // OK: Arc implements Send
});
```

**The Valid Kernel**

This is Rust's strongest argument. C++ has no equivalent to `Send` and `Sync`. The compiler cannot prevent data races. Thread safety is entirely the programmer's responsibility.

**The C++ Response**

C++ relies on tools and discipline:

```cpp
// Thread safety through API design
class ThreadSafeCounter {
public:
    void increment() {
        std::lock_guard lock(mutex_);
        ++count_;
    }
    
    int get() const {
        std::lock_guard lock(mutex_);
        return count_;
    }
    
private:
    mutable std::mutex mutex_;
    int count_ = 0;
};

// Or use atomics
std::atomic<int> count{0};
count.fetch_add(1, std::memory_order_relaxed);
```

We can't prevent data races at compile time, but we can:
- Design thread-safe APIs
- Use ThreadSanitizer in CI
- Follow patterns (lock guards, atomics, message passing)
- Review concurrent code carefully

**The Honest Limitations**

There's no way to make C++ as safe as Rust for concurrency. The compiler cannot help you. If you need guaranteed freedom from data races, Rust is genuinely better.

**How to Respond to Ferris**

"You're right that Rust's concurrency guarantees are stronger. We can't match them in C++. Our mitigations:

1. ThreadSanitizer catches data races in testing
2. We use Clang's thread safety annotations where possible
3. Concurrent code requires extra review scrutiny
4. We prefer message passing over shared state

For new concurrent code, we could consider Rust. Which specific code concerns you?"

---

### Battle #4: The Memory Safety Manifesto

**Ferris's Position**

"C++ has buffer overflows, use-after-free, double-free, null pointer dereferences, uninitialized memory. Rust prevents ALL of these at compile time. Every hour spent debugging these in C++ is an hour wasted—the bugs wouldn't exist in Rust."

**The Valid Kernel**

Rust really does prevent these at compile time. C++ really does have these bugs. We really do spend hours debugging them.

**The C++ Response**

Modern C++ mitigates (but doesn't eliminate) these:

```cpp
// Buffer overflows
std::vector<int> v = {1, 2, 3};
v.at(5);  // Throws, doesn't overflow (unlike v[5])
std::span<int> s(v);  // Bounded view

// Use-after-free: smart pointers
auto widget = std::make_unique<Widget>();
process(widget.get());  // Can still dangle if process stores the pointer
// But ownership is clear, and the common case is safe

// Double-free: smart pointers
// unique_ptr can't be double-freed
// shared_ptr tracks references

// Null pointers
std::optional<Widget> opt;
if (opt) { use(*opt); }  // Explicit null handling

// Uninitialized memory
std::vector<int> v(100);  // Zero-initialized
std::array<int, 100> a{};  // Zero-initialized with {}
```

**The Honest Limitations**

- `.at()` is slower than `[]` (bounds checking has cost)
- Smart pointers don't prevent all lifetime bugs
- `optional` requires discipline to check
- Legacy code still has raw patterns
- Sanitizers catch bugs at runtime, not compile time

**How to Respond to Ferris**

"We can't achieve Rust's compile-time guarantees. Here's what we do:

1. Static analysis (clang-tidy, PVS-Studio) catches many issues
2. Sanitizers (ASan, UBSan, MSan) catch runtime violations
3. Modern idioms (smart pointers, optional, span) reduce attack surface
4. Security-critical code gets extra scrutiny

We accept that Rust would be safer. The question is whether the migration cost justifies the safety benefit for this codebase."

---

### Battle #5: The Build System Brawl

**Ferris's Position**

"Cargo is a revelation. One command to build. One command to test. Dependencies just work. Why does C++ have CMake, Make, Ninja, Meson, Bazel, SCons, and they ALL require manual configuration?"

```bash
# Rust
cargo build
cargo test

# C++
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_PREFIX_PATH=/opt/dependencies \
    -DENABLE_TESTS=ON \
    -DWITH_OPENSSL=ON \
    ...
cmake --build build
ctest --test-dir build
```

**The Valid Kernel**

Cargo is better than CMake. Everyone knows this. Nobody defends CMake from passion. We defend it from necessity.

**The C++ Response**

There is no good response. CMake is pain. We cope:

```cmake
# CMakeLists.txt - hide the suffering
cmake_minimum_required(VERSION 3.20)
project(myproject)

# vcpkg/Conan for dependencies (it's not Cargo, but it helps)
find_package(fmt REQUIRED)
find_package(Catch2 REQUIRED)

add_executable(myproject main.cpp)
target_link_libraries(myproject PRIVATE fmt::fmt)

# Make it as simple as possible for developers
# cmake --preset default && cmake --build --preset default
```

**The Honest Limitations**

C++ has no standard package manager. CMake is complex. Build times are long. Header files are tedious. There's no good defense.

**How to Respond to Ferris**

"You're right. Cargo is better. We use CMake because:

1. It's the de facto standard for C++
2. Our dependencies use it
3. Everyone knows it (for better or worse)
4. We've invested in presets and scripts to simplify it

If you want to improve the build experience, you'd be a hero. Just don't propose rewriting in Rust to fix the build system."

---

### Battle #6: The Rewrite Resistance

**Ferris's Position**

"Look, I've identified 47 memory safety issues, 12 potential data races, and uncountable opportunities for bugs. The fundamental problem is C++. We should rewrite in Rust."

**The Response**

This is where you must be firm.

"Here's why we're not rewriting in Rust:

**Time:** This codebase is 500,000 lines. A rewrite is 2-3 years of work. That's 2-3 years of no new features, no bug fixes, no competitive development.

**Risk:** Rewrites fail. The second-system effect is real. We'd introduce new bugs replacing old bugs. The old system's quirks are often load-bearing.

**Knowledge:** The team knows C++. Learning Rust well enough to write production code takes 6-12 months. During that time, we'd be writing bad Rust instead of good C++.

**Dependencies:** We depend on 47 C++ libraries. Many don't have Rust equivalents. We'd write FFI bindings, which are unsafe anyway.

**Interop:** We expose a C API to customers. That doesn't change. The safety benefits of Rust disappear at the C boundary.

**Hiring:** We can hire C++ developers. Rust developers are scarcer and more expensive.

What we CAN do:
1. Write new isolated components in Rust (if they have clean boundaries)
2. Modernize the C++ we have
3. Add tooling (sanitizers, static analysis) to catch bugs
4. Address specific issues you've identified—in C++

Now, which of those 47 memory safety issues should we fix first?"

---

### Battle #7: The Incremental Introduction

**Ferris's Position**

"Fine, we won't rewrite everything. But let's write new code in Rust. We can use FFI to integrate with the existing C++."

**The Valid Kernel**

This is actually reasonable. Rust can interoperate with C/C++. New components with clear boundaries could be written in Rust.

**The Considerations**

```
┌─────────────────────────────────────────────────────────────┐
│                     RUST-C++ INTEROP                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────┐     FFI      ┌─────────────────┐      │
│  │   Rust Code     │◄────────────►│   C++ Code      │      │
│  │                 │   (unsafe)   │                 │      │
│  │  • Type safe    │              │  • Existing     │      │
│  │  • Memory safe  │              │  • Tested       │      │
│  │  • New features │              │  • Known        │      │
│  └─────────────────┘              └─────────────────┘      │
│                                                             │
│  Challenges:                                                │
│  • FFI is unsafe (Rust safety guarantees don't cross)      │
│  • C++ objects are hard to represent in Rust               │
│  • Build system complexity increases                        │
│  • Debugging across boundary is painful                     │
│  • Team now needs to know two languages                     │
│                                                             │
│  Good candidates for Rust:                                  │
│  • New, isolated components                                 │
│  • Clear C API boundary                                     │
│  • High security requirements                               │
│  • Concurrent/parallel code                                 │
│                                                             │
│  Poor candidates:                                           │
│  • Deep integration with existing C++ classes              │
│  • Heavy use of C++ templates                               │
│  • Performance-critical (FFI has overhead)                  │
│  • Small utilities (overhead not worth it)                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**How to Respond to Ferris**

"Let's evaluate this seriously. For Rust to make sense for a new component:

1. **Isolation:** Does it have a clean C-style interface?
2. **Size:** Is it big enough to justify the overhead?
3. **Security:** Does it handle untrusted input?
4. **Concurrency:** Does it have complex threading?
5. **Maintenance:** Will the team maintain two languages long-term?

If the answer is 'yes' to most of these, let's prototype it. If not, let's use modern C++ and address your concerns with tooling.

What component were you thinking?"

---

## The Techniques That Actually Help

Ferris isn't entirely wrong. Here's how to address his concerns legitimately:

### Technique #1: Clang Thread Safety Annotations

```cpp
// Compile-time thread safety checking (not as good as Rust, but something)
#include <mutex>

class [[clang::capability("mutex")]] Mutex {
public:
    void lock() [[clang::acquire_capability()]] { impl_.lock(); }
    void unlock() [[clang::release_capability()]] { impl_.unlock(); }
private:
    std::mutex impl_;
};

class [[clang::guarded_by(mutex_)]] Data {
public:
    void update(int value) [[clang::requires_capability(mutex_)]] {
        value_ = value;  // Compiler verifies mutex is held
    }
    
private:
    Mutex mutex_;
    int value_;
};
```

### Technique #2: Sanitizers in CI

```yaml
# .github/workflows/ci.yml
jobs:
  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, undefined, thread, memory]
    steps:
      - run: |
          cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=${{ matrix.sanitizer }}"
          cmake --build build
          ctest --test-dir build
```

### Technique #3: Lifetime Annotations (C++ Core Guidelines)

```cpp
// Use gsl::not_null for non-nullable pointers
#include <gsl/gsl>

void process(gsl::not_null<Widget*> w);  // Can't be null, crashes if violated

// Use owner<T> to mark owning pointers
gsl::owner<Widget*> create();  // Caller must delete

// Static analysis (clang-tidy) enforces these
```

### Technique #4: std::expected for Rust-style Errors

```cpp
// C++23 std::expected, or use tl::expected for older standards
#include <expected>

std::expected<Widget, Error> get_widget(int id) {
    if (id < 0) return std::unexpected(Error::InvalidId);
    if (id >= size_) return std::unexpected(Error::NotFound);
    return widgets_[id];
}

// Monadic operations (C++23)
auto result = get_widget(id)
    .and_then([](Widget& w) { return w.validate(); })
    .transform([](Widget& w) { return w.name(); });
```

### Technique #5: Compile-Time Bounds Checking

```cpp
// std::span with static extent
template<size_t N>
void process(std::span<int, N> data) {
    // Compiler knows size at compile time
    // Many bounds checks can be elided
}

// Safe indexing
std::vector<int> v = {1, 2, 3};
auto first = v.at(0);  // Bounds-checked
auto oops = v.at(5);   // Throws std::out_of_range
```

---

## Psychological Survival Strategies

### Strategy #1: The Validation Valve

Ferris needs to feel heard. His concerns are valid. Acknowledge them.

"You're right that Rust would prevent this class of bug at compile time. C++ requires us to use tools and discipline instead. It's a tradeoff we've made for other reasons. What specific concern do you have here?"

Validation defuses evangelism. Ferris isn't evil—he's experienced a better way and wants to share it. Acknowledge the better way. Then explain why you're not taking it.

### Strategy #2: The Concrete Challenge

Abstract complaints about C++ being "unsafe" are hard to address. Concrete bugs are actionable.

"You've mentioned memory safety concerns several times. Can you identify a specific bug in our codebase? A specific pattern you're worried about? Let's fix that."

This channels Ferris's energy into improvement. He's now finding bugs (useful) rather than complaining about the language (not useful).

### Strategy #3: The Learning Opportunity

Ferris knows things about memory safety, concurrency, and API design that he learned from Rust. This knowledge is valuable even in C++.

"You mentioned that Rust's mutex holds the data it protects. Can you show me how we could design our C++ API to make misuse harder?"

```cpp
// Ferris's Rust-inspired design
template<typename T>
class Mutex {
public:
    class Guard {
    public:
        T& operator*() { return mutex_->data_; }
        T* operator->() { return &mutex_->data_; }
        ~Guard() { mutex_->unlock(); }
    private:
        friend class Mutex;
        Guard(Mutex* m) : mutex_(m) { mutex_->lock(); }
        Mutex* mutex_;
    };
    
    Guard lock() { return Guard(this); }
    
private:
    void lock() { impl_.lock(); }
    void unlock() { impl_.unlock(); }
    
    std::mutex impl_;
    T data_;
};

// Usage: can't access data without lock
Mutex<std::vector<int>> protected_data;

{
    auto guard = protected_data.lock();
    guard->push_back(42);  // Must hold lock to access
}  // Automatically unlocked
```

Ferris designed this. He feels valued. The codebase improved. Everyone wins.

### Strategy #4: The Contained Experiment

If Ferris really wants to write Rust, find a contained experiment:

"The new logging component has a clean C interface. It handles untrusted input. It's isolated. Write it in Rust. Let's see how the integration works."

If it succeeds: you've learned something, Ferris is happy, and maybe Rust is appropriate for some components.

If it fails: you've learned something, Ferris has confronted the integration challenges, and the argument is more grounded.

### Strategy #5: The Exit Clause

If Ferris can't function in a C++ codebase—if every code review is a Rust lecture, if every design discussion becomes a rewrite proposal, if he's actively harming team morale—then the situation is untenable.

"Ferris, I value your perspective on memory safety. But this team works in C++. We need someone who can work productively in C++ while improving it, not someone who sees every line as an argument for a different language. Can you commit to that?"

Sometimes the answer is no. That's a management problem, not a technical one.

---

## When Ferris Is Actually Right

There are situations where Rust genuinely is the better choice:

### 1. Security-Critical New Code

If you're writing:
- A cryptography library
- A parser for untrusted input
- Network protocol handling
- Anything that will face adversarial input

Rust's memory safety guarantees are genuinely valuable. The bugs it prevents are the bugs attackers exploit.

### 2. New Concurrent Infrastructure

If you're writing:
- A new lock-free data structure
- A complex async runtime
- High-contention shared-state code

Rust's `Send`/`Sync` traits catch bugs that C++ ThreadSanitizer might miss in testing.

### 3. New Standalone Tools

If you're writing:
- A command-line tool
- A separate microservice
- A standalone utility

With no integration burden, Rust's benefits come without FFI costs.

### 4. The Team Is Willing to Learn

If your team:
- Is interested in Rust
- Has time to learn it properly
- Commits to maintaining two languages

Then gradual Rust adoption might make sense.

---

## The Honest Assessment

| Aspect | C++ | Rust | Verdict |
|--------|-----|------|---------|
| Memory safety | Runtime tools | Compile-time | Rust wins |
| Thread safety | Discipline | Type system | Rust wins |
| Error handling | Exceptions/expected | Result | Tie |
| Build system | CMake (pain) | Cargo (joy) | Rust wins |
| Ecosystem | Massive | Growing | C++ wins |
| Hiring | Easy | Harder | C++ wins |
| Learning curve | Familiar | Steep | Depends |
| Interop with C | Native | FFI | C++ wins |
| Performance | Equal | Equal | Tie |
| Legacy code | Exists | Doesn't | C++ wins (by default) |

Rust is a better-designed language for safety-critical systems. C++ has 40 years of code, tools, and developers. Migrating is expensive. Both can be true.

---

## Appendix: Quick Reference Card

### What Ferris Says → What It Means → How to Respond

| Ferris Says | What It Means | How to Respond |
|-------------|---------------|----------------|
| "This would be a compile error in Rust" | Rust's type system would catch this bug | "You're right. What tooling can we add to catch this in C++?" |
| "Who owns this pointer?" | Ownership is unclear | "Let's document it, or better, use smart pointers" |
| "The borrow checker would prevent this" | There's a potential lifetime bug | "Can you identify the bug? Let's fix it." |
| "We should rewrite this in Rust" | He's frustrated with C++ limitations | "What specific problem are you trying to solve?" |
| "Fearless concurrency" | C++ threading is error-prone | "You're right. Let's add thread safety annotations." |
| "Result<T, E> makes errors explicit" | He wants explicit error handling | "Let's use std::expected for new code" |
| "Cargo is so much better" | CMake is painful | "Agreed. Want to improve our build scripts?" |
| "This is all unsafe" | He sees risk everywhere | "Which risks are you most concerned about?" |

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "We're not rewriting in Rust." | "Let's focus on improving the C++ we have." |
| "Stop comparing everything to Rust." | "I appreciate the Rust perspective. How can we apply that insight here?" |
| "This is fine. It's worked for years." | "It works, but I hear your concern. What would you change?" |
| "Your Rust evangelism is exhausting." | "I'd love your help improving our C++ practices." |
| "We hire C++ developers, not Rust developers." | "The team's expertise is in C++. How can we leverage that?" |
| "The rewrite would take years." | "Migration has costs. Let's quantify them before deciding." |

---

*Ferris isn't wrong about Rust. He's wrong about what's practical for your team, your codebase, and your timeline. Meet him where he is, learn what you can, and keep shipping.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 77%  
**Survival probability without:** 35%  
**Probability Ferris mentions the borrow checker in the next meeting:** 100%  
**Probability he has a point:** 73%
