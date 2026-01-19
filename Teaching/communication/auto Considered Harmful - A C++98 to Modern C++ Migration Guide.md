# auto Considered Harmful
## A C++98 → Modern C++ Migration Guide for the Willfully Antiquated

*"We've always done it this way."*

---

## Know Your Adversary

You've joined a team with an established C++ codebase. The code compiles. The tests pass. The product ships. And the code looks like it was written in 2003—because it was.

The senior architect—let's call him Mr. Standards—has mass: 25 years of C++ experience, mass: opinions formed during the Clinton administration. He learned C++ from the ARM (Annotated Reference Manual). He survived the transition from cfront to native compilers. He remembers when `export template` was supposed to work.

Mr. Standards is not a bad programmer. He's experienced, careful, and his code is reliable. But he stopped learning in 2003. Every feature added since then is, in his view, unnecessary complexity that solves problems he doesn't have.

Mr. Standards exhibits the following clinical symptoms:

**auto Aversion.** He believes that explicit types are documentation. When he sees `auto x = get_value();`, he doesn't know what `x` is. This is intolerable. Every variable must have its type spelled out, no matter how obvious or verbose.

```cpp
// Mr. Standards' code
std::map<std::string, std::vector<std::pair<int, double>>>::const_iterator it = 
    complicated_map.find(key);

// "Now I know exactly what type 'it' is!"
// (He will never type this correctly on the first try. Neither will you.)
```

**Lambda Loathing.** Lambdas are "anonymous functions," and anonymous is suspicious. You can't search for them. You can't set breakpoints on them. They're "clever" code that makes debugging harder. Real functions have names.

```cpp
// Mr. Standards: a proper functor
struct IsPositive {
    bool operator()(int x) const { return x > 0; }
};

std::vector<int> result;
std::remove_copy_if(v.begin(), v.end(), std::back_inserter(result), 
                    std::not1(IsPositive()));

// The functor is defined 200 lines above this usage.
// You have to scroll up to understand what this does.
// This is "readable" because the type has a name.
```

**Move Semantics Mysticism.** He knows move semantics exist. He's read about them. He doesn't trust them. What if something gets moved from unexpectedly? What if he uses a moved-from object? Better to use const references and explicit copies. The performance difference is probably negligible anyway.

```cpp
// Mr. Standards: "safe" resource handling
class Document {
public:
    // Copy only. Moving is dangerous.
    Document(const Document& other) : data_(new char[other.size_]) {
        std::copy(other.data_, other.data_ + other.size_, data_);
    }
    Document& operator=(const Document& other) {
        if (this != &other) {
            delete[] data_;
            data_ = new char[other.size_];
            size_ = other.size_;
            std::copy(other.data_, other.data_ + other.size_, data_);
        }
        return *this;
    }
    // No move operations. "We don't need them."
private:
    char* data_;
    size_t size_;
};

// Returning a Document copies the entire contents.
// Mr. Standards considers this "safe."
```

**Smart Pointer Skepticism.** He documents ownership in comments. If ownership is documented, why do you need smart pointers? `std::unique_ptr` is just overhead. `std::shared_ptr` has atomic reference counting. He'll manage his own pointers, thank you.

```cpp
// Mr. Standards: documented ownership
class Engine {
public:
    // NOTE: Engine takes ownership of 'part'. Caller must not delete.
    void add_part(Part* part);  // Ownership transferred
    
    // NOTE: Returns borrowed pointer. Do not delete.
    Part* get_part(int id);  // Ownership retained
    
    // NOTE: Caller takes ownership. Must delete.
    Part* release_part(int id);  // Ownership transferred
    
    ~Engine() {
        for (size_t i = 0; i < parts_.size(); ++i) {
            delete parts_[i];  // Don't forget!
        }
    }
private:
    std::vector<Part*> parts_;  // Owned pointers
};

// Six months later, someone calls delete on a borrowed pointer.
// The documentation was clear! It said "do not delete"!
// (They didn't read the documentation.)
```

**Range-Based Reluctance.** He uses explicit iterators because that's how STL works. Range-based for loops hide the iterator type. What if you need the iterator? What if the loop semantics change? Explicit is better than implicit.

```cpp
// Mr. Standards: explicit iterator loops
for (std::vector<Widget>::iterator it = widgets.begin(); 
     it != widgets.end(); 
     ++it) {
    it->process();
}

// "What if I need to erase an element? Range-based can't do that!"
// (He's not erasing elements in this loop. He just might someday.)
```

**nullptr Neglect.** NULL has worked for 40 years. Why change now? `nullptr` is just syntax sugar. The compiler knows what NULL means.

```cpp
// Mr. Standards: NULL everywhere
Widget* find_widget(int id) {
    for (size_t i = 0; i < widgets_.size(); ++i) {
        if (widgets_[i]->id() == id) {
            return widgets_[i];
        }
    }
    return NULL;  // "Everyone knows what NULL means"
}

void process(int* p) { /* ... */ }
void process(Widget* p) { /* ... */ }

process(NULL);  // Which overload? Depends on the compiler!
process(nullptr);  // Always Widget* version. Unambiguous.
```

**The Compatibility Crusade.** The codebase must compile on GCC 4.1 because "some customers might still use it." The codebase must compile on Solaris because "we had a Solaris customer in 2009." The codebase must avoid C++11 because "not everyone has upgraded."

It's 2026. The last Solaris customer migrated away in 2018. Nobody has reported a GCC 4.1 issue in a decade. But the requirement persists, zombie-like, blocking every modernization attempt.

**The Template Terror.** Templates are slow to compile. Templates produce unreadable error messages. Templates are hard to debug. Mr. Standards uses templates sparingly, prefers runtime polymorphism, and believes that compile times matter more than runtime performance.

---

## The Modernization Battles

### Battle #1: The auto Argument

**The Conflict**

Mr. Standards demands explicit types. He believes this is self-documenting code:

```cpp
std::map<std::string, std::vector<std::shared_ptr<Transaction>>>::iterator it =
    transactions_by_customer.find(customer_id);
```

When you propose `auto`:

```cpp
auto it = transactions_by_customer.find(customer_id);
```

He objects: "Now I can't see the type! I have to go find the declaration of `transactions_by_customer` to know what `it` is!"

**Arguments That Don't Work**

- "auto is more readable." (No it isn't. I can't see the type.)
- "The compiler deduces the type." (I don't want to guess what the compiler deduced.)
- "Everyone uses auto now." (Appeal to popularity isn't an argument.)

**Arguments That Do Work**

- "You wrote the wrong type. Your explicit declaration says `iterator`, but `find()` returns `const_iterator` when called on a const map. auto can't get it wrong." (Show him his own bugs.)
- "This line has a bug: you're creating a temporary when you meant to create a reference. `Widget w = get_widget();` copies; `auto& w = get_widget();` doesn't. The auto version forces you to think about the reference, while the explicit type buries the copy." (Explicit types hide copies.)
- "auto isn't 'hiding' the type—it's delaying the commitment. When the return type of `find()` changes (and it did, in your refactoring last month), auto adapts. Your explicit type broke." (Maintainability argument.)

**The Correct Usage**

```cpp
// auto for iterators: impossible to get wrong
auto it = container.find(key);

// auto with & for references: forces you to be explicit about copying
const auto& widget = get_widget();  // Explicit: this is a reference
auto widget_copy = get_widget();    // Explicit: this is a copy

// auto for complex types: readable
auto results = query.execute();     // Who cares about the exact type?
                                    // It's "the thing that query.execute() returns"

// Explicit types when the type IS the documentation
size_t count = 0;      // Not auto: "count is a size"
double average = 0.0;  // Not auto: "average is a double"
```

The key insight: **auto isn't hiding types—it's expressing "I don't care about the exact type; I care about the interface."** When you write `auto it = map.find(key);`, you're saying "it is whatever find returns, and I'll use it as an iterator." The exact type is noise.

---

### Battle #2: The Lambda Litigation

**The Conflict**

Mr. Standards writes functors for everything:

```cpp
// Mr. Standards' approach: functor class
struct MatchesId {
    int target_id;
    explicit MatchesId(int id) : target_id(id) {}
    bool operator()(const Record& r) const {
        return r.id == target_id;
    }
};

// Usage, 150 lines away from definition
auto it = std::find_if(records.begin(), records.end(), MatchesId(target_id));
```

When you propose a lambda:

```cpp
auto it = std::find_if(records.begin(), records.end(),
    [target_id](const Record& r) { return r.id == target_id; });
```

He objects: "I can't search for this. I can't set a breakpoint. It's anonymous. What if I need to reuse it?"

**Arguments That Don't Work**

- "Lambdas are more concise." (Concise isn't better. Clear is better.)
- "This is the modern way." (Modern isn't automatically better.)
- "Functors are verbose." (Verbosity aids understanding.)

**Arguments That Do Work**

- "The functor is 150 lines away. To understand this `find_if`, I have to scroll up, find `MatchesId`, read it, hold it in my head, scroll back, and resume reading. The lambda puts the predicate at the point of use. Everything I need is in one place." (Locality of reasoning.)
- "The functor is a bug waiting to happen. If someone adds a field to `MatchesId` without updating all users, we get silent misbehavior. The lambda captures exactly what it needs, and the compiler verifies the captures." (Capture correctness.)
- "You CAN set breakpoints in lambdas. You CAN search for them (search for the find_if call). The tooling has evolved. Your objections were valid in 2011; they're not valid now." (Tooling has caught up.)

**The Correct Usage**

```cpp
// Lambda for single-use predicates
auto it = std::find_if(records.begin(), records.end(),
    [target_id](const Record& r) { return r.id == target_id; });

// Lambda for captures that would clutter a functor
auto processor = [&config, &logger, error_count = 0](const Event& e) mutable {
    if (!config.is_enabled(e.type)) return;
    logger.log(e);
    if (e.is_error) ++error_count;
};

// Named lambda when you need to reuse
auto is_valid = [](const Widget& w) { 
    return w.status == Status::Active && w.health > 0; 
};

// Use in multiple places
auto first_valid = std::find_if(widgets.begin(), widgets.end(), is_valid);
auto valid_count = std::count_if(widgets.begin(), widgets.end(), is_valid);

// Functor ONLY when:
// - Stateful operation with complex initialization
// - Need virtual dispatch (rare)
// - Pre-C++11 compatibility required (rarer)
```

The key insight: **Lambdas are not anonymous—they're local. The name of a predicate used once in one place is noise, not signal.**

---

### Battle #3: The Move Semantics Melee

**The Conflict**

Mr. Standards avoids move semantics. He's heard of "use after move" bugs. He knows moved-from objects are in an "unspecified but valid state." He'd rather copy than risk corruption.

```cpp
// Mr. Standards: copies everywhere
class Document {
public:
    explicit Document(const std::string& content) : content_(content) {}
    
    std::string get_content() const { return content_; }  // Copies out
    void set_content(const std::string& content) { content_ = content; }  // Copies in
    
private:
    std::string content_;
};

std::string create_report() {
    std::string report;
    report += header;
    report += body;
    report += footer;
    return report;  // Copies on return (or does it?)
}
```

When you propose move semantics:

```cpp
void set_content(std::string content) { content_ = std::move(content); }
```

He objects: "What if someone uses `content` after this? What state is it in? This is dangerous."

**Arguments That Don't Work**

- "Move semantics are faster." (Premature optimization.)
- "Everyone uses moves now." (Everyone does lots of dumb things.)
- "The compiler knows what it's doing." (I don't trust the compiler.)

**Arguments That Do Work**

- "Your return-by-value already uses move semantics via copy elision (RVO) and implicit move. You've been benefiting from move semantics for a decade without knowing it. We're just making it explicit." (He's already using it.)
- "The 'use after move' bug requires two things: moving from a named variable, then using it again. This is like worrying about use-after-free—a real concern, but one we can prevent with discipline. You already prevent use-after-free; you can prevent use-after-move." (Same discipline he already has.)
- "Your Document class copies a 10MB string on every `set_content`. With move semantics, temporary strings move in for free. Profile it: move version is 100x faster for large documents." (Concrete performance data.)

**The Correct Migration**

```cpp
// Modern Document: value semantics with efficient moves
class Document {
public:
    // Take by value, move into member
    explicit Document(std::string content) : content_(std::move(content)) {}
    
    // Return by const ref for reading (no copy)
    const std::string& content() const { return content_; }
    
    // Take by value for setting (caller can move or copy)
    void set_content(std::string content) { content_ = std::move(content); }
    
    // Compiler generates move operations automatically for this class!
    
private:
    std::string content_;
};

// Usage is intuitive
Document doc("initial content");            // Moves from temporary
doc.set_content("new content");             // Moves from temporary
doc.set_content(std::move(existing_str));   // Explicit move from named var

// After this line, existing_str is empty. DOCUMENT THIS!
// But this is the caller's choice to move—the API doesn't force it.
```

The key insight: **Move semantics don't make code dangerous—they make explicit a transfer that was always implicit. The caller chooses whether to move or copy.**

---

### Battle #4: The Smart Pointer Skirmish

**The Conflict**

Mr. Standards manages memory manually with documented ownership:

```cpp
// Mr. Standards: raw pointers with ownership comments
class ResourceManager {
public:
    // Transfers ownership to ResourceManager
    void add(Resource* r);  
    
    // Returns borrowed pointer, do not delete
    Resource* get(int id);
    
    // Transfers ownership to caller
    Resource* release(int id);
    
    ~ResourceManager() {
        for (auto* r : resources_) delete r;
    }
    
private:
    std::vector<Resource*> resources_;
};
```

When you propose smart pointers:

```cpp
class ResourceManager {
public:
    void add(std::unique_ptr<Resource> r);
    Resource* get(int id);  // Raw pointer = borrowed
    std::unique_ptr<Resource> release(int id);
    // No destructor needed!
private:
    std::vector<std::unique_ptr<Resource>> resources_;
};
```

He objects: "Now I need to understand `unique_ptr`. What's the overhead? What if I need to convert to `shared_ptr` later? The raw pointers are simpler."

**Arguments That Don't Work**

- "Smart pointers prevent leaks." (I don't write leaks. I'm careful.)
- "This is modern C++." (Modern isn't a virtue.)
- "Raw pointers are unsafe." (They're safe if you use them correctly.)

**Arguments That Do Work**

- "Your ownership comments are not enforced. I can add a resource, get a pointer, and delete it—the compiler won't stop me. With unique_ptr, transfer is explicit and verified: you can't delete what you don't own because you literally can't call delete on a unique_ptr." (Compiler enforcement vs honor system.)
- "Your destructor has a bug. If `add()` throws after some resources are added but before others, the already-added resources leak. With unique_ptr, the partially-constructed vector's destructor cleans up automatically." (Exception safety.)
- "unique_ptr has zero overhead. In release builds, it compiles to exactly the same code as raw pointers. I'll show you the assembly." (Benchmark proof.)

**The Migration**

```cpp
// Ownership in the type system

// I own this exclusively
std::unique_ptr<Resource> exclusive_owner = std::make_unique<Resource>();

// I share ownership with others
std::shared_ptr<Resource> shared_owner = std::make_shared<Resource>();

// I don't own this, just using it temporarily
Resource* borrowed = exclusive_owner.get();

// Ownership transfer is explicit
void take_ownership(std::unique_ptr<Resource> r);  // I will own this
void borrow(Resource* r);                           // I will not own this
void maybe_share(std::shared_ptr<Resource> r);      // I might keep a copy

// The difference is in the TYPE, not in comments
```

The key insight: **Smart pointers encode ownership in the type system. Comments can lie or be ignored; types cannot.**

---

### Battle #5: The Range-Based Resistance

**The Conflict**

Mr. Standards uses explicit iterator loops:

```cpp
for (std::vector<Widget>::iterator it = widgets.begin(); 
     it != widgets.end(); 
     ++it) {
    process(*it);
}
```

When you propose range-based for:

```cpp
for (Widget& w : widgets) {
    process(w);
}
```

He objects: "What if I need the iterator? What if I need the index? What if I need to erase elements? Range-based is limited."

**Arguments That Don't Work**

- "Range-based is cleaner." (It hides the iterator.)
- "This is the modern way." (Not an argument.)
- "You rarely need the iterator." (But when I do, I'm stuck.)

**Arguments That Do Work**

- "You don't need the iterator in this loop. You're not using it for anything except dereferencing. The range-based for expresses your intent more clearly: 'for each widget in widgets.'" (Match syntax to intent.)
- "When you need the index, use `enumerate` (C++23) or a separate counter. When you need to erase, use `std::erase_if`. The range-based for is for iteration, not mutation." (Right tool for the job.)
- "Your explicit loop has a bug: you're using `iterator` instead of `const_iterator`, so you might accidentally modify elements. The range-based `const auto&` makes immutability explicit." (Const-correctness.)

**The Migration**

```cpp
// Range-based for: iteration
for (const auto& widget : widgets) {
    display(widget);
}

// When you need mutation
for (auto& widget : widgets) {
    widget.update();
}

// When you need the index (C++20 with ranges)
for (auto [idx, widget] : widgets | std::views::enumerate) {
    std::cout << idx << ": " << widget.name() << "\n";
}

// Pre-C++20: explicit counter
for (size_t i = 0; i < widgets.size(); ++i) {
    std::cout << i << ": " << widgets[i].name() << "\n";
}

// When you need to erase: use algorithm
std::erase_if(widgets, [](const Widget& w) { 
    return w.is_expired(); 
});

// NOT this (undefined behavior in range-based):
// for (auto& w : widgets) {
//     if (w.is_expired()) widgets.erase(&w);  // WRONG
// }
```

The key insight: **Range-based for is for iteration. If you're doing something other than iteration, use something other than range-based for.**

---

### Battle #6: The nullptr Negotiation

**The Conflict**

Mr. Standards uses NULL everywhere:

```cpp
Widget* find(int id) {
    // ...
    return NULL;
}

if (ptr == NULL) {
    // ...
}
```

When you propose nullptr:

```cpp
Widget* find(int id) {
    // ...
    return nullptr;
}
```

He objects: "What's wrong with NULL? Everyone knows what NULL means. This is change for the sake of change."

**Arguments That Don't Work**

- "nullptr is type-safe." (NULL works fine.)
- "This is modern C++." (That's not an argument.)
- "NULL is a macro." (So what?)

**Arguments That Do Work**

- "NULL is defined as 0 or `((void*)0)` depending on the compiler. In this overload set, `process(NULL)` calls the `int` overload, not the pointer overload. `process(nullptr)` unambiguously calls the pointer overload." (Show the actual bug.)

```cpp
void process(int x) { std::cout << "int: " << x << "\n"; }
void process(Widget* p) { std::cout << "pointer\n"; }

process(NULL);     // Might print "int: 0" - compiler-dependent!
process(nullptr);  // Always prints "pointer"
```

- "Template code breaks with NULL. This template deduces `T` as `int` when you pass NULL, but deduces `std::nullptr_t` when you pass nullptr. If the function expects a pointer, NULL causes a compile error." (Template interaction.)
- "It's a trivial change with zero risk. Search-and-replace NULL with nullptr. If something breaks, you've found a bug." (Easy migration.)

**The Migration**

```bash
# Simple migration
sed -i 's/\bNULL\b/nullptr/g' src/*.cpp src/*.h

# Then compile and fix any issues (there won't be many)
```

```cpp
// Modern style
if (ptr != nullptr) { ... }
if (ptr) { ... }  // Idiomatic: implicitly compared to nullptr

return nullptr;

Widget* maybe_null = condition ? &widget : nullptr;
```

---

### Battle #7: The Compatibility Conundrum

**The Conflict**

Mr. Standards insists on GCC 4.1 compatibility. C++11 features are forbidden. The codebase is frozen in 2003.

"We have customers on old systems. We can't require newer compilers."

**Arguments That Don't Work**

- "Nobody uses GCC 4.1 anymore." (You don't know that.)
- "We should require C++17." (That would break backward compatibility.)
- "Old compilers have bugs." (So do new ones.)

**Arguments That Do Work**

- "Which customer uses GCC 4.1? I'd like to contact them and verify. If we have no current customer on GCC 4.1, we're paying a cost for a benefit no one receives." (Demand evidence.)
- "GCC 4.1 was released in 2006. Its last update was 2007. It has known code generation bugs. By supporting it, we're exposing customers to those bugs. Dropping support isn't abandoning customers—it's protecting them." (Security argument.)
- "Let's add telemetry to the build system. Track which compiler versions customers actually use. After 6 months, we'll have data. I predict GCC 4.1 usage is zero." (Measure, don't assume.)

**The Migration Strategy**

```cpp
// Phase 1: Feature detection, not version requirements
#if __cplusplus >= 201103L
    #define HAS_MOVE_SEMANTICS 1
    #define HAS_NULLPTR 1
    #define HAS_LAMBDAS 1
#else
    #define HAS_MOVE_SEMANTICS 0
    #define HAS_NULLPTR 0
    #define HAS_LAMBDAS 0
#endif

// Phase 2: Gradual adoption
#if HAS_NULLPTR
    return nullptr;
#else
    return NULL;
#endif

// Phase 3: Sunset old compilers
// After telemetry shows zero usage, remove the ifdefs
```

Eventually:

```cpp
// Phase 4: Modern codebase
// All compatibility macros removed
// Minimum: C++17
// Recommended: C++20
```

---

### Battle #8: The Template Trepidation

**The Conflict**

Mr. Standards fears templates. They're slow to compile. The error messages are unreadable. He prefers runtime polymorphism with virtual functions.

```cpp
// Mr. Standards: virtual everything
class Processor {
public:
    virtual void process(const Data& data) = 0;
    virtual ~Processor() = default;
};

class JsonProcessor : public Processor {
public:
    void process(const Data& data) override { /* ... */ }
};

void run(Processor& p, const Data& data) {
    p.process(data);  // Virtual dispatch at runtime
}
```

When you propose templates:

```cpp
template<typename Processor>
void run(Processor& p, const Data& data) {
    p.process(data);  // Resolved at compile time
}
```

He objects: "Compile times! Error messages! I can't debug template code!"

**Arguments That Don't Work**

- "Templates are faster at runtime." (Premature optimization.)
- "Templates are more flexible." (Too flexible. Hard to constrain.)
- "This is generic programming." (I don't need generic programming.)

**Arguments That Do Work**

- "Your virtual dispatch costs 15ns per call. This function is called 100 million times in the inner loop. That's 1.5 seconds of overhead. The template version inlines the call, removing all overhead." (Profile data.)
- "C++20 concepts give you the error messages you want. `template<typename T> requires Processor<T>` fails with 'T does not satisfy Processor' instead of 500 lines of template instantiation errors." (Concepts fix error messages.)
- "Compile times improved 3x with modules. Your fear of templates was valid in 2005; it's less valid now." (Tooling has improved.)

**The Correct Balance**

```cpp
// Use virtual functions when:
// - Types determined at runtime (plugins, factories)
// - ABI stability required
// - Compile-time polymorphism impossible

// Use templates when:
// - Types known at compile time
// - Performance critical
// - Working with containers/algorithms

// C++20 concepts: best of both worlds
template<typename T>
concept Processor = requires(T t, const Data& d) {
    { t.process(d) } -> std::same_as<void>;
};

template<Processor P>
void run(P& p, const Data& data) {
    p.process(data);  // Compile-time dispatch, clear error messages
}

// Usage
JsonProcessor jp;
run(jp, data);  // Compiles: JsonProcessor satisfies Processor

struct NotAProcessor {};
NotAProcessor nap;
run(nap, data);  // Error: NotAProcessor does not satisfy Processor
                  // (One clear line, not 500 lines of template noise)
```

---

## The Modernization Reference

### Feature Adoption Priority

| Feature | Priority | Risk | Effort |
|---------|----------|------|--------|
| nullptr | 🟢 High | None | Trivial |
| auto (iterators) | 🟢 High | None | Low |
| Range-based for | 🟢 High | None | Low |
| Uniform initialization | 🟡 Medium | Some | Medium |
| Lambdas | 🟡 Medium | Low | Medium |
| Smart pointers | 🟢 High | Low | Medium |
| Move semantics | 🟡 Medium | Medium | High |
| constexpr | 🟢 High | None | Medium |
| std::optional | 🟢 High | None | Medium |
| std::string_view | 🟡 Medium | Medium | Medium |
| Structured bindings | 🟢 High | None | Low |
| Concepts (C++20) | 🟡 Medium | None | Medium |
| Modules (C++20) | 🔴 Low | High | High |

### Quick Conversion Guide

```cpp
// OLD → MODERN

// NULL → nullptr
return NULL;                    →  return nullptr;

// Explicit iterator type → auto
std::vector<int>::iterator it   →  auto it

// Iterator loop → range-based
for (it = v.begin(); it != v.end(); ++it)  →  for (auto& x : v)

// Functor → lambda
struct Pred { bool operator()(int x) { return x > 0; } };
std::find_if(v.begin(), v.end(), Pred());
→
std::find_if(v.begin(), v.end(), [](int x) { return x > 0; });

// Raw pointer ownership → unique_ptr
class C { T* owned; ~C() { delete owned; } }
→
class C { std::unique_ptr<T> owned; }  // No destructor needed

// Output parameter → return value
void compute(int input, Result* output)  →  Result compute(int input)

// Pair for multiple returns → structured bindings
std::pair<int, std::string> get_pair();
std::pair<int, std::string> p = get_pair();
int x = p.first;
std::string s = p.second;
→
auto [x, s] = get_pair();

// 0/1 for bool → true/false
int success = 1;  →  bool success = true;

// #define constants → constexpr
#define MAX_SIZE 1024  →  constexpr size_t MAX_SIZE = 1024;

// typedef → using
typedef std::vector<int> IntVec;  →  using IntVec = std::vector<int>;

// Cast → named cast
(int)x  →  static_cast<int>(x)

// Empty check with size
if (v.size() == 0)  →  if (v.empty())

// Copy then swap idiom → move
void swap(T& a, T& b) { T tmp = a; a = b; b = tmp; }
→
void swap(T& a, T& b) { std::swap(a, b); }  // Uses moves internally
```

---

## Psychological Survival Strategies

### Strategy #1: The Bug Bounty

Mr. Standards trusts his old patterns because they've worked for 20 years. Show him where they haven't.

Every C++11+ feature solves a problem. Find instances of that problem in the codebase:

- NULL ambiguity in overload resolution → nullptr
- Iterator type bugs → auto
- Memory leaks in exception paths → smart pointers
- Inefficient copies → move semantics

When you show him bugs in HIS code that modern features prevent, the resistance weakens.

### Strategy #2: The Side-by-Side

Don't argue aesthetics. Show code.

```cpp
// Before: Mr. Standards' version
std::map<std::string, std::vector<std::pair<int, double>>>::const_iterator it =
    data.find(key);
if (it != data.end()) {
    for (std::vector<std::pair<int, double>>::const_iterator vit = it->second.begin();
         vit != it->second.end(); ++vit) {
        process(vit->first, vit->second);
    }
}

// After: Modern version
if (auto it = data.find(key); it != data.end()) {
    for (const auto& [id, value] : it->second) {
        process(id, value);
    }
}
```

"Which would you rather review in a code review? Which is more likely to have a typo?"

### Strategy #3: The Incremental Invasion

Don't propose a codebase-wide rewrite. Modernize one file. Then another. Build a beachhead.

```
Month 1: New code uses modern style
Month 3: Modified files adopt modern style
Month 6: Critical paths modernized
Month 12: Only legacy corners remain
Month 18: Full modernization
```

Mr. Standards won't notice until modern C++ is the norm and his old patterns are the exception.

### Strategy #4: The Benchmark Betrayal

Mr. Standards believes modern features have overhead. Prove him wrong.

```cpp
// Benchmark: raw pointer vs unique_ptr
// Result: identical assembly, identical performance

// Benchmark: virtual function vs template
// Result: template 10x faster (inlined)

// Benchmark: explicit loop vs ranges
// Result: ranges 5% faster (compiler optimizes better)
```

Performance is Mr. Standards' language. Speak it.

### Strategy #5: The Retirement Clock

Mr. Standards is 58. He retires in 7 years. The codebase will outlive his tenure.

Frame modernization as succession planning:

"New graduates learn C++20 in school. If we want to hire them, the codebase needs to look familiar. If we stay on C++98, we're limiting our talent pool to people who retired from COBOL."

This isn't about Mr. Standards being wrong. It's about the codebase being maintainable by the next generation.

---

## Appendix: Quick Reference Card

### The Mindset Shift

| C++98 Thinking | Modern Thinking |
|----------------|-----------------|
| Explicit types are documentation | auto expresses intent |
| Named functors are searchable | Lambdas are local to use |
| Copies are safe | Moves are efficient |
| Raw pointers with discipline | Smart pointers with guarantees |
| Iterator loops show mechanics | Range-for shows intent |
| NULL means null | nullptr is unambiguous |
| Backward compatibility first | Progress with deprecation paths |
| Templates are slow | Templates are zero-overhead |

### The Modernization Checklist

- [ ] nullptr instead of NULL
- [ ] auto for complex types and iterators
- [ ] Range-based for loops where appropriate
- [ ] Lambdas for local predicates/callbacks
- [ ] unique_ptr for exclusive ownership
- [ ] shared_ptr only when truly shared
- [ ] Move semantics for resource-heavy types
- [ ] constexpr for compile-time computation
- [ ] enum class instead of C-style enums
- [ ] using instead of typedef
- [ ] = default / = delete for special members
- [ ] override on virtual function overrides
- [ ] [[nodiscard]] on functions whose return matters

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "Your code is ancient." | "We could simplify this with newer features." |
| "NULL is broken." | "nullptr avoids this overload ambiguity." |
| "This functor is overkill." | "A lambda would keep the logic local." |
| "Raw pointers are dangerous." | "unique_ptr would encode the ownership." |
| "Nobody uses GCC 4.1." | "Which customer specifically requires this?" |
| "Your iterator loop has a bug." | "auto would have deduced the correct type." |
| "You're blocking progress." | "What would it take to adopt C++17?" |

---

*The language has evolved. Your code should too.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 75%  
**Survival probability without:** 20%  
**Probability Mr. Standards says "we've always done it this way":** 100%  
**Probability he's wrong:** 73%
