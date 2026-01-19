# Keep Your RAII, I Have Discipline
## A C → C++ Migration Guide for the Willfully Unsafe

*"I can see exactly what the machine is doing."*

---

## Know Your Adversary

You've been assigned to modernize a codebase. It's written in C—not ancient K&R C, but that somehow makes it worse. This is *deliberate* C. The author chose C in 2019. They chose it again in 2022. They'll choose it tomorrow.

The author—let's call him Chad—has mass: 15 years of C, mass: a GitHub profile featuring "blazingly fast" libraries with 47 stars. He believes, with religious conviction, that C is faster than C++. Not just sometimes. Always. Any abstraction is overhead. Any overhead is unacceptable. The only trustworthy code is code where you can see every instruction.

Chad exhibits the following clinical symptoms:

**Abstraction Anaphylaxis.** The word "class" causes visible discomfort. "Template" triggers fight-or-flight. "RAII" sounds like a disease. Chad believes that every layer of abstraction adds overhead, and the only way to achieve maximum performance is to operate as close to the machine as possible—which means C, obviously. (He has never examined the assembly output of modern C++ code. The comparison would cause cognitive dissonance.)

**void* Worship.** Chad's code is full of `void*`. It's how he achieves "genericity" without templates. Need a container that holds any type? `void*`. Callback function? `void*` context parameter. Custom allocator? `void*` user data. The fact that `void*` erases all type information and requires manual casting is not a bug—it's "flexibility."

```c
// Chad's "generic" linked list
typedef struct Node {
    void* data;           // Could be anything!
    struct Node* next;
} Node;

void list_push(Node** head, void* data) {
    Node* n = malloc(sizeof(Node));
    n->data = data;
    n->next = *head;
    *head = n;
}

// Usage: just remember what type you put in!
int* x = malloc(sizeof(int));
*x = 42;
list_push(&head, x);
// ...later...
double* y = (double*)head->data;  // WRONG TYPE. Compiles. Crashes at runtime. Eventually.
```

**Manual Memory Evangelism.** Chad manages memory by hand because he "knows when to free things." He views garbage collectors with contempt and RAII with suspicion. Smart pointers are "training wheels." He has a "discipline" about allocation and deallocation that means he doesn't need safety features. (His code leaks approximately 3MB per hour under load, but the process restarts nightly, so it's fine.)

**The Benchmark Delusion.** Chad has benchmarks. So many benchmarks. Microbenchmarks showing that raw array access is faster than `std::vector::at()`. Benchmarks showing that function pointers are faster than virtual dispatch. Benchmarks showing that his hand-rolled hash table beats `std::unordered_map`. All of these benchmarks are technically accurate and completely misleading.

**Macro Mania.** Without templates, Chad uses macros for "generic" programming:

```c
#define DECLARE_VECTOR(type) \
    typedef struct { \
        type* data; \
        size_t size; \
        size_t capacity; \
    } Vector_##type; \
    \
    void Vector_##type##_push(Vector_##type* v, type val) { \
        if (v->size == v->capacity) { \
            v->capacity = v->capacity ? v->capacity * 2 : 8; \
            v->data = realloc(v->data, v->capacity * sizeof(type)); \
        } \
        v->data[v->size++] = val; \
    }

DECLARE_VECTOR(int)
DECLARE_VECTOR(double)
DECLARE_VECTOR(char_ptr)  // Hope you didn't want "char*"
```

The error messages from macro expansion fill three screens. The debugger shows you `Vector_##type##_push`. Good luck.

**Struct Purity.** Chad uses structs, never classes. Structs with function pointers for "polymorphism." Structs with manual vtables. Structs that he passes to initialization functions and cleanup functions because constructors and destructors are "implicit" and implicit is bad. (Forgetting to call the cleanup function is a feature, not a bug. It lets you "control when resources are released.")

**The Performance Persecution Complex.** Chad believes that anyone who suggests using C++ features is willing to sacrifice performance for "convenience." He is the guardian of efficiency. He alone understands the true cost of abstraction. When you show him benchmark data proving C++ is equally fast, he questions your methodology. When you show him assembly output, he finds one extra instruction and declares victory.

---

## The Myths That Must Die

### Myth #1: "C is Faster Than C++"

**The Belief**

Chad believes that C++ adds overhead. Classes have vtables. Templates cause code bloat. Exceptions require unwinding tables. RAII inserts hidden destructor calls. All of this makes C++ slower than C.

**The Reality**

C++ follows the zero-overhead principle: you don't pay for what you don't use. Non-virtual member functions compile to the same code as C functions. Templates generate specialized code at compile time—there's no runtime overhead. A `std::vector` with bounds checking disabled is identical to a raw array.

Let's prove it:

```c
// Chad's C implementation
typedef struct {
    double* data;
    size_t size;
    size_t capacity;
} DoubleVector;

void dv_push(DoubleVector* v, double val) {
    if (v->size == v->capacity) {
        v->capacity = v->capacity ? v->capacity * 2 : 8;
        v->data = realloc(v->data, v->capacity * sizeof(double));
    }
    v->data[v->size++] = val;
}

double dv_sum(const DoubleVector* v) {
    double sum = 0.0;
    for (size_t i = 0; i < v->size; ++i) {
        sum += v->data[i];
    }
    return sum;
}
```

```cpp
// C++ implementation
std::vector<double> v;

// push_back compiles to the same code as dv_push
v.push_back(val);

// Range-based sum
double sum = std::accumulate(v.begin(), v.end(), 0.0);
```

Compile both with `-O3`. Examine the assembly. They're identical. Sometimes the C++ version is faster because the compiler has more semantic information to optimize.

**The Benchmark Chad Cites**

```c
// "Proof" that std::vector is slower
for (int i = 0; i < 1000000; ++i) {
    vec.at(i) = i;  // Bounds-checked access - OF COURSE IT'S SLOWER
}
```

Chad uses `.at()` in his benchmark, which performs bounds checking. Then he compares it to unchecked C array access and declares victory.

**The Fair Comparison**

```cpp
// Equivalent to C array access
for (int i = 0; i < 1000000; ++i) {
    vec[i] = i;  // No bounds checking, same as C
}
```

Same speed. Sometimes faster, because `std::vector` guarantees contiguous memory, which the optimizer can exploit.

**How To Convince Chad**

Don't argue theory. Show him the assembly:

```bash
# Compile both versions
gcc -O3 -S chad_c_version.c -o c_version.s
g++ -O3 -S modern_cpp_version.cpp -o cpp_version.s

# Compare
diff c_version.s cpp_version.s
```

When the diff is empty (or nearly empty), the argument ends. If Chad finds minor differences, ask him to benchmark the actual runtime. Identical.

---

### Myth #2: "void* is Flexible and Fast"

**The Belief**

Chad uses `void*` for generic programming. It's flexible—you can point to anything! It's fast—no template instantiation, no code bloat. Just cast and go.

**The Reality**

`void*` is type erasure without safety. Every use requires a cast. Every cast can be wrong. The compiler cannot help you. The bugs manifest at runtime, often far from the source.

```c
// Chad's "flexible" code
void process_data(void* data, int type_tag) {
    switch (type_tag) {
        case TYPE_INT:
            process_int(*(int*)data);
            break;
        case TYPE_DOUBLE:
            process_double(*(double*)data);
            break;
        case TYPE_STRING:
            process_string((char*)data);
            break;
        // 47 more cases...
    }
}

// Somewhere else in the codebase, 6 months later:
int x = 42;
process_data(&x, TYPE_DOUBLE);  // Oops. Silent corruption.
```

The type tag system is manual. It's not checked. It can't be checked. You're asking humans to never make mistakes. Humans always make mistakes.

**The C++ Alternative**

```cpp
// Type-safe variant: compiler checks everything
using Data = std::variant<int, double, std::string>;

void process_data(const Data& data) {
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) {
            process_int(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            process_double(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            process_string(arg);
        }
    }, data);
}

// This won't compile:
Data d = 42;  // int
process_string(std::get<std::string>(d));  // Compile error or exception - never silent corruption
```

**Performance**

`std::variant` stores a type tag internally—the same tag Chad maintains manually. The visitor pattern compiles to a switch. The generated code is identical to Chad's version, except the compiler enforces correctness.

For type-homogeneous generics, templates are even better:

```cpp
// Templates: zero overhead, complete type safety
template<typename T>
T sum(const std::vector<T>& vec) {
    T total{};
    for (const auto& val : vec) {
        total += val;
    }
    return total;
}

// Generates specialized code for each type at compile time
// No void*, no casts, no runtime type tags, no overhead
auto int_sum = sum(int_vector);       // Specialized for int
auto double_sum = sum(double_vector); // Specialized for double
```

**How To Convince Chad**

1. Show him a bug in his `void*` code. There's always one. Find it.
2. Fix the bug with `std::variant` or templates.
3. Benchmark both versions. Same speed.
4. Ask: "Would you rather have the compiler catch this, or debug it at 3 AM in production?"

---

### Myth #3: "I Know When To Free Things"

**The Belief**

Chad manages memory manually because he's disciplined. He allocates, he uses, he frees. Every malloc has a matching free. He doesn't need smart pointers because he doesn't make mistakes.

```c
// Chad's "clean" memory management
Data* load_data(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) return NULL;
    
    Data* data = malloc(sizeof(Data));
    if (!data) {
        fclose(f);
        return NULL;
    }
    
    data->buffer = malloc(BUFFER_SIZE);
    if (!data->buffer) {
        free(data);
        fclose(f);
        return NULL;
    }
    
    if (read_header(f, data) < 0) {
        free(data->buffer);
        free(data);
        fclose(f);
        return NULL;
    }
    
    if (read_body(f, data) < 0) {
        free(data->buffer);
        free(data);
        fclose(f);
        return NULL;
    }
    
    fclose(f);
    return data;
}
```

Look at all that cleanup code. Duplicated at every error path. And this is a *simple* function.

**The Reality**

The function above has a bug. Did you spot it? What if `read_header` partially fills `data->buffer` and then `read_body` fails? Is the buffer in a consistent state? What if `Data` has more fields that need cleanup? What if someone adds a field later and forgets to update all five error paths?

Manual memory management doesn't scale. The cleanup code grows with the square of the number of resources. Every new resource multiplies every error path.

And this is assuming the function is self-contained. What about ownership transfer?

```c
// Who frees this?
Data* data = load_data("input.txt");
process_data(data);  // Does this take ownership? Does it copy? WHO KNOWS.
// Should I free data here? Did process_data free it? Let's find out at runtime!
```

**The C++ Alternative**

```cpp
// RAII: resources are tied to scope
std::unique_ptr<Data> load_data(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) return nullptr;
    
    auto data = std::make_unique<Data>();
    data->buffer.resize(BUFFER_SIZE);  // vector manages its own memory
    
    if (!read_header(f, *data)) return nullptr;  // data auto-freed
    if (!read_body(f, *data)) return nullptr;    // data auto-freed
    
    return data;  // Ownership transferred to caller. Clear. Explicit.
}

// Ownership is in the type:
std::unique_ptr<Data> data = load_data("input.txt");  // I own it
process_data(*data);  // Borrows reference, doesn't own
// data automatically freed when scope ends
```

No cleanup code. No error path duplication. No leaks. The compiler ensures every allocation is freed exactly once.

**The Convincing Benchmark**

Run both versions under Valgrind or AddressSanitizer:

```bash
# Chad's version
valgrind --leak-check=full ./chad_version
# "definitely lost: 3,847 bytes in 12 blocks"

# C++ version  
valgrind --leak-check=full ./cpp_version
# "All heap blocks were freed -- no leaks are possible"
```

The numbers don't lie. Chad's discipline has gaps. RAII doesn't.

---

### Myth #4: "Function Pointers Are Faster Than Virtual Functions"

**The Belief**

Chad implements polymorphism with structs containing function pointers:

```c
// Chad's "fast" polymorphism
typedef struct Shape {
    void (*draw)(struct Shape*);
    void (*area)(struct Shape*);
    // ... data follows
} Shape;

typedef struct {
    Shape base;
    double radius;
} Circle;

void circle_draw(Shape* s) {
    Circle* c = (Circle*)s;
    // draw circle with radius c->radius
}

Shape* create_circle(double radius) {
    Circle* c = malloc(sizeof(Circle));
    c->base.draw = circle_draw;
    c->base.area = circle_area;
    c->radius = radius;
    return (Shape*)c;
}
```

This is "faster" than virtual functions because... reasons.

**The Reality**

This *is* a virtual function table. Chad has implemented vtables by hand. The function pointer `draw` is looked up at runtime and called indirectly—exactly like C++ virtual dispatch.

Except worse:
- No compiler optimization across virtual calls
- No devirtualization when the type is known
- No type checking on the casts
- No automatic destructor chaining

```cpp
// C++: same mechanism, safer, often faster
class Shape {
public:
    virtual void draw() = 0;
    virtual double area() = 0;
    virtual ~Shape() = default;
};

class Circle : public Shape {
public:
    explicit Circle(double r) : radius_(r) {}
    
    void draw() override { /* draw */ }
    double area() override { return 3.14159 * radius_ * radius_; }
    
private:
    double radius_;
};
```

The compiler generates the same vtable mechanism. But it can also optimize:

```cpp
// Compiler can devirtualize when type is known
Circle c(5.0);
c.draw();  // Direct call, no vtable lookup - the compiler KNOWS it's a Circle

// Chad's version always goes through function pointer
// Even when the type is obvious
```

Modern compilers perform speculative devirtualization, inline caching, and other optimizations that Chad's manual vtable cannot benefit from.

**How To Convince Chad**

```cpp
// Show the assembly for both versions
// When type is known at compile time:

// Chad's version: still indirect call through function pointer
// call *(%rax)

// C++ version: direct call, possibly inlined
// call Circle::draw()  -- or completely inlined
```

The C++ version is often faster because the compiler has more information.

---

### Myth #5: "Templates Cause Code Bloat"

**The Belief**

Every template instantiation generates new code. `std::vector<int>` and `std::vector<double>` are two complete copies of vector. This "bloat" makes executables huge and caches miss.

**The Reality**

Yes, templates generate specialized code. That's the point—it's what makes them fast. But the "bloat" concern is usually overblown:

1. **You'd write the specialized code anyway.** Chad's `DECLARE_VECTOR(int)` and `DECLARE_VECTOR(double)` macros generate the same "bloat"—he just doesn't call it that.

2. **Linkers deduplicate.** Modern linkers (LLD, Gold, MSVC) use identical code folding (ICF). Functions with identical machine code are merged. Template instantiations that generate the same code become one function.

3. **Unused code is eliminated.** If you instantiate `std::vector<int>` but only use `push_back` and `size`, the compiler and linker remove unused methods.

4. **The alternative is runtime overhead.** `void*` generics require runtime type handling. Templates move that work to compile time. The "bloat" is work that would happen at runtime in C.

**The Measurement**

```bash
# Compile Chad's macro-based vectors
gcc -O3 chad_vectors.c -o chad_version
size chad_version
#    text    data     bss     dec     hex filename
#   14832     648      16   15496    3c88 chad_version

# Compile C++ templates
g++ -O3 cpp_vectors.cpp -o cpp_version
size cpp_version
#    text    data     bss     dec     hex filename
#   14912     728      24   15664    3d30 cpp_version
```

The difference is negligible. Often the C++ version is *smaller* because standard library implementations are heavily optimized.

**When Bloat Matters**

Template bloat is real in extreme cases:
- Instantiating complex templates with hundreds of types
- Header-heavy codebases with templates in headers
- Debug builds (no optimization, no dead code elimination)

But the solution isn't "avoid templates." It's:
- Use explicit instantiation for common types
- Use type erasure (`std::function`, `std::any`) when you have many types
- Measure before optimizing

---

### Myth #6: "I Don't Need Exceptions"

**The Belief**

Exceptions are slow. They require unwinding tables. They make control flow unpredictable. Real programmers use error codes.

```c
// Chad's error handling
typedef enum {
    ERR_OK = 0,
    ERR_FILE_NOT_FOUND,
    ERR_PARSE_ERROR,
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_STATE,
    // ... 50 more ...
} ErrorCode;

ErrorCode process(const char* filename, Data** out_data) {
    FILE* f = fopen(filename, "r");
    if (!f) return ERR_FILE_NOT_FOUND;
    
    Data* data = malloc(sizeof(Data));
    if (!data) {
        fclose(f);
        return ERR_OUT_OF_MEMORY;
    }
    
    ErrorCode err = parse(f, data);
    if (err != ERR_OK) {
        free(data);
        fclose(f);
        return err;
    }
    
    // ... 200 more lines of error checking ...
    
    *out_data = data;
    fclose(f);
    return ERR_OK;
}

// Every caller must check
ErrorCode err = process("input.txt", &data);
if (err != ERR_OK) {
    // Handle error... or forget to check and corrupt state silently
}
```

**The Reality**

The "exceptions are slow" myth comes from ancient implementations and debug builds. Modern C++ exceptions have zero cost on the success path—the unwinding tables are only consulted when an exception is thrown.

```cpp
// C++: exceptions for exceptional cases
Data process(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) throw FileNotFoundError(filename);
    
    Data data;
    parse(f, data);  // throws on error
    
    return data;  // No error checking clutter
}

// Caller
try {
    auto data = process("input.txt");
    use(data);
} catch (const FileNotFoundError& e) {
    log_error(e.what());
} catch (const ParseError& e) {
    log_error(e.what());
}
```

**The Performance Reality**

| Scenario | Error Codes | Exceptions |
|----------|-------------|------------|
| Success path | Branch at every call | Zero overhead |
| Error path | Branch + propagation | Unwinding (slower) |
| Typical ratio | 99.9% success | 99.9% success |
| Net effect | Constant small cost | Amortized near-zero |

Exceptions are slower when thrown. But errors are (should be) rare. The success path—which executes 99.9% of the time—is faster with exceptions because there are no branches.

**The Real Problem With Error Codes**

```c
// What happens when someone forgets to check?
ErrorCode err = dangerous_operation();
// Forgot: if (err != ERR_OK) { ... }
continue_assuming_success();  // Undefined behavior
```

Ignoring an exception is impossible. Ignoring an error code is the default.

**The Middle Ground: Expected**

If you want explicit error handling without exceptions:

```cpp
// Best of both worlds
Expected<Data, Error> process(const std::string& filename) {
    std::ifstream f(filename);
    if (!f) return Unexpected(Error::FileNotFound);
    
    auto data = parse(f);
    if (!data) return Unexpected(data.error());
    
    return *data;
}

// Caller must handle the error - it's in the return type
auto result = process("input.txt");
if (!result) {
    handle_error(result.error());
    return;
}
use(*result);
```

---

## Technical Reference: C → C++ Patterns

### Direct Equivalents (Zero Overhead)

| C | C++ | Notes |
|---|-----|-------|
| `malloc(sizeof(T))` | `new T` | Constructor called |
| `malloc(n * sizeof(T))` | `new T[n]` | Or `std::vector<T>(n)` |
| `free(p)` | `delete p` | Destructor called |
| `free(arr)` | `delete[] arr` | Or vector destructor |
| `realloc(p, n)` | `vec.resize(n)` | With `std::vector` |
| `memcpy(d, s, n)` | `std::copy(s, s+n, d)` | Or `std::memcpy` |
| `memset(p, 0, n)` | `std::fill(p, p+n, 0)` | Or `std::memset` |
| `qsort(arr, n, sz, cmp)` | `std::sort(arr, arr+n)` | Often 2x faster |
| `bsearch(...)` | `std::binary_search(...)` | Type-safe |
| `struct S { ... };` | `struct S { ... };` | Identical |
| `typedef struct { } S;` | `struct S { };` | No typedef needed |
| Function pointer | `std::function` or template | See below |

### Memory Management Transformations

```c
// C: manual allocation
Data* data = malloc(sizeof(Data));
if (!data) return NULL;
init_data(data);
// ... use data ...
cleanup_data(data);
free(data);
```

```cpp
// C++: RAII with unique_ptr
auto data = std::make_unique<Data>();
// ... use data ...
// Automatically cleaned up
```

```c
// C: manual array
int* arr = malloc(n * sizeof(int));
if (!arr) return NULL;
// ... use arr ...
free(arr);
```

```cpp
// C++: vector
std::vector<int> arr(n);
// ... use arr ...
// Automatically freed
```

```c
// C: shared ownership (refcount)
typedef struct {
    int refcount;
    Data data;
} SharedData;

SharedData* share(SharedData* s) {
    s->refcount++;
    return s;
}

void release(SharedData* s) {
    if (--s->refcount == 0) {
        cleanup(&s->data);
        free(s);
    }
}
```

```cpp
// C++: shared_ptr
auto data = std::make_shared<Data>();
auto copy = data;  // Refcount incremented
// Both automatically released when last reference dies
```

### Struct Transformation

```c
// C: struct with init/cleanup functions
typedef struct {
    char* name;
    int* data;
    size_t size;
} Thing;

int thing_init(Thing* t, const char* name, size_t size) {
    t->name = strdup(name);
    if (!t->name) return -1;
    
    t->data = malloc(size * sizeof(int));
    if (!t->data) {
        free(t->name);
        return -1;
    }
    
    t->size = size;
    return 0;
}

void thing_cleanup(Thing* t) {
    free(t->name);
    free(t->data);
}

// Usage
Thing t;
if (thing_init(&t, "example", 100) < 0) {
    // handle error
}
// ... use t ...
thing_cleanup(&t);  // DON'T FORGET!
```

```cpp
// C++: class with constructor/destructor
class Thing {
public:
    Thing(std::string name, size_t size)
        : name_(std::move(name))
        , data_(size)
    {}
    
    // Destructor automatic - members clean themselves up
    
    const std::string& name() const { return name_; }
    std::span<int> data() { return data_; }
    
private:
    std::string name_;
    std::vector<int> data_;
};

// Usage
Thing t("example", 100);  // Can't forget initialization
// ... use t ...
// Can't forget cleanup - destructor runs automatically
```

### Function Pointer Transformation

```c
// C: callback with void* context
typedef void (*Callback)(int value, void* context);

void process(int* data, size_t n, Callback cb, void* context) {
    for (size_t i = 0; i < n; ++i) {
        cb(data[i], context);
    }
}

// Usage
void my_callback(int value, void* ctx) {
    int* sum = (int*)ctx;  // Hope this cast is right!
    *sum += value;
}

int sum = 0;
process(data, n, my_callback, &sum);
```

```cpp
// C++: template for zero overhead
template<typename Func>
void process(std::span<int> data, Func&& callback) {
    for (int value : data) {
        callback(value);
    }
}

// Usage with lambda - inlined, zero overhead
int sum = 0;
process(data, [&sum](int value) {
    sum += value;
});

// Or with std::function for type-erased callbacks
void process(std::span<int> data, std::function<void(int)> callback);
```

### Error Handling Transformation

```c
// C: error codes
typedef struct {
    int code;
    char message[256];
} Error;

int parse_file(const char* path, Data* out, Error* err) {
    FILE* f = fopen(path, "r");
    if (!f) {
        err->code = ERR_FILE;
        snprintf(err->message, 256, "Cannot open: %s", path);
        return -1;
    }
    // ... parsing ...
    return 0;
}

// Usage
Data data;
Error err;
if (parse_file("input.txt", &data, &err) < 0) {
    fprintf(stderr, "Error %d: %s\n", err.code, err.message);
    return 1;
}
```

```cpp
// C++: Expected<T, E> for explicit error handling
Expected<Data, Error> parse_file(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        return Unexpected(Error{ERR_FILE, "Cannot open: " + path});
    }
    
    Data data;
    // ... parsing ...
    return data;
}

// Usage
auto result = parse_file("input.txt");
if (!result) {
    std::cerr << "Error " << result.error().code 
              << ": " << result.error().message << "\n";
    return 1;
}
Data& data = *result;
```

### Macro Transformation

```c
// C: macro for "generic" min
#define MIN(a, b) ((a) < (b) ? (a) : (b))

// Problems:
// MIN(x++, y)  - x incremented twice if x < y
// MIN(expensive_func(), cheap_val)  - expensive_func called twice
// No type checking
```

```cpp
// C++: template - evaluated once, type-safe
template<typename T>
constexpr T min(T a, T b) {
    return a < b ? a : b;
}

// Or just use std::min
auto m = std::min(x++, y);  // x incremented exactly once
```

```c
// C: macro for compile-time array size
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Problem: silently wrong for pointers
void func(int arr[]) {
    size_t n = ARRAY_SIZE(arr);  // WRONG - arr is pointer, gives sizeof(ptr)/sizeof(int)
}
```

```cpp
// C++: std::size or template that fails on pointers
template<typename T, size_t N>
constexpr size_t array_size(T (&)[N]) { return N; }

void func(int arr[]) {
    size_t n = array_size(arr);  // COMPILE ERROR - can't deduce N
}

std::array<int, 10> arr;
size_t n = std::size(arr);  // Correct: 10
```

---

## Survival Strategies

### Strategy #1: The Compiler Output Gambit

Chad respects the machine. Show him what the machine actually does.

```bash
# Compile both versions with optimization
gcc -O3 -S chad_version.c
g++ -O3 -S cpp_version.cpp

# Compare the assembly
vimdiff chad_version.s cpp_version.s
```

When the assembly is identical—and it usually is—the performance argument evaporates. When the C++ version has fewer instructions (devirtualization, inlining), Chad must confront reality.

### Strategy #2: The Bug Catalog

Chad's code has bugs. Memory leaks, use-after-free, buffer overflows, type confusion. He doesn't know about them because C doesn't tell him.

Before proposing any migration, run his code under:
- AddressSanitizer (`-fsanitize=address`)
- UndefinedBehaviorSanitizer (`-fsanitize=undefined`)
- Valgrind
- Static analyzers (clang-tidy, PVS-Studio, Coverity)

Collect the bugs. Present them without judgment. Then show how C++ prevents each category:

| Bug | C | C++ |
|-----|---|-----|
| Use-after-free | AddressSanitizer finds it | `unique_ptr` makes it impossible |
| Memory leak | Valgrind finds it | RAII prevents it |
| Buffer overflow | Runtime crash (maybe) | `std::vector` bounds checks |
| Type confusion | Silent corruption | Compiler error |

### Strategy #3: The Incremental Migration

Don't propose rewriting Chad's magnum opus. Propose improving one module.

1. **Wrap, don't rewrite.** Keep Chad's C code. Call it from C++. Add safety at the boundary.

```cpp
// Wrapper that adds safety around Chad's C code
class SafeProcessor {
public:
    SafeProcessor() : handle_(chad_create(), chad_destroy) {}
    
    void process(std::span<const double> data) {
        chad_process(handle_.get(), data.data(), data.size());
    }
    
private:
    std::unique_ptr<ChadHandle, decltype(&chad_destroy)> handle_;
};
```

2. **Replace one function at a time.** The critical path function that leaks memory. The serialization code with buffer overflows. Fix one, prove it works, repeat.

3. **Maintain C ABI compatibility.** Chad can still call the code from C. He can still read the interface. He doesn't feel locked out.

### Strategy #4: The Performance Paradox

Find a place where C++ is faster. They exist.

```cpp
// std::sort vs qsort
// std::sort is 2-3x faster because:
// - Inlines comparison function (qsort uses function pointer)
// - Uses introsort (quicksort + heapsort + insertion sort)
// - Optimized for the specific type

std::vector<int> data = /* ... */;

// qsort: ~850ms for 10M elements
qsort(data.data(), data.size(), sizeof(int), compare_ints);

// std::sort: ~280ms for 10M elements
std::sort(data.begin(), data.end());
```

When C++ is 3x faster than C for basic sorting, Chad must reconsider his worldview.

### Strategy #5: The Third-Party Leverage

Chad respects certain authorities. Use them.

- "The Linux kernel is adding C++ for the Rust interop layer."
- "SQLite's author uses C++ internally now."
- "Game engines (Unreal, Unity's C++ parts) are C++."
- "High-frequency trading firms use C++, not C."
- "LLVM/Clang are written in C++."

If the experts Chad respects use C++, maybe it's not just for the weak.

---

## The Exit Strategy

Some C programmers will never convert. They've built their identity around C. Admitting that C++ is equally fast would mean admitting years of unnecessary struggle.

When you encounter an immovable Chad, your options are:

1. **Isolate the C.** Keep Chad's code in a well-defined library with a C interface. Write new code in C++. The codebase slowly becomes C++ with a C tumor that nobody touches.

2. **Let bugs be the teacher.** Document every memory bug. Every security vulnerability. Every hour spent debugging issues that C++ would have prevented. Eventually, management notices the pattern.

3. **Wait for turnover.** Chad will retire or move to another company. His successor, untainted by C ideology, will see the code with fresh eyes and immediately understand why it needs to change.

4. **Leave.** Life is short. Work somewhere that values your time.

---

## Appendix: Quick Reference Card

### The Mindset Shift

| C Thinking | C++ Thinking |
|------------|--------------|
| I manage memory | The compiler manages memory |
| void* is flexible | void* is a type system escape hatch |
| Function pointers are fast | Virtual functions are the same, but safer |
| Macros are generic programming | Templates are generic programming |
| Error codes are explicit | Exceptions or Expected are explicit AND safe |
| I see every instruction | The compiler sees more instructions than I do |
| Abstractions cost performance | Abstractions enable optimizations |
| If it compiles, I can debug it | If it compiles, it's probably correct |

### The Conversion Checklist

- [ ] Memory leaks identified (Valgrind, ASan)
- [ ] `malloc`/`free` → RAII (unique_ptr, vector, string)
- [ ] `void*` generics → templates or std::variant
- [ ] Function pointers → lambdas or std::function
- [ ] Manual vtables → inheritance and virtual
- [ ] Error codes → Expected<T, E> or exceptions
- [ ] Macros → constexpr, templates, inline functions
- [ ] C strings → std::string / std::string_view
- [ ] Raw arrays → std::vector / std::array / std::span
- [ ] Struct init/cleanup → constructor/destructor
- [ ] C casts → static_cast / dynamic_cast
- [ ] Benchmarks proving equivalent or better performance

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "Your code is full of memory bugs." | "I found some edge cases under AddressSanitizer." |
| "void* is not type-safe." | "We could get compile-time checks with templates." |
| "This has undefined behavior." | "The sanitizers are flagging this section." |
| "C++ is just as fast." | "Let me show you the assembly comparison." |
| "You've reinvented vtables poorly." | "This pattern looks like virtual dispatch—should we use it?" |
| "Your macros are unmaintainable." | "The template version has better error messages." |
| "RAII would prevent this bug." | "What if we tied the lifetime to the scope?" |
| "You're wasting time on solved problems." | "std::vector is heavily optimized for this use case." |

---

*The machine doesn't care what language you write. It executes instructions. Write the code that produces the best instructions with the fewest bugs.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 71%  
**Survival probability without:** 15%  
**Probability Chad benchmarks incorrectly to "disprove" this guide:** 94%
