---
doc_id: UM-REFLECTION-001
doc_type: "User Manual"
title: "Reflection"
fatp_components: ["Reflection", "Reflectable", "FieldAccessor", "Field"]
topics: ["compile-time reflection", "struct field iteration", "NTTP field names", "macro-based registration", "visitor pattern", "generic programming", "serialization", "debug output", "member pointers", "template specialization", "preprocessor"]
constraints: ["no native C++ reflection", "macro registration required", "public fields only", "32-field maximum", "global scope registration"]
cxx_standard: "C++20"
std_equivalent: "P2996 (proposed for C++26)"
boost_equivalent: "Boost.PFR"
build_modes: ["Debug", "Release"]
last_verified: "2026-02-10"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# User Manual - Reflection

*Updated February 2026*

---

## Table of Contents

1. [The Function You've Written a Hundred Times](#the-function-youve-written-a-hundred-times)
2. [Why C++ Can't Do What Python Does](#why-cpp-cant-do-what-python-does)
3. [Member Pointers: The Foundation](#member-pointers-the-foundation)
4. [How the Registration Macro Actually Works](#how-the-registration-macro-actually-works)
5. [Getting Started](#getting-started)
6. [Accessing Fields by Index](#accessing-fields-by-index)
7. [Field Names and the NTTP Trick](#field-names-and-the-nttp-trick)
8. [Visiting All Fields: The Fold Expression Engine](#visiting-all-fields-the-fold-expression-engine)
9. [Looking Up Fields by Name](#looking-up-fields-by-name)
10. [Tuple Conversion](#tuple-conversion)
11. [Debug Strings](#debug-strings)
12. [Nested Structs](#nested-structs)
13. [Working with Const Objects](#working-with-const-objects)
14. [The Two-Stage Registration Rule](#the-two-stage-registration-rule)
15. [Practical Patterns](#practical-patterns)
16. [The C++26 Transition Path](#the-cpp26-transition-path)
17. [Limitations](#limitations)
18. [Troubleshooting](#troubleshooting)
19. [API Reference](#api-reference)

---

## The Function You've Written a Hundred Times

Every C++ programmer who has maintained a codebase for more than a year has written some version of this:

```cpp
void log_employee(const Employee& e) {
    std::cout << "name=" << e.name
              << " department=" << e.department
              << " salary=" << e.salary
              << " start_date=" << e.start_date << "\n";
}
```

And then written this:

```cpp
json serialize_employee(const Employee& e) {
    json j;
    j["name"] = e.name;
    j["department"] = e.department;
    j["salary"] = e.salary;
    j["start_date"] = e.start_date;
    return j;
}
```

And then this:

```cpp
bool employees_equal(const Employee& a, const Employee& b) {
    return a.name == b.name
        && a.department == b.department
        && a.salary == b.salary
        && a.start_date == b.start_date;
}
```

Three functions doing fundamentally the same thing—enumerating the fields of `Employee` and applying an operation to each one. The pattern is obvious. In any language with reflection, you'd write a single generic function that iterates the fields and applies the operation. In C++, you write the same field list over and over.

The cost isn't the initial writing. It's what happens six months later when someone adds `std::string employee_id;` to the struct. They update the serializer because the JSON tests broke. They update the deserializer for the same reason. But the logger compiles fine without the new field—it just silently omits `employee_id` from the output. The comparator compiles fine too—it just considers two employees equal even if they have different IDs.

The compiler cannot catch these omissions. It has no concept of "this function should mention every field of Employee." The omission is a semantic error, invisible to static analysis, visible only at runtime—and often not even then, not until the exact scenario where the missing field matters.

Reflection exists to make this class of bug impossible. Register a type's fields once, in one place, and write your logger, serializer, and comparator as generic visitors over "all fields." Add a field, add it to the registration, and every consumer automatically picks it up.

---

## Why C++ Can't Do What Python Does

To understand why this requires a library, you need to understand what happens to type information during compilation.

### The Compilation Model

When the C++ compiler processes your source file, it builds an abstract syntax tree (AST) containing complete type information. For a struct like:

```cpp
struct Config {
    std::string host;
    int port;
    bool use_tls;
    double timeout;
};
```

The AST node for `Config` records four members with their names, types, sizes, and offsets. The compiler uses this information throughout the compilation pipeline—resolving member access expressions, generating constructor code, computing argument passing conventions.

When compilation reaches the code generation phase, the compiler emits machine instructions that use the computed offsets directly. `config.port` becomes something like "load the 4-byte integer at address `config_base + 32`." The name `"port"` has served its purpose—resolving the source expression to an offset—and is discarded. It does not appear in the object file.

This is fundamentally different from what happens in Java or C#. The Java compiler emits bytecode that says "access the field named `port` on the object of type `Config`." The JVM resolves that name at class-loading time, looking up `port` in Config's metadata table. The metadata table ships in the compiled `.class` file and is available through the reflection API at runtime.

C++ chose not to ship that metadata table. The cost would be real: every struct in every translation unit would carry a table of strings and offsets in the compiled binary. For the systems programming domain where C++ lives—operating system kernels, embedded firmware, game engines, HPC clusters—that overhead is unacceptable.

### The Information Asymmetry

The result is an asymmetry: the compiler has complete knowledge of your types, but provides almost no mechanism for your code to query that knowledge.

You can ask `sizeof(Config)` and `alignof(Config)`. You can form `&Config::port` and get a pointer-to-member. You can use `std::is_trivially_copyable<Config>` to query certain type properties. But you cannot write a template that says "for each field in T, call this function." The compiler knows the answer, but the language provides no syntax to ask the question.

This is the gap that reflection libraries fill. They use the mechanisms C++ *does* provide—macros, templates, and member pointers—to reconstruct the metadata that the compiler knows but won't expose.

---

## Member Pointers: The Foundation

Before diving into the API, it's worth understanding what a pointer-to-member actually is, because the entire reflection system is built on this primitive.

### What &T::field Really Is

In most programming languages, when you take the address of something, you get a memory address—a number identifying a byte in your process's address space. In C++, `&config.port` gives you exactly that: an `int*` pointing at the specific `port` member of the specific `config` object.

But `&Config::port`—without an object—gives you something different. It's not the address of any specific port member, because you haven't specified *which* Config object. Instead, it's a *pointer-to-data-member*: a value representing "the port field, in any Config instance."

On most ABIs, this value is simply the byte offset of the field within the struct. If `Config` lays out as:

```
Offset 0:  std::string host       (32 bytes on most 64-bit platforms)
Offset 32: int port               (4 bytes)
Offset 36: bool use_tls           (1 byte + 3 padding)
Offset 40: double timeout         (8 bytes)
```

then `&Config::port` holds the value 32. To read the port from a specific config, the compiler generates `*(int*)(config_address + 32)`.

The type of this value is `int Config::*`—"pointer to an int member of Config." You use it with the `.*` operator:

```cpp
int Config::* mp = &Config::port;  // Holds offset 32
Config c{"localhost", 8080, true, 30.0};
int value = c.*mp;                  // Reads the int at c + 32
```

### Why This Enables Reflection

Member pointers can be template parameters. When you write:

```cpp
template <typename Class, typename FieldType, auto Ptr>
struct FieldInfo {
    static decltype(auto) get(Class& obj) { return obj.*Ptr; }
};
```

And instantiate it as `FieldInfo<Config, int, &Config::port>`, the compiler knows the member pointer at compile time. The `get()` function compiles to a direct memory access at offset 32—identical to `obj.port`. No lookup, no indirection, no vtable.

This is the key insight: a member pointer carries the same information as a Java `Field` object (which field, what type, how to access it), but as a compile-time constant rather than a runtime value. By encoding member pointers as template parameters, you get the introspection capability of runtime reflection with the performance of direct access.

---

## How the Registration Macro Actually Works

The macro `FATP_REFLECT_REGISTER(Type, x, y, z)` must transform its arguments into a template specialization. Understanding the expansion clarifies both the power and the limitations of the system.

### Step 1: Stringification

For each field name, the macro needs two things: the member pointer `&Type::x` and the string `"x"`. The member pointer comes from normal C++ syntax. The string comes from the preprocessor's stringification operator `#`:

```cpp
#define FATP_REFLECT_FIELD(Type, field) \
    Field<Type, decltype(Type::field), &Type::field, #field>{}
```

When the preprocessor sees `FATP_REFLECT_FIELD(Config, port)`, it expands to:

```cpp
Field<Config, decltype(Config::port), &Config::port, "port">{}
```

The `#field` turns the token `port` into the string literal `"port"`. This is the only mechanism C++ provides for converting source-code identifiers into strings, and it only works through the preprocessor—there is no equivalent in template metaprogramming.

### Step 2: The Counting Problem

The macro needs to generate different code depending on how many fields you pass. `FATP_REFLECT_REGISTER(Type, x, y)` should produce a 2-element tuple. `FATP_REFLECT_REGISTER(Type, a, b, c, d, e)` should produce a 5-element tuple.

The preprocessor has no counting facility. It doesn't know how many arguments are in `__VA_ARGS__`. The standard trick, used by preprocessor libraries since at least Boost.Preprocessor in the early 2000s, is a counting macro with fixed positional parameters where the count "falls through" to the correct position:

```cpp
#define FATP_REFLECT_COUNT_IMPL(                              \
    _1,_2,_3,_4,_5,_6,_7,_8,_9,_10,                         \
    _11,_12,_13,_14,_15,_16,_17,_18,_19,_20,                 \
    _21,_22,_23,_24,_25,_26,_27,_28,_29,_30,_31,_32,         \
    N, ...) N

#define FATP_REFLECT_COUNT(...) \
    FATP_REFLECT_COUNT_IMPL(__VA_ARGS__, 32,31,30,...,3,2,1)
```

When you call `FATP_REFLECT_COUNT(x, y, z)`, the preprocessor substitutes: `x` goes to `_1`, `y` to `_2`, `z` to `_3`, then the trailing numbers 32, 31, 30... shift until `N` lands on `3`. The count falls through to the correct position.

### Step 3: The Map Problem

Knowing the count isn't enough. The macro must *apply* a transformation to each field. `FATP_REFLECT_REGISTER(Type, a, b, c)` needs to generate `FIELD(Type, a), FIELD(Type, b), FIELD(Type, c)`. Since the preprocessor can't loop, you need a separate macro for each arity:

```cpp
#define FATP_REFLECT_MAP_1(m, T, a)          m(T, a)
#define FATP_REFLECT_MAP_2(m, T, a, b)       m(T, a), m(T, b)
#define FATP_REFLECT_MAP_3(m, T, a, b, c)    m(T, a), m(T, b), m(T, c)
// ... all the way to FATP_REFLECT_MAP_32
```

This is the source of the ~500 lines of boilerplate in Reflection.h. Every macro-based reflection library has this same code—Boost.Fusion, Boost.Describe, refl-cpp, Qt's MOC. It is the irreducible cost of doing iteration in a language whose preprocessor was designed in 1972 and has never been extended with control flow.

### Step 4: Assembly

The top-level macro assembles the pieces:

```cpp
#define FATP_REFLECT_REGISTER(Type, ...)                              \
    template <>                                                       \
    struct fat_p::Reflectable<Type> {                                 \
        static constexpr auto fields = std::make_tuple(               \
            FATP_REFLECT_MAP(FATP_REFLECT_FIELD, Type, __VA_ARGS__)   \
        );                                                            \
        static constexpr size_t field_count =                         \
            FATP_REFLECT_COUNT(__VA_ARGS__);                          \
        /* ... accessor methods ... */                                \
    };
```

After macro expansion and template instantiation, the compiler has a complete compile-time model of the type's fields—member pointers for access, string NTTPs for names, type aliases for each field's type—all stored in the type system with zero runtime cost.

### MSVC Complications

MSVC has a long-standing bug where `__VA_ARGS__` is not expanded properly when passed through nested macros. The standard says macro arguments should be expanded before substitution, but MSVC sometimes passes the entire `__VA_ARGS__` pack as a single argument to the inner macro. Reflection.h includes workaround macros (`FATP_EXPAND`, `FATP_EXPAND1`, `FATP_EXPAND2`, `FATP_EXPAND3`) that force multiple rounds of expansion. On GCC and Clang, these are identity operations. On MSVC, they provide the extra expansion the compiler needs.

---

## Getting Started

### Include

```cpp
#include "Reflection.h"
```

### Define Your Type

Define your struct normally. Reflection requires public member variables—it works through member pointers, which can only form to public members:

```cpp
namespace app {
    struct Sensor {
        std::string id;
        double temperature;
        double humidity;
        int battery_percent;
    };
}
```

### Register

Registration must happen at global scope (explained in the Two-Stage Registration Rule section). Use the fully qualified type name with a leading `::`:

```cpp
FATP_REFLECT_REGISTER(::app::Sensor, id, temperature, humidity, battery_percent)
```

Optionally, place a `FATP_REFLECT_DECLARE` inside the namespace for documentation:

```cpp
namespace app {
    FATP_REFLECT_DECLARE(Sensor, id, temperature, humidity, battery_percent);
}
```

### Use

Once registered, the full API is available:

```cpp
app::Sensor s{"TH-042", 23.5, 61.2, 87};

fat_p::visit_fields(s, [](std::string_view name, const auto& value) {
    std::cout << name << ": ";
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::string>)
        std::cout << value;
    else if constexpr (std::is_floating_point_v<T>)
        std::cout << value;
    else
        std::cout << value;
    std::cout << "\n";
});
```

Output:

```
id: TH-042
temperature: 23.5
humidity: 61.2
battery_percent: 87
```

That visitor works identically for any registered type. You write the logic once. Reflection handles the enumeration.

---

## Accessing Fields by Index

The most direct access method. `get_field<I>(obj)` returns a reference to the I-th field:

```cpp
Point p{10, 20};
int x = fat_p::get_field<0>(p);   // 10
int y = fat_p::get_field<1>(p);   // 20
```

Because `I` is a template parameter, the compiler resolves it at compile time. The generated code is indistinguishable from `p.x` and `p.y`—the same memory access at the same offset, with no additional instructions.

The return value is a reference, so fields can be modified:

```cpp
fat_p::get_field<0>(p) = 100;  // p.x is now 100
```

An out-of-bounds index produces a `static_assert` failure:

```cpp
fat_p::get_field<5>(p);  // Compile error: "Field index out of bounds"
```

This is a significant advantage over runtime reflection. In Java, accessing a field by invalid index throws `IndexOutOfBoundsException` at runtime. In C++, the error is caught at compile time—the program cannot be built with an invalid field access.

You can query the field count with `field_count<T>()`:

```cpp
static_assert(fat_p::field_count<Point>() == 2);
```

---

## Field Names and the NTTP Trick

### Why Field Names Are Hard

Retrieving a field's name is what separates useful reflection from an academic exercise. Without names, you can iterate fields by index—useful for generic algorithms—but you can't produce human-readable output, can't serialize to named formats like JSON, can't map to database columns.

The difficulty is that field names are source-code identifiers. They exist in the source file as text. The preprocessor can turn them into string literals using `#field`. But where do those strings live?

Before C++20, the answer was: in static storage. Each Field type would contain `static constexpr const char* name = "temperature"`. The string data occupies bytes in the binary. It's accessible at runtime. But the name isn't part of the *type itself*—two fields with different names but the same class, member type, and offset would be the same type.

C++20 changed this with non-type template parameters (NTTPs) for literal class types. A `fixed_string` struct that holds a char array qualifies:

```cpp
template <size_t N>
struct fixed_string {
    char data[N];
    constexpr fixed_string(const char (&str)[N]) {
        for (size_t i = 0; i < N; ++i) data[i] = str[i];
    }
    constexpr std::string_view view() const { return {data, N - 1}; }
};
```

Now `"temperature"` can be a template parameter. The compiler deduces `fixed_string<12>{"temperature"}` and embeds it in the type system. `Field<Sensor, double, &Sensor::temperature, "temperature">` is a unique type that carries the field name without runtime storage. Two fields with different names are different types, distinguishable at compile time.

### Using Field Names

`get_field_name<I, T>()` returns a `constexpr std::string_view`:

```cpp
auto name = fat_p::get_field_name<0, Sensor>();  // "id"
```

`get_field_names<T>()` returns all names as a compile-time array:

```cpp
auto names = fat_p::get_field_names<Sensor>();
// std::array<std::string_view, 4>{"id", "temperature", "humidity", "battery_percent"}
```

These are compile-time constants usable in `constexpr` contexts and `static_assert` conditions. They don't allocate memory and don't involve string operations at runtime.

---

## Visiting All Fields: The Fold Expression Engine

### How It Works Under the Hood

`visit_fields` is where the generic programming power lives. Understanding its implementation illuminates how C++ templates simulate the iteration that other languages provide natively.

The implementation uses three C++ features: `std::index_sequence`, parameter pack expansion, and fold expressions.

First, `visit_fields` generates an index sequence matching the field count:

```cpp
template <typename T, typename Visitor>
constexpr void visit_fields(T& obj, Visitor&& visitor) {
    detail::visit_fields_impl(
        obj,
        std::forward<Visitor>(visitor),
        std::make_index_sequence<field_count<T>()>{}  // {0, 1, 2, 3}
    );
}
```

For a type with 4 fields, `std::make_index_sequence<4>` produces the type `std::index_sequence<0, 1, 2, 3>`. This type carries the indices as template parameters—it has no runtime representation.

The implementation function uses a fold expression to expand the pack:

```cpp
template <typename T, typename Visitor, size_t... Is>
constexpr void visit_fields_impl(T& obj, Visitor&& v, std::index_sequence<Is...>) {
    (v(get_field_name<Is, T>(), get_field<Is>(obj)), ...);
}
```

The `(expr, ...)` is a C++17 fold expression over the comma operator. For `Is = {0, 1, 2, 3}`, it expands to:

```cpp
v(get_field_name<0, T>(), get_field<0>(obj)),
v(get_field_name<1, T>(), get_field<1>(obj)),
v(get_field_name<2, T>(), get_field<2>(obj)),
v(get_field_name<3, T>(), get_field<3>(obj));
```

Each call to `v` passes the concrete field name and a reference with the concrete field type. The compiler generates four separate call sites, each with full type information. If the visitor uses `if constexpr` to dispatch on type, the compiler evaluates the branches at each site and eliminates dead code.

The result is code equivalent to:

```cpp
v("id", obj.id);
v("temperature", obj.temperature);
v("humidity", obj.humidity);
v("battery_percent", obj.battery_percent);
```

No loop. No runtime dispatch. No type erasure. Four inline function calls with full type information, generated at compile time from the registered metadata.

### Writing Effective Visitors

A visitor receives two arguments: a `std::string_view` name and a reference whose type varies per field. Use `if constexpr` to handle different types:

```cpp
fat_p::visit_fields(sensor, [](std::string_view name, const auto& value) {
    using T = std::decay_t<decltype(value)>;

    if constexpr (std::is_integral_v<T>)
        log_integer(name, value);
    else if constexpr (std::is_floating_point_v<T>)
        log_float(name, value);
    else if constexpr (std::is_same_v<T, std::string>)
        log_string(name, value);
    else
        log_unknown(name);
});
```

The `if constexpr` branches are evaluated at compile time for each field. For `Sensor::id` (a `std::string`), only the `log_string` branch is compiled. For `Sensor::temperature` (a `double`), only `log_float`. Dead branches are eliminated entirely.

Visitors can also modify fields by taking a non-const reference:

```cpp
fat_p::visit_fields(sensor, [](std::string_view, auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (std::is_same_v<T, std::string>)
        value = "[REDACTED]";
});
// sensor.id is now "[REDACTED]"
```

---

## Looking Up Fields by Name

Sometimes you need to access a field whose name is determined at runtime—processing a JSON key, applying a configuration override, or routing a command to a specific field. `FieldAccessor<T>` provides this:

```cpp
bool has_port = fat_p::FieldAccessor<Config>::has_field("port");  // true
bool has_foo  = fat_p::FieldAccessor<Config>::has_field("foo");   // false
```

To access a field by name, `visit_field` calls a visitor if the field is found and returns a boolean:

```cpp
bool found = fat_p::FieldAccessor<Config>::visit_field(
    config, "port",
    [](auto& value) {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_integral_v<T>)
            value = 8080;
    }
);
```

The implementation uses recursive `if constexpr` templates that walk the fields one by one, comparing names. For a struct with N fields, this generates at most N string comparisons. The comparison chain is fully unrolled at compile time—there's no loop variable, no iterator state.

For the field counts typical of struct reflection (2–32 fields), linear search is faster than a hash table. String comparison on short field names (averaging 5–10 characters) completes in a few nanoseconds. A hash table would need to hash the query string, find the bucket, and handle collisions—overhead that only pays off at hundreds of entries. At N=8, the entire linear walk fits in a single cache line worth of branch instructions.

---

## Tuple Conversion

`to_tuple(obj)` converts a reflected object into a `std::tuple` of references:

```cpp
Point p{10, 20};
auto t = fat_p::to_tuple(p);

std::get<0>(t) = 50;   // p.x is now 50
std::get<1>(t) = 100;  // p.y is now 100
```

The tuple contains references, not copies. Modification propagates to the original object. This bridges reflected types into the STL ecosystem—code that works with `std::tuple` (like `std::apply`, `std::tuple_cat`, structured bindings) can now operate on any reflected type without knowing its structure.

---

## Debug Strings

During development, you frequently need a quick textual representation of an object—for a log message, a test assertion failure, or a debugger watch expression. `to_debug_string(obj)` generates one:

```cpp
Point p{42, 84};
std::string s = fat_p::to_debug_string(p);
// "Point { x: 42, y: 84 }"

Person person{"Charlie", 35, 175.0};
std::string ps = fat_p::to_debug_string(person);
// "Person { name: \"Charlie\", age: 35, height: 175.000000 }"
```

The function uses `visit_fields` internally, dispatching on type with `if constexpr`: integers and floats go through `std::to_string`, strings are quoted, pointers show "null" or "non-null", and unrecognized types display "?". The type name prefix comes from `type_name<T>()`, which extracts the compiler-generated name from `__PRETTY_FUNCTION__` (GCC/Clang) or `__FUNCSIG__` (MSVC).

This is a development aid, not a serialization format. Field order depends on registration order, string escaping is minimal, and floating-point formatting uses default precision. For machine-readable output, write a proper serializer using `visit_fields`.

---

## Nested Structs

Reflection operates one level at a time. A struct containing another struct sees the inner struct as a single field:

```cpp
struct Labeled {
    Point position;
    std::string label;
    int id;
};
FATP_REFLECT_REGISTER(::app::Labeled, position, label, id)
```

`field_count<Labeled>()` returns 3, not 5. The `position` field is accessible as a `Point&`:

```cpp
Labeled obj{{10, 20}, "Origin", 1};
auto& pos = fat_p::get_field<0>(obj);  // Returns Point&
pos.x = 100;
```

To recurse into nested structs, check whether a field's type is itself reflectable:

```cpp
fat_p::visit_fields(obj, [](std::string_view name, const auto& value) {
    using T = std::decay_t<decltype(value)>;
    if constexpr (fat_p::is_reflectable<T>()) {
        // value is a reflected struct — recurse
        fat_p::visit_fields(value, [](std::string_view inner_name, const auto& inner_val) {
            // Process inner field
        });
    } else {
        // Leaf field
    }
});
```

This pattern composes naturally. A recursive `to_json` function would serialize nested structs as nested JSON objects, and a recursive `to_debug_string` would produce indented output showing the full tree.

---

## Working with Const Objects

All reflection operations provide overloads for both `T&` and `const T&`:

```cpp
const Point p{7, 14};

int x = fat_p::get_field<0>(p);           // Returns const int&
fat_p::visit_fields(p, [](auto, const auto&) { /* ... */ });
auto t = fat_p::to_tuple(p);              // std::tuple<const int&, const int&>
std::string s = fat_p::to_debug_string(p);
```

`FieldAccessor::visit_field` also works with const objects—the visitor receives a const reference and cannot modify the field. This const propagation is enforced at compile time through the template overload set.

---

## The Two-Stage Registration Rule

Registration has a syntactic requirement that trips up first-time users: `FATP_REFLECT_REGISTER` must appear at global scope, not inside a namespace.

The reason is a C++ language rule about template specialization. The macro expands to `template <> struct fat_p::Reflectable<Type> { ... }`. This is an explicit specialization of a template defined in namespace `fat_p`. The standard requires that explicit specializations be declared at namespace scope, in the same namespace as the primary template or an enclosing namespace. Global scope encloses all namespaces, so it always works.

Correct:

```cpp
// At global scope
FATP_REFLECT_REGISTER(::my::app::Config, host, port, use_tls)
```

Also correct:

```cpp
namespace fat_p {
    FATP_REFLECT_REGISTER(::my::app::Config, host, port, use_tls)
}
```

Wrong:

```cpp
namespace my::app {
    FATP_REFLECT_REGISTER(Config, host, port, use_tls)  // Compiler error
}
```

Always use the fully qualified type name with a leading `::`. Without it, name lookup may find an unrelated type in a nested scope. `::my::app::Config` is unambiguous regardless of where the macro appears.

---

## Practical Patterns

### Generic Serialization

A single function that serializes any registered type to JSON:

```cpp
template <typename T>
json to_json(const T& obj) {
    json j;
    fat_p::visit_fields(obj, [&j](std::string_view name, const auto& value) {
        j[std::string(name)] = value;
    });
    return j;
}

json j1 = to_json(sensor);   // Works for Sensor
json j2 = to_json(config);   // Works for Config
json j3 = to_json(employee); // Works for Employee
```

Add a field to any of these types, add it to the registration, and every `to_json` call automatically includes it.

### Generic Equality

Compare any two objects of the same registered type, field by field:

```cpp
template <typename T>
bool reflect_equal(const T& a, const T& b) {
    bool equal = true;
    fat_p::visit_fields(a, [&](std::string_view name, const auto& va) {
        fat_p::FieldAccessor<T>::visit_field(b, name, [&](const auto& vb) {
            using A = std::decay_t<decltype(va)>;
            using B = std::decay_t<decltype(vb)>;
            if constexpr (std::is_same_v<A, B>)
                if (!(va == vb)) equal = false;
        });
    });
    return equal;
}
```

### Configuration Binding

Apply string key-value overrides from any configuration source:

```cpp
template <typename T>
void apply_overrides(T& obj, const std::map<std::string, std::string>& overrides) {
    for (const auto& [key, value] : overrides) {
        fat_p::FieldAccessor<T>::visit_field(obj, key, [&value](auto& field) {
            using F = std::decay_t<decltype(field)>;
            if constexpr (std::is_same_v<F, std::string>)
                field = value;
            else if constexpr (std::is_integral_v<F>)
                field = std::stoi(value);
            else if constexpr (std::is_floating_point_v<F>)
                field = std::stod(value);
        });
    }
}
```

### Struct Diffing

Find which fields differ between two instances:

```cpp
template <typename T>
std::vector<std::string> diff_fields(const T& a, const T& b) {
    std::vector<std::string> changed;
    fat_p::visit_fields(a, [&](std::string_view name, const auto& va) {
        fat_p::FieldAccessor<T>::visit_field(b, name, [&](const auto& vb) {
            using A = std::decay_t<decltype(va)>;
            using B = std::decay_t<decltype(vb)>;
            if constexpr (std::is_same_v<A, B>) {
                if (!(va == vb))
                    changed.push_back(std::string(name));
            }
        });
    });
    return changed;
}
```

---

## The C++26 Transition Path

P2996 proposes native reflection for C++26. If adopted and implemented, code like this becomes possible:

```cpp
template <typename T>
void print_fields(const T& obj) {
    template for (constexpr auto member : std::meta::members_of(^T)) {
        std::cout << std::meta::name_of(member) << ": " << obj.[:member:] << "\n";
    }
}
```

No registration macro. The compiler provides the metadata directly. The `^T` operator creates a "reflection" of type T, `members_of` returns its members, and `[:member:]` splices the reflection back into an expression. It's elegant—and it's not available today.

Fat-P Reflection is designed for this transition. The consumer-facing API—`visit_fields`, `get_field`, `get_field_name`—doesn't expose the macro machinery. A future version of Reflection.h could reimplement these functions against P2996 without changing any call sites. The `FATP_HAS_CPP26_REFLECTION` flag is already stubbed in the header.

The migration path when your compiler supports P2996:

1. Update Reflection.h (or let Fat-P update it)
2. Delete your `FATP_REFLECT_REGISTER` lines
3. Your `visit_fields` and `get_field` calls continue to work

The macro registrations are temporary scaffolding. The API they enable is the structure.

---

## Limitations

**Public fields only.** Member pointers require public access. Private and protected members cannot be registered. If you need to reflect private state, provide public accessors or reconsider the design—private state exists precisely to prevent external access.

**32-field maximum.** The preprocessor `MAP_N` macros are written for arities 1–32. Structs with more than 32 public data members usually indicate a design that should be decomposed into smaller types.

**Global scope registration.** Template specialization rules require it. This is a C++ language constraint.

**No function reflection.** Only data members. Member functions, static members, and free functions cannot be registered.

**Single registration per type.** Multiple registrations produce a "redefinition of specialization" error. Place the registration in exactly one location—typically a `.cpp` file or a dedicated registration header with include guards.

**C++20 required.** NTTP strings require C++20. Compilers targeting C++17 or earlier would need a fundamentally different approach to field names.

---

## Troubleshooting

**"Type must be registered with REFLECT_REGISTER"** — The type hasn't been registered, or the registration is in a translation unit that hasn't been included. Add `FATP_REFLECT_REGISTER(::fully::qualified::Type, field1, field2, ...)` at global scope.

**"Field index out of bounds"** — The template index exceeds `field_count<T>()`. Caught at compile time.

**Logger/serializer doesn't include a new field** — The field was added to the struct but not to the registration macro. Update the `FATP_REFLECT_REGISTER` call.

**MSVC compilation errors in macro expansion** — Use MSVC 2019 16.8 or later. Earlier versions have `__VA_ARGS__` bugs that the workaround macros cannot fully address.

**"Redefinition of specialization"** — The type is registered in a header included by multiple translation units. Move the registration to a single `.cpp` file, or ensure the registration header has proper include guards.

---

---

## Use Case: Generic Serialization

Write a single function that serializes any reflectable struct to JSON:

```cpp
template <typename T>
    requires fat_p::is_reflectable_v<T>
JsonValue serialize(const T& obj)
{
    JsonValue j;
    fat_p::visit_fields(obj, [&](std::string_view name, const auto& value) {
        j[std::string(name)] = value;
    });
    return j;
}

// Works for any registered struct
FATP_REFLECT_REGISTER(Config, host, port, timeout);
Config cfg{"localhost", 8080, 30};
auto j = serialize(cfg);  // {"host": "localhost", "port": 8080, "timeout": 30}
```

## Use Case: Diff / Change Detection

Compare two instances of the same struct and report which fields changed:

```cpp
template <typename T>
    requires fat_p::is_reflectable_v<T>
std::vector<std::string> diff(const T& a, const T& b)
{
    std::vector<std::string> changed;
    fat_p::visit_fields(a, [&](std::string_view name, const auto& val_a) {
        fat_p::FieldAccessor<T>::visit_field(b, name, [&](const auto& val_b) {
            if (val_a != val_b)
                changed.emplace_back(name);
        });
    });
    return changed;
}
```

## Use Case: Debug Logging with to_debug_string

Automatically format any registered struct for logging:

```cpp
FATP_REFLECT_REGISTER(Request, method, url, body_size, auth_token);

void log_request(const Request& req)
{
    spdlog::debug("Incoming: {}", fat_p::to_debug_string(req));
    // Output: "Request { method: GET, url: /api/v1/users, body_size: 0, auth_token: abc123 }"
}
```

## Use Case: ORM-Style Field Mapping

Map struct fields to database columns dynamically:

```cpp
template <typename T>
std::string build_insert_sql(const std::string& table, const T& obj)
{
    auto names = fat_p::get_field_names<T>();
    std::string cols, vals;
    fat_p::visit_fields(obj, [&](std::string_view name, const auto& value) {
        if (!cols.empty()) { cols += ", "; vals += ", "; }
        cols += name;
        vals += quote(value);
    });
    return "INSERT INTO " + table + " (" + cols + ") VALUES (" + vals + ")";
}
```

## Best Practices

**Register at namespace scope, not inside functions.** The `FATP_REFLECT_REGISTER` macro defines template specializations that must be at namespace scope.

**Use visit_fields for generic algorithms, get_field<I> for known indices.** `visit_fields` is the workhorse for serialization/deserialization. `get_field<I>` is for cases where you know the field index at compile time.

**Prefer to_debug_string for logging.** It handles all field types automatically and produces readable output.

**Keep registered structs simple.** Reflection works best with plain data structs. Complex types with private members, virtual functions, or non-copyable fields may require more manual handling.

## Expanded Troubleshooting

### Compile error: "FATP_REFLECT_REGISTER must be at namespace scope"

The macro defines template specializations which cannot be inside a function or class. Move the macro call to namespace scope, after the struct definition.

### visit_fields doesn't see all fields

Only fields listed in `FATP_REFLECT_REGISTER` are visible. If you added a field to the struct but not to the macro, reflection doesn't know about it. Update the macro call.

### to_debug_string crashes with non-streamable types

`to_debug_string` uses `operator<<` for each field. If a field type doesn't have `operator<<`, compilation fails. Provide one, or use `visit_fields` with a custom visitor that handles the type.

### field_count returns 0

The struct is not registered. Verify `FATP_REFLECT_REGISTER` is called and the header is included.

---

## API Reference

### Registration Macros

`FATP_REFLECT_DECLARE(Type, fields...)` — Optional documentation macro. Place inside the type's namespace. Produces a `static_assert` that the type is complete.

`FATP_REFLECT_REGISTER(Type, fields...)` — Registers a type for reflection. Place at global scope. Use fully qualified type name with leading `::`. Supports 1–32 fields.

### Free Functions

`field_count<T>()` — Returns the number of registered fields. `constexpr`.

`get_field<I>(obj)` — Returns a reference to the I-th field. Mutable and const overloads. Compile-time bounds check via `static_assert`.

`get_field_name<I, T>()` — Returns the I-th field name as `constexpr std::string_view`.

`get_field_names<T>()` — Returns all names as `std::array<std::string_view, N>`.

`visit_fields(obj, visitor)` — Calls `visitor(name, field_ref)` for each registered field. Mutable and const overloads.

`to_tuple(obj)` — Returns `std::tuple` of references to all fields.

`to_debug_string(obj)` — Returns formatted `std::string` with type name and all field name-value pairs.

`type_name<T>()` — Returns compiler-dependent type name as `std::string_view`. Uses `__PRETTY_FUNCTION__` (GCC/Clang) or `__FUNCSIG__` (MSVC).

`is_reflectable<T>()` — Returns `true` if `T` has been registered.

### FieldAccessor<T>

`FieldAccessor<T>::has_field(name)` — Returns `true` if `T` has a field with the given name. Linear O(N).

`FieldAccessor<T>::visit_field(obj, name, visitor)` — Calls `visitor(field_ref)` if the named field exists. Returns `true` if found. Linear O(N).

### Field<Class, FieldType, Ptr, Name>

`Field::get(obj)` — Returns a reference to the field.

`Field::set(obj, value)` — Sets the field via perfect forwarding.

`Field::get_name()` — Returns field name as `std::string_view`.

`Field::get_hash()` — Returns `uint64_t` hash of the field name (via `constexpr_hash64`).

---

*Reflection.h — Fat-P Library*
