# The Angle Brackets Are Not Your Enemy
## A Guide for Developers Suffering from Template Phobia

*"I'll just copy-paste it for each type."*

---

## Know Your Adversary

A developer on your team—let's call him Steve—has been writing C++ for eight years. He's productive, experienced, and mass: absolutely terrified of templates. He has a photographic memory of the time in 2014 when he misused `std::vector<std::map<std::string, std::pair<int, std::vector<double>>>>` and the compiler produced an error message longer than *War and Peace*.

Steve has since avoided templates entirely. Not just writing them—*using* them makes him nervous. He has a utility file called `math_functions.cpp` with separate implementations for `int`, `float`, `double`, `long`, and `unsigned int`. When you ask why, his eye twitches.

Steve exhibits the following clinical symptoms:

**The Copy-Paste Compulsion.** Rather than write one generic function, Steve writes seventeen specific ones. Each is identical except for the types. When a bug is found, it must be fixed in seventeen places. When a bug is found in only fourteen of them, Steve calls this "expected."

```cpp
// Steve's math library
int max_int(int a, int b) { return a > b ? a : b; }
float max_float(float a, float b) { return a > b ? a : b; }
double max_double(double a, double b) { return a > b ? a : b; }
long max_long(long a, long b) { return a > b ? a : b; }
unsigned int max_uint(unsigned int a, unsigned int b) { return a > b ? a : b; }

// And when he needs max for std::string?
// He doesn't use max. He writes a different algorithm.
```

**The Void Pointer Regression.** When Steve needs polymorphism, he reaches for `void*` like a recovering alcoholic reaching for the bottle. He casts pointers to `void*`, passes them around, and casts them back. Type safety is for people who don't know what they're doing. Steve knows what he's doing. Usually.

```cpp
// Steve's "generic" container
struct GenericArray {
    void** data;
    size_t size;
    size_t element_size;
    
    void* get(size_t i) {
        return data[i];  // Caller must know the type. Good luck!
    }
};

// Usage
GenericArray arr;
// ... populate with Widget*
Widget* w = static_cast<Widget*>(arr.get(5));  // Pray this is actually a Widget
```

**The Macro Madness.** Steve discovered that macros can generate code for multiple types. He now has a file called `generic_macros.h` that contains 2,000 lines of `#define` statements. The preprocessor output is 50,000 lines. Debugging is impossible because line numbers don't match. But at least there are no templates.

```cpp
// Steve's "templates"
#define DECLARE_VECTOR_OPS(TYPE)                     \
    TYPE vector_sum_##TYPE(TYPE* arr, size_t n) {    \
        TYPE sum = 0;                                \
        for (size_t i = 0; i < n; ++i) sum += arr[i];\
        return sum;                                  \
    }                                                \
    TYPE vector_max_##TYPE(TYPE* arr, size_t n) {    \
        TYPE max = arr[0];                           \
        for (size_t i = 1; i < n; ++i)               \
            if (arr[i] > max) max = arr[i];          \
        return max;                                  \
    }

DECLARE_VECTOR_OPS(int)
DECLARE_VECTOR_OPS(float)
DECLARE_VECTOR_OPS(double)

// Usage
int result = vector_sum_int(data, n);  // At least it's type-safe?
```

**The Error Message PTSD.** The real source of Steve's trauma is a single error message from 2014. He used a `std::set` incorrectly. The compiler produced 347 lines of errors, most involving nested templates he'd never heard of, internal helper classes like `_Rb_tree_const_iterator`, and phrases like "no matching function for call to." Steve printed the error, framed it, and hung it on his wall as a warning.

```
// What Steve wrote
std::set<Widget> widgets;
widgets.insert(w);

// What the compiler said (abbreviated)
error: no match for 'operator<' in '__x < __y'
/usr/include/c++/4.9/bits/stl_function.h:387:20: note: candidate:
    bool std::less<_Tp>::operator()(const _Tp&, const _Tp&) const
                    [with _Tp = Widget]
    { return __x < __y; }
           ~~~~^~~~~
/usr/include/c++/4.9/bits/stl_tree.h:1861:28: required from
    'std::pair<std::_Rb_tree_node_base*, std::_Rb_tree_node_base*>
    std::_Rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::
    _M_get_insert_unique_pos(const key_type&)
         [with _Key = Widget; _Val = Widget; _KeyOfValue = std::_Identity<Widget>;
          _Compare = std::less<Widget>; _Alloc = std::allocator<Widget>;
          std::_Rb_tree<_Key, _Val, _KeyOfValue, _Compare, _Alloc>::key_type = Widget]'

// ... 340 more lines ...

// What the compiler meant:
// "Widget needs operator< defined."
```

**The "Simple Code" Philosophy.** Steve believes templates make code "complicated." Never mind that his copy-pasted functions have diverged over time, that his void pointers have caused three production crashes, and that his macros take 30 seconds to compile. The code is *simple*. He can read it. He doesn't have to understand template syntax.

**The C Heritage Pride.** Steve started in C. In C, there are no templates. C programmers got by without templates for decades. If `qsort()` is good enough for sorting, why do we need `std::sort<T>()`? Real programmers write specific code for specific types. Generics are for people who can't commit.

---

## The Uncomfortable Truth

Here's the thing about Steve: **his fear is rational.**

C++ template error messages were historically atrocious. Before concepts (C++20), a single typo in template code could produce pages of incomprehensible diagnostics. The syntax is genuinely unusual. Template metaprogramming can become write-only code. SFINAE is witchcraft.

The problem with Steve isn't that he experienced bad things. It's that:

1. He hasn't revisited templates since 2014
2. He doesn't know that error messages have improved dramatically
3. He doesn't realize that using templates is much simpler than writing them
4. His workarounds cause more problems than templates ever would

Your job is to:
- Acknowledge his legitimate concerns
- Show him modern template usage (which is approachable)
- Demonstrate the real cost of his avoidance strategies
- Graduate him from "template-phobic" to "template-tolerant"

---

## The Battles

### Battle #1: The Error Message Trauma

**Steve's Position**

"Template error messages are incomprehensible. When something goes wrong, you get 500 lines of garbage about internal implementation details. I can't debug that. With regular functions, I get one line: 'type mismatch on line 47.' I can fix that."

**The Valid Kernel**

He's not wrong about historical error messages. Pre-C++20 compilers were genuinely awful at explaining template errors. The error might be in your code, but the message talks about `__internal_detail_helper<_Tp, _Allocator, void>`. This was a legitimate usability disaster.

**The Modern Reality**

C++20 concepts changed everything. The compiler now tells you *what's actually wrong*:

```cpp
// C++20 with concepts
template<typename T>
concept Comparable = requires(T a, T b) { { a < b } -> std::convertible_to<bool>; };

template<Comparable T>
T better_max(T a, T b) { return a > b ? a : b; }

struct Widget { int id; };

int main() {
    Widget w1{1}, w2{2};
    better_max(w1, w2);  // Error!
}
```

**Old error (pre-concepts):**
```
error: no match for 'operator>' in 'a > b'
[47 more lines about template instantiation]
```

**New error (C++20 with concepts):**
```
error: 'Widget' does not satisfy 'Comparable'
note: because 'w1 < w2' would be invalid: no match for 'operator<'
```

Even without concepts, modern compilers (GCC 10+, Clang 10+) have dramatically improved template diagnostics.

**How to Respond to Steve**

"I know template errors used to be nightmarish. But try this: write a small example using `std::vector` incorrectly. Look at the error. It's actually readable now. The compiler tells you what constraint you violated, not what internal helper class failed.

And here's the thing—your void pointer code has no error messages at all. When you cast wrong, the program crashes at runtime. Or worse, it silently corrupts data. Template errors are annoying, but they happen at compile time. Your approach pushes errors to production."

---

### Battle #2: The Copy-Paste Problem

**Steve's Position**

"I understand my copy-pasted functions. I can read each one. I know exactly what it does for each type. With templates, there's this abstract `T` that could be anything. It's harder to reason about."

**The Valid Kernel**

Explicit code is easier to understand locally. When you read `int max_int(int a, int b)`, there's no ambiguity about types.

**The Actual Problem**

```cpp
// Steve's max functions over time

// Original implementation (all five types)
int max_int(int a, int b) { return a > b ? a : b; }

// Six months later: bug fix applied to some
int max_int(int a, int b) { return (a > b) ? a : b; }  // Added parens
float max_float(float a, float b) { return a > b ? a : b; }  // Forgot to update

// One year later: handling edge case
int max_int(int a, int b) { 
    // Handle INT_MIN edge case... wait, is this even needed?
    return a > b ? a : b; 
}
double max_double(double a, double b) { 
    // Handle NaN
    if (std::isnan(a)) return b;
    if (std::isnan(b)) return a;
    return a > b ? a : b; 
}
// float version doesn't handle NaN. Nobody noticed for 8 months.
```

With templates, there's one implementation. Fix once, fixed everywhere:

```cpp
template<typename T>
T max_value(T a, T b) {
    if constexpr (std::is_floating_point_v<T>) {
        if (std::isnan(a)) return b;
        if (std::isnan(b)) return a;
    }
    return a > b ? a : b;
}
```

**How to Respond to Steve**

"Count your copy-pasted functions. Now count the bugs that exist in only some of them. Every divergence is a maintenance burden and a potential defect. Templates enforce consistency. You fix the bug once. You add the feature once. The 'abstract T' you're worried about is actually a guarantee: this code behaves the same for all types.

Your explicit code is clear locally but unclear globally. Which versions have which fixes? You don't know without reading all of them."

---

### Battle #3: The Void Pointer Danger

**Steve's Position**

"void* has been used for generic programming since the dawn of C. It works. I know I need to cast correctly. I'm careful."

**The Valid Kernel**

void* is genuinely useful for type-erased interfaces, especially when interacting with C APIs or runtime polymorphism.

**The Actual Problem**

```cpp
// Steve's "careful" code
void generic_process(void* data, int type_id) {
    switch (type_id) {
        case 1: process_int(static_cast<int*>(data)); break;
        case 2: process_float(static_cast<float*>(data)); break;
        case 3: process_widget(static_cast<Widget*>(data)); break;
    }
}

// Three months later, someone adds:
case 4: process_gadget(static_cast<Gadget*>(data)); break;

// Six months later, someone calls:
Gizmo g;
generic_process(&g, 4);  // They thought 4 was Gizmo. It's Gadget. 

// Result: undefined behavior. Maybe crash. Maybe silent corruption.
// No compiler error. No runtime error. Just wrong behavior.
```

Templates catch this at compile time:

```cpp
template<typename T>
void typed_process(T* data) {
    process(data);  // Compiler selects correct overload
}

// Three months later, someone tries:
Gizmo g;
typed_process(&g);  // Compiler error: no matching function 'process(Gizmo*)'
```

**How to Respond to Steve**

"I know you're careful. But 'careful' doesn't scale to teams. It doesn't scale to 3 AM debugging. It doesn't scale to the new hire who doesn't know your type_id conventions.

The void pointer error doesn't produce an error message—it produces a crash report. Or a silent data corruption bug that takes weeks to diagnose. Template errors are annoying, but they're the compiler saying 'I caught this for you.'"

---

### Battle #4: The Macro Nightmare

**Steve's Position**

"Macros generate type-specific code just like templates. They're simpler. No angle brackets. No typename. No template argument deduction. Just text substitution I can understand."

**The Valid Kernel**

Macros do generate code. They predate templates. They work.

**The Actual Problems**

```cpp
// Steve's macro
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Problem 1: Double evaluation
int x = MAX(expensive_function(), y);  
// Calls expensive_function() TWICE if it's greater than y

// Problem 2: No type checking
int result = MAX("hello", 42);  // Compiles! Comparing pointer to int.

// Problem 3: Debugging nightmare
MAX(a, b)  // Breakpoint here shows... what? The macro is gone.

// Problem 4: No scope
#define VALUE 100
namespace internal {
    int VALUE = 200;  // ERROR: "100 = 200" doesn't compile
}

// Problem 5: The expansion from hell
int z = MAX(MAX(a, b), MAX(c, d));
// Expands to:
// ((((a) > (b) ? (a) : (b))) > (((c) > (d) ? (c) : (d))) ? 
//  (((a) > (b) ? (a) : (b))) : (((c) > (d) ? (c) : (d))))
// Good luck debugging that.
```

The template equivalent:

```cpp
template<typename T>
constexpr T max_value(T a, T b) {
    return a > b ? a : b;
}

int x = max_value(expensive_function(), y);  // Called once
int bad = max_value("hello", 42);  // Compile error: mismatched types
// Debugger sees max_value<int>, steps through real code
// max_value is a real function in a real scope
```

**How to Respond to Steve**

"Your macros have no type safety, evaluate arguments multiple times, and disappear from debug info. Templates have all the code generation benefits with none of those drawbacks.

Look at your compile times. Your 2,000-line macro header expands to 50,000 lines in every translation unit. Templates use explicit instantiation—compile once, use everywhere. Your macros are actually slower to compile than the templates you're avoiding."

---

### Battle #5: The "I Just Won't Use Generic Code" Defense

**Steve's Position**

"I don't need generic code. I write specific code for specific problems. If I need to sort integers, I write integer sort. If I need to sort strings, I write string sort. This is fine."

**The Valid Kernel**

YAGNI (You Ain't Gonna Need It) is a real principle. Writing generic code for one concrete use case is over-engineering.

**The Actual Problem**

Steve is already using templates. He just doesn't know it.

```cpp
// Steve's code
std::vector<int> numbers;           // Template
std::string name;                   // Template (basic_string<char>)
std::map<std::string, int> counts;  // Template
std::unique_ptr<Widget> widget;     // Template
std::function<void()> callback;     // Template

// Steve writes this:
for (std::vector<int>::iterator it = numbers.begin(); 
     it != numbers.end(); ++it) {
    // ...
}

// He could write this:
for (auto it = numbers.begin(); it != numbers.end(); ++it) {
    // ...
}

// Or even:
for (int n : numbers) {
    // ...
}
```

Steve uses templates every day. The standard library is templates. His fear has made him write verbose code around templates rather than embracing them.

**How to Respond to Steve**

"You use templates constantly. `std::vector<int>` is a template. `std::string` is `std::basic_string<char>`. Your entire codebase depends on template code working correctly.

The question isn't whether to use templates—it's whether to use them *well*. Right now you're writing `std::vector<int>::iterator` when you could write `auto`. You're avoiding `std::max<T>` and writing five functions instead. You're using templates the hard way."

---

## The Progressive Curriculum

Don't try to teach Steve variadic templates and SFINAE on day one. Graduate him through levels:

### Level 1: Template User (Week 1)

Just use standard library templates more fluently.

```cpp
// Before: Steve's verbose code
std::vector<int>::iterator it;
std::map<std::string, int>::const_iterator map_it;

// After: auto
auto it = vec.begin();
auto map_it = counts.find("key");

// Before: manual loops
for (size_t i = 0; i < vec.size(); ++i) {
    process(vec[i]);
}

// After: range-based for
for (const auto& item : vec) {
    process(item);
}

// Before: manual search
bool found = false;
for (auto it = vec.begin(); it != vec.end(); ++it) {
    if (*it == target) { found = true; break; }
}

// After: algorithm
bool found = std::find(vec.begin(), vec.end(), target) != vec.end();
// Or C++20:
bool found = std::ranges::contains(vec, target);
```

### Level 2: Simple Generic Functions (Week 2-3)

Write basic templates that work for any type.

```cpp
// Start with the simplest possible template
template<typename T>
T identity(T value) {
    return value;
}

// Slightly more useful: a logging wrapper
template<typename T>
void debug_print(const std::string& name, const T& value) {
    std::cout << name << " = " << value << std::endl;
}

// Practical: a null-safe getter
template<typename T>
T value_or(const T* ptr, T default_value) {
    return ptr ? *ptr : default_value;
}

// Usage is obvious and error messages are clear
int x = identity(42);
debug_print("count", items.size());
int count = value_or(maybe_null, 0);
```

### Level 3: Container-Agnostic Code (Week 4-6)

Write code that works with any container.

```cpp
// Works with vector, list, deque, array...
template<typename Container>
auto sum(const Container& c) {
    using ValueType = typename Container::value_type;
    ValueType total{};
    for (const auto& item : c) {
        total += item;
    }
    return total;
}

// Works with any container of any type
template<typename Container, typename Predicate>
size_t count_if_any(const Container& c, Predicate pred) {
    size_t count = 0;
    for (const auto& item : c) {
        if (pred(item)) ++count;
    }
    return count;
}

// Usage
std::vector<int> vec = {1, 2, 3, 4, 5};
std::list<double> lst = {1.1, 2.2, 3.3};

auto vec_sum = sum(vec);  // int
auto lst_sum = sum(lst);  // double

auto evens = count_if_any(vec, [](int n) { return n % 2 == 0; });
```

### Level 4: Concepts (Week 7-8)

Use C++20 concepts to constrain templates and get better error messages.

```cpp
#include <concepts>

// Constrained template: T must support comparison
template<typename T>
    requires std::totally_ordered<T>
T clamped(T value, T min, T max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Or using concept directly
template<std::integral T>  // T must be an integer type
T safe_divide(T a, T b) {
    if (b == 0) throw std::invalid_argument("division by zero");
    return a / b;
}

// Custom concept
template<typename T>
concept Printable = requires(std::ostream& os, T value) {
    { os << value } -> std::same_as<std::ostream&>;
};

template<Printable T>
void log(const T& value) {
    std::cout << "[LOG] " << value << std::endl;
}
```

### Level 5: (Optional) Template Metaprogramming

Only if Steve is curious and ready. Most developers never need this.

```cpp
// Compile-time factorial (teaching example)
template<int N>
struct Factorial {
    static constexpr int value = N * Factorial<N-1>::value;
};

template<>
struct Factorial<0> {
    static constexpr int value = 1;
};

// Usage: computed at compile time
constexpr int fact5 = Factorial<5>::value;  // 120

// Modern alternative: constexpr functions (much clearer)
constexpr int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}
```

---

## Psychological Survival Strategies

### Strategy #1: The Empathy Opening

Steve's fear isn't irrational. Acknowledge it.

"I remember the first time I got a 400-line template error. I wanted to quit programming and become a farmer. The old errors were genuinely terrible. But the tooling has improved. Want to try a small example together?"

Validation disarms defensiveness.

### Strategy #2: The Side-by-Side Comparison

Don't argue abstractly. Show concrete before/after.

```cpp
// Steve's current code: 47 lines, 5 functions
int max_int(int a, int b) { return a > b ? a : b; }
float max_float(float a, float b) { return a > b ? a : b; }
double max_double(double a, double b) { return a > b ? a : b; }
long max_long(long a, long b) { return a > b ? a : b; }
unsigned max_uint(unsigned a, unsigned b) { return a > b ? a : b; }

// Template version: 4 lines, infinite types
template<typename T>
T max_value(T a, T b) { 
    return a > b ? a : b; 
}
```

"Which one would you rather maintain? Which one is easier to add a bug fix to?"

### Strategy #3: The Incremental Adoption

Don't ask Steve to rewrite his code. Ask him to use `auto` more.

"You don't have to write templates. Just use `auto` instead of spelling out `std::vector<int>::const_iterator`. That's it. That's the whole change."

Once he's comfortable with `auto`, introduce range-based for loops. Then `std::find`. Then maybe, someday, a simple template function.

### Strategy #4: The Production Bug Retrospective

Find a bug caused by Steve's template avoidance.

"Remember that crash last month? It was the void* cast. The caller passed a Gadget* but the function expected a Widget*. No type checking. With templates, that's a compile error. The compiler would have caught it."

Past pain is memorable. Connect template benefits to specific incidents.

### Strategy #5: The Modern Tooling Demo

Show Steve that IDE support has improved dramatically.

- **Clangd/VSCode:** Hover over a template instantiation to see the resolved types
- **C++ Insights:** Website that shows template expansion
- **Compiler Explorer:** Godbolt shows generated assembly for each instantiation

"You don't have to imagine what `std::vector<int>` becomes. The tooling shows you."

---

## When Steve Is Actually Right

There are situations where avoiding templates makes sense:

### 1. Compile Time Constraints

Templates increase compile time. In a massive codebase where build time matters, sometimes explicit instantiation or virtual dispatch is worth the runtime cost.

```cpp
// Template: instantiated in every translation unit that uses it
template<typename T>
void process(T& data);  // In header, compiled everywhere

// Alternative: virtual dispatch, compiled once
class Processor {
public:
    virtual void process(void* data) = 0;
};
```

### 2. ABI Stability

Templates make ABI stability harder. If you're writing a library with stable ABI guarantees, sometimes type-erased interfaces are appropriate.

```cpp
// std::function uses type erasure internally
// This is appropriate when you need a stable interface
std::function<void(int)> callback;
```

### 3. Runtime Type Determination

When types are determined at runtime (user input, configuration files, plugins), templates don't apply.

```cpp
// Can't do this with templates—type determined at runtime
std::string type_name = config.get("data_type");  // "int" or "float"
// Need runtime dispatch or type erasure here
```

### 4. Genuinely Simple Code

If you have exactly one type and will never need another, a specific function is fine.

```cpp
// If you only ever process Widgets, this is fine:
void process_widget(Widget& w);

// Don't write a template just to prove you can:
template<typename T>
void process(T& t);  // YAGNI if T is always Widget
```

---

## The Honest Assessment

| Approach | Maintainability | Type Safety | Compile Time | Runtime Speed | Debug Experience |
|----------|-----------------|-------------|--------------|---------------|------------------|
| Copy-paste functions | Low | High | Fast | Optimal | Good |
| void* casting | Low | None | Fast | Optimal | Poor |
| Macros | Very Low | None | Slow | Optimal | Terrible |
| Templates | High | High | Slower | Optimal | Improved |
| Virtual dispatch | High | High | Fast | Slower | Good |

Templates trade compile time for maintainability, type safety, and runtime performance. For most code, this is the right tradeoff.

---

## Appendix: Quick Reference Card

### Steve's Symptoms → The Real Problem → The Solution

| Symptom | Real Problem | Solution |
|---------|--------------|----------|
| Copy-pasting functions for each type | DRY violation, divergence bugs | Write one template |
| Using void* everywhere | No type safety, runtime crashes | Use templates or std::variant |
| Macros for generic code | No type checking, debug nightmare | Templates with better errors |
| Avoiding standard algorithms | Verbose, error-prone manual loops | Use `std::find`, `std::sort`, etc. |
| Spelling out iterator types | Noise, maintenance burden | Use `auto` |
| Fear of angle brackets | Historical trauma, outdated | Try modern compiler, see improved errors |

### The Graduated Introduction

| Week | Skill | Example |
|------|-------|---------|
| 1 | Use `auto` | `auto it = vec.begin();` |
| 2 | Range-based for | `for (const auto& x : vec)` |
| 3 | Standard algorithms | `std::find`, `std::sort` |
| 4 | Simple template function | `template<typename T> T max(T a, T b)` |
| 5 | Container-agnostic code | `template<typename C> auto sum(const C& c)` |
| 6 | Concepts (C++20) | `template<std::integral T>` |

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "Your code is unmaintainable." | "What if we had to add a new type?" |
| "This is obvious copy-paste." | "I notice these functions are similar. Could we consolidate?" |
| "void* is dangerous." | "What happens if someone passes the wrong type here?" |
| "Your macros are a nightmare." | "Have you tried debugging through this macro expansion?" |
| "Just use templates." | "Modern C++ has much better error messages. Want to try?" |
| "You're making this harder than it needs to be." | "Let's see what the simplest solution looks like." |

### Template Syntax Cheatsheet

```cpp
// Basic function template
template<typename T>
T func(T arg);

// Multiple type parameters  
template<typename T, typename U>
auto func(T a, U b) -> decltype(a + b);

// Non-type parameter
template<int N>
struct Array { int data[N]; };

// Default template argument
template<typename T = int>
class Container;

// Template specialization
template<>
class Container<bool> { /* special implementation */ };

// Concepts constraint (C++20)
template<typename T>
    requires std::integral<T>
T func(T arg);

// Abbreviated function template (C++20)
void func(auto arg);  // Same as template<typename T> void func(T arg)
```

---

*The angle brackets aren't your enemy. They're the compiler offering to check your work. Let it help.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 81%  
**Survival probability without:** 23%  
**Probability Steve mentions the 2014 error message incident:** 100%  
**Probability his copy-pasted functions have diverged:** 94%  
**Lines in Steve's longest macro:** 847  
**Number of those lines anyone understands:** 12
