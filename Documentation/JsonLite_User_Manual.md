# JsonLite User Manual

## Table of Contents

1. [What is JSON and Why JsonLite?](#what-is-json-and-why-jsonlite)
   - [Understanding JSON](#understanding-json)
   - [The C++ JSON Landscape](#the-c-json-landscape)
   - [Where JsonLite Fits](#where-jsonlite-fits)
2. [Core Architecture](#core-architecture)
   - [The Type System](#the-type-system)
   - [Why Variant-Based?](#why-variant-based)
   - [Design Decisions](#design-decisions)
3. [Getting Started](#getting-started)
   - [Prerequisites](#prerequisites)
   - [Integration](#integration)
   - [Compilation](#compilation)
   - [First Program](#first-program)
4. [The Macro System](#the-macro-system)
   - [CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE](#cpp_json_define_type_non_intrusive)
   - [CPP_JSON_DEFINE_TYPE_OPTIONAL](#cpp_json_define_type_optional)
   - [CPP_JSON_DEFINE_TYPE_INTRUSIVE](#cpp_json_define_type_intrusive)
   - [Macro Limitations](#macro-limitations)
5. [Serialization: C++ to JSON](#serialization-c-to-json)
   - [Basic Types](#basic-types)
   - [Containers](#containers)
   - [Structs with Macros](#structs-with-macros)
   - [Converting to JSON String](#converting-to-json-string)
6. [Deserialization: JSON to C++](#deserialization-json-to-c)
   - [Two-Style API](#two-style-api)
   - [When to Use Which API](#when-to-use-which-api)
   - [Basic Types](#basic-types-1)
   - [Numeric Safety](#numeric-safety)
   - [Containers](#containers-1)
   - [Structs with Macros](#structs-with-macros-1)
   - [Direct Key Access](#direct-key-access)
7. [Parsing and Formatting](#parsing-and-formatting)
   - [Parsing JSON Strings](#parsing-json-strings)
   - [Formatting Policies](#formatting-policies)
8. [JSON Pointer (RFC 6901)](#json-pointer-rfc-6901)
   - [What is JSON Pointer?](#what-is-json-pointer)
   - [Basic Navigation](#basic-navigation)
   - [Type-Safe Queries](#type-safe-queries)
   - [Mutable Access](#mutable-access)
   - [Escape Sequences](#escape-sequences)
   - [Error Handling](#error-handling)
9. [File I/O](#file-io)
   - [High-Level Convenience Functions](#high-level-convenience-functions)
   - [Backup and Safe Saving](#backup-and-safe-saving)
   - [Low-Level File Operations](#low-level-file-operations)
   - [Error Handling](#error-handling-1)
10. [Policy System](#policy-system)
    - [StandardJsonPolicy](#standardjsonpolicy)
    - [PrettyJsonPolicy](#prettyjsonpolicy)
    - [CompatJsonPolicy](#compatjsonpolicy)
    - [ConfigJsonPolicy](#configjsonpolicy)
    - [Custom Policies](#custom-policies)
11. [Error Handling](#error-handling-2)
    - [Exception Types](#exception-types)
    - [Example Error Messages](#example-error-messages)
    - [Catching Errors](#catching-errors)
12. [Supported Types Reference](#supported-types-reference)
    - [Fundamental Types](#fundamental-types)
    - [Container Types](#container-types)
    - [Notes on Container Support](#notes-on-container-support)
13. [Advanced Patterns](#advanced-patterns)
    - [Nested Structures](#nested-structures)
    - [Heterogeneous Arrays](#heterogeneous-arrays)
    - [Custom Serialization](#custom-serialization)
    - [Polymorphism](#polymorphism)
14. [Comparison with Other Libraries](#comparison-with-other-libraries)
    - [JsonLite vs nlohmann/json](#jsonlite-vs-nlohmannjson)
    - [JsonLite vs RapidJSON](#jsonlite-vs-rapidjson)
    - [JsonLite vs simdjson](#jsonlite-vs-simdjson)
    - [JsonLite vs Boost.PropertyTree](#jsonlite-vs-boostpropertytree)
    - [JsonLite vs FatPJsonLite](#jsonlite-vs-fatpjsonlite)
15. [Use Case Guide](#use-case-guide)
    - [Configuration Files](#configuration-files)
    - [REST API Responses](#rest-api-responses)
    - [Game Save Files](#game-save-files)
    - [High-Frequency Trading / Real-Time](#high-frequency-trading--real-time)
    - [Large Data Processing](#large-data-processing)
    - [Embedded Systems](#embedded-systems)
    - [Cross-Platform Tools](#cross-platform-tools)
16. [Migration Guide](#migration-guide)
    - [From nlohmann/json to JsonLite](#from-nlohmannjson-to-jsonlite)
    - [From RapidJSON to JsonLite](#from-rapidjson-to-jsonlite)
    - [From Boost.PropertyTree to JsonLite](#from-boostpropertytree-to-jsonlite)
17. [Compiler Requirements](#compiler-requirements)
    - [Minimum Version](#minimum-version)
    - [Tested Compilers](#tested-compilers)
    - [Compilation Flags](#compilation-flags)
    - [Dependencies](#dependencies)
18. [Performance Characteristics](#performance-characteristics)
    - [Parsing Throughput](#parsing-throughput)
    - [Memory Usage](#memory-usage)
    - [Optimization Tips](#optimization-tips)
19. [Best Practices](#best-practices)
    - [Design Patterns](#design-patterns)
    - [Error Handling](#error-handling-3)
    - [Testing](#testing)
20. [Summary](#summary)

---

## What is JSON and Why JsonLite?

### Understanding JSON

JSON (JavaScript Object Notation) is a lightweight data interchange format that's easy for 
humans to read and write, and easy for machines to parse and generate. Despite its name, JSON 
is language-independent and has become the de facto standard for configuration files, REST APIs, 
and data serialization.

A JSON document consists of six value types:
- **null**: Represents absence of a value
- **boolean**: `true` or `false`
- **number**: Integer or floating-point (no distinction in JSON spec)
- **string**: Unicode text in double quotes
- **array**: Ordered list of values `[1, 2, 3]`
- **object**: Unordered collection of key-value pairs `{"name": "value"}`

### The C++ JSON Landscape

The C++ ecosystem has numerous JSON libraries, each with different priorities:
- **nlohmann/json**: Feature-rich, C++11, implicit conversions
- **RapidJSON**: Maximum speed, SAX/DOM parsers, custom allocators
- **simdjson**: SIMD acceleration, read-only, multi-GB/s parsing
- **Boost.PropertyTree**: Multi-format (JSON/XML/INI), but not true JSON semantics

### Where JsonLite Fits

JsonLite is designed for a specific niche: **applications that need simple, safe JSON handling 
for configuration files and parameter persistence with zero external dependencies**.

It makes deliberate trade-offs:
- **Safety over speed**: All numeric conversions check for overflow 
- **Simplicity over features**: Core JSON + JSON Pointer, no streaming, no binary formats 
- **Clarity over convenience**: Explicit conversions, no silent type coercion 
- **Modern C++ over backwards compatibility**: Requires C++17, uses std::variant 

JsonLite is **not** the fastest JSON library. It's **not** the most feature-complete. It **is** 
the right choice when you need to parse configuration files safely without pulling in external 
dependencies or writing boilerplate serialization code.

---

## Core Architecture

### The Type System

JsonLite uses `std::variant` to represent JSON values, providing compile-time type safety:

```cpp
struct JsonValue : std::variant<
    std::nullptr_t,  // JSON null
    bool,            // JSON boolean
    int64_t,         // JSON number (integer)
    double,          // JSON number (floating-point)
    std::string,     // JSON string
    JsonArray,       // JSON array (std::vector<JsonValue>)
    JsonObject       // JSON object (std::map<std::string, JsonValue>)
>
```

All integer types (int, long, short, etc.) are stored as `int64_t` to preserve precision. All 
floating-point types are stored as `double`. This means JsonLite can round-trip any numeric value 
that fits in these types without loss.

### Why Variant-Based?

Many JSON libraries use a class with virtual functions or a union-like structure. JsonLite uses 
`std::variant` because:

1.  **Type Safety**: Cannot accidentally access wrong type - throws `std::bad_variant_access`
2.  **No Heap Overhead**: Small values (bool, int64_t, double) stored inline
3.  **Modern C++**: Works naturally with `std::visit`, pattern matching
4.  **Compile-Time Dispatch**: No virtual function overhead

The downside is more verbose access compared to libraries like nlohmann/json:

```cpp
// nlohmann/json - convenient but type-unsafe
json j = {"name", "Alice"};
std::string name = j["name"];  // Implicit conversion

// JsonLite - verbose but type-safe
JsonObject obj;
obj["name"] = std::string("Alice");
JsonValue j = obj;
std::string name = std::get<std::string>(
    std::get<JsonObject>(j).at("name")
);
```

JsonLite provides a value-returning API to reduce verbosity while maintaining safety:

```cpp
// JsonLite - value-returning API
JsonObject obj;
obj["name"] = std::string("Alice");
const auto name = from_json<std::string>(obj.at("name"));
```

### Design Decisions

**Why int64_t instead of separate int/long types?**
- JSON spec doesn't distinguish integer sizes
- Avoids overflow when round-tripping large integers
- Simplifies implementation

**Why no implicit conversions?**
- A double value of 3.7 shouldn't silently become integer 3
- Explicit conversions catch logic errors at runtime
- Users can opt into conversions with `from_json<T>()`

**Why exceptions instead of error codes?**
- Configuration parsing typically happens at startup
- Exceptions provide rich context (position, field path, expected vs actual type)
- FatPJsonLite provides Expected-based alternative if exceptions are unacceptable

**Why standard containers (std::vector, std::map)?**
- Familiar to all C++ programmers
- Well-tested and optimized by standard library implementations
- FatPJsonLite provides SmallVector and FlatMap for performance-critical code

---

## Getting Started

### Prerequisites

- C++17 or later compiler
- Standard library with `<variant>`, `<optional>`, `<string_view>`

### Integration

JsonLite is a single-header library. Copy `JsonLite.h` to your project and include it:

```cpp
#include "JsonLite.h"
USING_JSON_LITE()
```

The `USING_JSON_LITE()` macro expands to:

```cpp
using namespace fat_p;
using fat_p::to_json;
using fat_p::from_json;
```

This brings both the fat_p namespace and the serialization functions into scope. The explicit 
`using` declarations for `to_json` and `from_json` are required for Argument Dependent Lookup 
(ADL) to work when you define custom serialization for your types.

**Important**: Do NOT use `using namespace fat_p;` without the macro when defining custom 
serialization, as it won't bring the ADL-required function declarations into scope.

### Compilation

**Minimum flags:**
```bash
g++ -std=c++17 -I/path/to/headers main.cpp -o app
```

**Recommended flags:**
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I/path/to/headers main.cpp -o app
```

**Debug with sanitizers:**
```bash
g++ -std=c++17 -g -fsanitize=address,undefined -I/path/to/headers main.cpp -o app
```

### First Program

```cpp
#include "JsonLite.h"
USING_JSON_LITE()

#include <iostream>

struct Config
{
    int port;
    std::string host;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, timeout)

int main()
{
    Config cfg{8080, "localhost", 30};
    
    save_params("config.json", cfg);
    
    Config loaded = load_params<Config>("config.json");
    
    std::cout << "Port: " << loaded.port << "\n";
    std::cout << "Host: " << loaded.host << "\n";
    if (loaded.timeout)
    {
        std::cout << "Timeout: " << *loaded.timeout << "\n";
    }
    
    return 0;
}
```

This program:
1. Defines a `Config` struct with three fields
2. Uses the macro to automatically generate serialization code
3. Saves the config to a JSON file
4. Loads it back and prints the values

---

## The Macro System

JsonLite provides three macros for automatic struct serialization. These macros generate 
`to_json()` and `from_json()` functions for your types.

### CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE

This macro defines serialization functions **outside** the class definition. Use this for types 
you don't control or when you want to keep serialization logic separate.

```cpp
struct Person
{
    std::string name;
    int age;
    std::optional<std::string> email;
};

CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age, email)
```

**Requirements:**
- All fields must be public (use INTRUSIVE for private fields)
- All fields must be listed in the macro
- Macro must be called in the same namespace as the type

> **Common Error:** If you get a compile error like "cannot access private member",
> your struct has private fields. Either make them public or use 
> `CPP_JSON_DEFINE_TYPE_INTRUSIVE` inside the class body instead.

**Behavior:**
- Missing optional fields are set to `std::nullopt`
- Missing required fields throw `std::runtime_error`
- Extra fields in JSON are ignored

### CPP_JSON_DEFINE_TYPE_OPTIONAL

This macro makes **all fields optional** with default values. If a field is missing from JSON, 
its current value is preserved (typically the default-initialized value).

```cpp
struct Config
{
    int port = 8080;
    std::string host = "localhost";
    int timeout = 30;
};

CPP_JSON_DEFINE_TYPE_OPTIONAL(Config, port, host, timeout)
```

**Use cases:**
- Configuration files where most fields have sensible defaults
- Partial updates where you want to preserve existing values
- Backward compatibility when adding new fields

**Example:**
```cpp
Config cfg{8080, "localhost", 30};
std::string json = R"({"port": 9000})";  // Only port specified
JsonValue val = parse_json(json);
from_json(val, cfg);  // port = 9000, host = "localhost", timeout = 30
```

### CPP_JSON_DEFINE_TYPE_INTRUSIVE

This macro defines serialization functions **inside** the class definition. Use this when you 
control the type and want to keep everything together.

```cpp
struct Point
{
    double x, y;
    
    CPP_JSON_DEFINE_TYPE_INTRUSIVE(Point, x, y)
};
```

**Requirements:**
- Macro must be called inside the class body
- Macro must have access to all fields (public or friend)
- All fields must be listed in the macro

**Advantages:**
- Serialization logic stays with the type
- Can access private fields
- Better encapsulation

### Macro Limitations

**What macros cannot handle:**
- Computed fields (e.g., area = width * height)
- Fields with custom serialization logic
- Polymorphic types (inheritance)
- Types requiring validation
- Circular references

For these cases, write custom `to_json()` and `from_json()` functions:

```cpp
struct Circle
{
    double radius;
    double area() const { return 3.14159 * radius * radius; }
};

inline void to_json(JsonValue& j, const Circle& c)
{
    JsonObject obj;
    obj["radius"] = c.radius;
    obj["area"] = c.area();  // Computed field
    j = obj;
}

inline void from_json(const JsonValue& j, Circle& c)
{
    const auto& obj = std::get<JsonObject>(j);
    c.radius = from_json<double>(obj.at("radius"));
    // area is computed, not loaded
}
```

---

## Serialization: C++ to JSON

### Basic Types

JsonLite supports automatic serialization of fundamental types:

```cpp
// Integers
JsonValue j1 = to_json(42);           // int64_t
JsonValue j2 = to_json(42u);          // uint32_t -> int64_t
JsonValue j3 = to_json(42L);          // long -> int64_t
JsonValue j4 = to_json(INT64_MAX);    // Maximum integer

// Floating-point
JsonValue j5 = to_json(3.14);         // double
JsonValue j6 = to_json(3.14f);        // float -> double

// Boolean
JsonValue j7 = to_json(true);         // bool
JsonValue j8 = to_json(false);

// String
JsonValue j9 = to_json(std::string("hello"));
JsonValue j10 = to_json("world");     // const char* -> std::string

// Null
JsonValue j11 = to_json(nullptr);     // std::nullptr_t
```

**Important**: All integer types are converted to `int64_t` internally. All floating-point types 
are converted to `double`. This ensures no precision loss during round-tripping.

### Containers

JsonLite supports STL containers with automatic serialization:

```cpp
// Vectors -> JSON array
std::vector<int> vec{1, 2, 3, 4, 5};
JsonValue j1 = to_json(vec);  // [1, 2, 3, 4, 5]

// Sets -> JSON array (unordered)
std::set<std::string> set{"apple", "banana", "cherry"};
JsonValue j2 = to_json(set);  // ["apple", "banana", "cherry"]

// Maps -> JSON object
std::map<std::string, int> map{{"a", 1}, {"b", 2}};
JsonValue j3 = to_json(map);  // {"a": 1, "b": 2}

// Nested containers
std::vector<std::vector<int>> matrix{{1, 2}, {3, 4}};
JsonValue j4 = to_json(matrix);  // [[1, 2], [3, 4]]

// Optional values
std::optional<int> some = 42;
std::optional<int> none = std::nullopt;
JsonValue j5 = to_json(some);  // 42
JsonValue j6 = to_json(none);  // null
```

**Supported container types:**
- `std::vector<T>`
- `std::deque<T>`
- `std::list<T>`
- `std::set<T>`
- `std::unordered_set<T>`
- `std::map<std::string, T>`
- `std::unordered_map<std::string, T>`
- `std::optional<T>`
- `std::pair<T, U>`
- `std::tuple<Ts...>`
- C-style arrays: `T[N]`

### Structs with Macros

For structs, use one of the three macros to generate serialization code:

```cpp
struct Address
{
    std::string street;
    std::string city;
    int zip;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Address, street, city, zip)

Address addr{"123 Main St", "Springfield", 12345};
JsonValue j = to_json(addr);
// {"street": "123 Main St", "city": "Springfield", "zip": 12345}
```

### Converting to JSON String

Once you have a `JsonValue`, convert it to a string:

```cpp
JsonValue j = to_json(42);

// Compact format (default)
std::string compact = to_json_string(j);  // "42"

// Pretty format
std::string pretty = to_json_string<PrettyJsonPolicy>(j);
// 42
```

For objects:
```cpp
struct Config { int port; std::string host; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host)

Config cfg{8080, "localhost"};
JsonValue j = to_json(cfg);

// Compact: {"port":8080,"host":"localhost"}
std::string compact = to_json_string(j);

// Pretty:
// {
//   "port": 8080,
//   "host": "localhost"
// }
std::string pretty = to_json_string<PrettyJsonPolicy>(j);
```

---

## Deserialization: JSON to C++

### Two-Style API

JsonLite provides two deserialization styles:

**1. Reference-Based API** - Efficient for large objects:
```cpp
JsonObject obj;
obj["port"] = to_json(8080);

int port;
from_json(obj["port"], port);  // Modifies port
```

**2. Value-Returning API** - Convenient and enables const:
```cpp
JsonObject obj;
obj["port"] = to_json(8080);

const int port = from_json<int>(obj["port"]);  // Returns value
```

Both APIs have identical performance. The value-returning API is syntactically cleaner and allows 
using `const` variables.

### When to Use Which API

**Use value-returning API when:**
- You want `const` correctness
- Working with small types (int, bool, string)
- Chaining operations
- Code clarity matters

**Use reference-based API when:**
- You need fine-grained error control
- Working with very large objects (to avoid copies)
- Matching existing code style

### Basic Types

```cpp
JsonValue j_int = to_json(42);
JsonValue j_double = to_json(3.14);
JsonValue j_bool = to_json(true);
JsonValue j_str = to_json(std::string("hello"));

// Value-returning API
const int x = from_json<int>(j_int);
const double y = from_json<double>(j_double);
const bool flag = from_json<bool>(j_bool);
const std::string s = from_json<std::string>(j_str);

// Reference-based API
int x2;
from_json(j_int, x2);
```

### Numeric Safety

JsonLite enforces strict numeric type checking to prevent data loss:

```cpp
JsonValue j_double = to_json(3.7);

// This throws - cannot convert double to int (fractional part would be lost)
int x = from_json<int>(j_double);  // ERROR!

// This works - explicit conversion to double
double y = from_json<double>(j_double);  // OK: 3.7

// Integer overflow detection
JsonValue j_big = to_json(INT64_MAX);
int8_t small = from_json<int8_t>(j_big);  // ERROR! Overflow

// Negative to unsigned
JsonValue j_neg = to_json(-42);
unsigned int u = from_json<unsigned int>(j_neg);  // ERROR! Negative
```

**Overflow checking applies to:**
- All signed integer types: int8_t, int16_t, int32_t, int64_t, int, long, etc.
- All unsigned integer types: uint8_t, uint16_t, uint32_t, uint64_t, unsigned, etc.
- Floating-point to integer conversions (checks for fractional part)
- Negative to unsigned conversions

**No artificial margins**: JsonLite supports the full range of int64_t and double without 
reducing available values for safety margins.

### Containers

```cpp
JsonValue j_vec = to_json(std::vector<int>{1, 2, 3});
JsonValue j_set = to_json(std::set<std::string>{"a", "b"});
JsonValue j_map = to_json(std::map<std::string, int>{{"x", 1}});

// Deserialize
const auto vec = from_json<std::vector<int>>(j_vec);
const auto set = from_json<std::set<std::string>>(j_set);
const auto map = from_json<std::map<std::string, int>>(j_map);

// Optional types
JsonValue j_some = to_json(42);
JsonValue j_null = to_json(nullptr);

const auto opt1 = from_json<std::optional<int>>(j_some);  // 42
const auto opt2 = from_json<std::optional<int>>(j_null);  // nullopt
```

### Structs with Macros

```cpp
struct Person
{
    std::string name;
    int age;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)

std::string json = R"({"name": "Alice", "age": 30})";
JsonValue j = parse_json(json);

// Value-returning API
const Person p = from_json<Person>(j);

// Reference-based API
Person p2;
from_json(j, p2);
```

### Direct Key Access

For JSON objects, you can access values directly by key:

```cpp
JsonValue j = parse_json(R"({"name": "Bob", "age": 25})");
const auto& obj = std::get<JsonObject>(j);

// Type-safe extraction
const std::string name = from_json<std::string>(obj.at("name"));
const int age = from_json<int>(obj.at("age"));

// With optional
const auto email = obj.count("email") 
    ? from_json<std::string>(obj.at("email"))
    : std::string("no-email");
```

---

## Parsing and Formatting

### Parsing JSON Strings

Convert JSON strings to `JsonValue`:

```cpp
std::string json = R"({"name": "Alice", "age": 30})";

try
{
    JsonValue val = parse_json(json);
    // val now contains parsed JSON
}
catch (const std::runtime_error& e)
{
    std::cerr << "Parse error: " << e.what() << "\n";
}
```

**Error reporting includes:**
- Position in the input string
- Context around the error
- Expected vs actual token
- Field path for nested errors

Example error:
```
JSON parse error at position 15: expected ':' but got ','
    "name": "Alice", "age": 30}
                   ^
```

### Formatting Policies

JsonLite supports multiple formatting policies for JSON output.

#### StandardJsonPolicy (Default)

Produces compact JSON with minimal whitespace:

```cpp
JsonValue j = parse_json(R"({"name": "Alice", "age": 30})");
std::string output = to_json_string(j);
// {"name":"Alice","age":30}
```

**Use when:**
- Minimizing file size
- Network transmission
- Machine-to-machine communication

#### PrettyJsonPolicy

Produces human-readable JSON with indentation:

```cpp
JsonValue j = parse_json(R"({"name": "Alice", "age": 30})");
std::string output = to_json_string<PrettyJsonPolicy>(j);
// {
//   "name": "Alice",
//   "age": 30
// }
```

**Use when:**
- Configuration files edited by humans
- Debugging
- Documentation examples

#### CompatJsonPolicy

Allows non-standard JSON extensions for maximum compatibility:

```cpp
// Allows NaN and Infinity
double nan_val = std::numeric_limits<double>::quiet_NaN();
double inf_val = std::numeric_limits<double>::infinity();

JsonValue j_nan = to_json(nan_val);
JsonValue j_inf = to_json(inf_val);

// Standard policy would throw, compat policy outputs special values
std::string nan_str = to_json_string<CompatJsonPolicy>(j_nan);  // "NaN"
std::string inf_str = to_json_string<CompatJsonPolicy>(j_inf);  // "Infinity"

// Parsing also accepts these values
JsonValue parsed_nan = parse_json<CompatJsonPolicy>("NaN");
JsonValue parsed_inf = parse_json<CompatJsonPolicy>("Infinity");
```

**Use when:**
- Interfacing with systems that use NaN/Infinity
- Scientific computing with special float values
- Maximum interoperability

#### ConfigJsonPolicy

Supports JSONC (JSON with Comments) for configuration files:

```cpp
std::string jsonc = R"(
{
    // Server configuration
    "port": 8080,
    "host": "localhost",  // Default host
    
    /* Database settings
       for production */
    "database": {
        "url": "postgresql://localhost"
    }
}
)";

// Parse with comment support
JsonValue config = parse_json<ConfigJsonPolicy>(jsonc);

// Comments are stripped during parsing
std::string clean = to_json_string<PrettyJsonPolicy>(config);
// No comments in output
```

**Supported comment styles:**
- Line comments: `// comment`
- Block comments: `/* comment */`

**Use when:**
- Human-edited configuration files
- Need explanatory comments in JSON
- Working with existing JSONC files

**Important**: Comments are only supported during **parsing**. When saving with 
`ConfigJsonPolicy`, comments are not preserved.

---

## JSON Pointer (RFC 6901)

JSON Pointer is a syntax for identifying a specific value within a JSON document. JsonLite 
provides full RFC 6901 compliant implementation for navigating and extracting values from 
complex JSON structures.

### What is JSON Pointer?

A JSON Pointer is a string that identifies a specific value in a JSON document using a path-like 
syntax:

```cpp
JsonValue doc = parse_json(R"(
{
    "name": "Alice",
    "age": 30,
    "address": {
        "street": "123 Main St",
        "city": "Springfield"
    },
    "phones": ["555-1234", "555-5678"]
}
)");

// Empty string "" refers to the root document
// "/name" refers to the "name" field
// "/address/city" refers to "Springfield"
// "/phones/0" refers to "555-1234"
// "/phones/1" refers to "555-5678"
```

**Syntax rules:**
- Empty string `""` refers to the entire document
- Must start with `/` (except empty string)
- Each `/` separates path segments
- Array indices are zero-based: `/arr/0` refers to first element
- Object keys are literal strings: `/obj/key` refers to field "key"

### Basic Navigation

Use `query_json_pointer()` to navigate to a specific location:

```cpp
JsonValue doc = parse_json(R"(
{
    "database": {
        "host": "localhost",
        "port": 5432,
        "credentials": {
            "username": "admin",
            "password": "secret"
        }
    },
    "servers": ["web1", "web2", "web3"]
}
)");

// Navigate to nested values
const JsonValue& host = query_json_pointer(doc, "/database/host");
std::cout << from_json<std::string>(host) << "\n";  // "localhost"

const JsonValue& port = query_json_pointer(doc, "/database/port");
std::cout << from_json<int>(port) << "\n";  // 5432

// Navigate through nested objects
const JsonValue& username = query_json_pointer(doc, "/database/credentials/username");
std::cout << from_json<std::string>(username) << "\n";  // "admin"

// Navigate through arrays
const JsonValue& server2 = query_json_pointer(doc, "/servers/1");
std::cout << from_json<std::string>(server2) << "\n";  // "web2"

// Root document
const JsonValue& root = query_json_pointer(doc, "");
// root refers to the entire doc
```

**Error handling:**
```cpp
try
{
    const JsonValue& val = query_json_pointer(doc, "/nonexistent/path");
}
catch (const std::runtime_error& e)
{
    // Thrown if path doesn't exist
    std::cerr << "Navigation failed: " << e.what() << "\n";
}
```

### Type-Safe Queries

`query_json_as<T>()` combines navigation and type conversion in one call:

```cpp
JsonValue config = parse_json(R"(
{
    "server": {
        "port": 8080,
        "host": "localhost",
        "timeout": 30
    },
    "features": {
        "logging": true,
        "metrics": false
    }
}
)");

// Extract primitives with type safety
const int port = query_json_as<int>(config, "/server/port");
const std::string host = query_json_as<std::string>(config, "/server/host");
const bool logging = query_json_as<bool>(config, "/features/logging");

// Extract optionals - does NOT throw on missing keys!
const auto timeout = query_json_as<std::optional<int>>(config, "/server/timeout");
if (timeout)
{
    std::cout << "Timeout: " << *timeout << " seconds\n";
}

// Safe check for optional nested config
const auto metrics_port = query_json_as<std::optional<int>>(config, "/metrics/port");
// Returns std::nullopt if /metrics or /metrics/port doesn't exist - no exception!

// Extract containers
JsonValue arr_doc = parse_json(R"({"servers": ["web1", "web2", "web3"]})");
const auto servers = query_json_as<std::vector<std::string>>(arr_doc, "/servers");
for (const auto& s : servers)
{
    std::cout << "Server: " << s << "\n";
}

// Extract custom structs
struct DatabaseConfig
{
    std::string host;
    int port;
    int timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(DatabaseConfig, host, port, timeout)

JsonValue db_doc = parse_json(R"(
{
    "database": {
        "host": "db.example.com",
        "port": 5432,
        "timeout": 30
    }
}
)");

const auto db_config = query_json_as<DatabaseConfig>(db_doc, "/database");
std::cout << "DB: " << db_config.host << ":" << db_config.port << "\n";
```

**Benefits:**
- Single-line extraction with type safety
- Automatic error checking for both navigation and conversion
- Supports all types that `from_json<T>()` supports
- Clean, readable code
- **`std::optional<T>` queries never throw on missing keys** - returns `std::nullopt` instead

> **Tip for Configuration Files:** When reading optional configuration values, always use
> `query_json_as<std::optional<T>>()`. This provides safe, exception-free access to fields
> that may or may not exist, making your code robust against config file variations.

### Mutable Access

Use the non-const overload to modify values:

```cpp
JsonValue config = parse_json(R"(
{
    "server": {
        "port": 8080,
        "host": "localhost"
    }
}
)");

// Get mutable reference and modify
JsonValue& port = query_json_pointer(config, "/server/port");
port = to_json(9000);  // Change port to 9000

JsonValue& host = query_json_pointer(config, "/server/host");
host = to_json(std::string("0.0.0.0"));  // Change host

// Save modified config
save_json_to_file("config.json", config);
```

**Use cases:**
- Runtime configuration updates
- Patching specific fields
- Dynamic value modifications

### Escape Sequences

JSON Pointer requires special escaping for keys containing `~` or `/`:

- `~0` represents `~`
- `~1` represents `/`

```cpp
JsonValue doc = parse_json(R"(
{
    "file~name": "test.txt",
    "path/to/resource": "value",
    "normal_key": "normal_value",
    "~special": "tilde",
    "a~b/c": "complex"
}
)");

// Escape tilde with ~0
const JsonValue& val1 = query_json_pointer(doc, "/file~0name");
std::cout << from_json<std::string>(val1) << "\n";  // "test.txt"

// Escape slash with ~1
const JsonValue& val2 = query_json_pointer(doc, "/path~1to~1resource");
std::cout << from_json<std::string>(val2) << "\n";  // "value"

// Normal keys don't need escaping
const JsonValue& val3 = query_json_pointer(doc, "/normal_key");
std::cout << from_json<std::string>(val3) << "\n";  // "normal_value"

// Multiple escapes
const JsonValue& val4 = query_json_pointer(doc, "/a~0b~1c");
std::cout << from_json<std::string>(val4) << "\n";  // "complex"
```

**Rules:**
- `~` must be escaped as `~0`
- `/` must be escaped as `~1`
- Escape sequences are only in the JSON Pointer, not in the actual keys
- The order matters: `~0` before `~1` when both appear

### Error Handling

JSON Pointer operations can fail for several reasons:

```cpp
JsonValue doc = parse_json(R"(
{
    "array": [1, 2, 3],
    "object": {"key": "value"}
}
)");

try
{
    // Invalid pointer format (doesn't start with /)
    const JsonValue& val = query_json_pointer(doc, "invalid");
}
catch (const std::runtime_error& e)
{
    // "JSON Pointer must start with '/' or be empty"
}

try
{
    // Key not found
    const JsonValue& val = query_json_pointer(doc, "/nonexistent");
}
catch (const std::runtime_error& e)
{
    // "Key 'nonexistent' not found in JSON object"
}

try
{
    // Array index out of bounds
    const JsonValue& val = query_json_pointer(doc, "/array/10");
}
catch (const std::runtime_error& e)
{
    // "Array index 10 out of bounds (size: 3)"
}

try
{
    // Type mismatch (trying to access object as array)
    const JsonValue& val = query_json_pointer(doc, "/object/0");
}
catch (const std::runtime_error& e)
{
    // "Expected array but got object"
}

try
{
    // Type conversion error
    const int val = query_json_as<int>(doc, "/object/key");
}
catch (const std::runtime_error& e)
{
    // "Type mismatch: expected integer but got string"
}
```

**Best practices:**
- Always wrap JSON Pointer operations in try-catch blocks
- Validate pointers before use in production code
- Use query_json_as<std::optional<T>> for optional fields
- Check array bounds before accessing by index

---

## File I/O

### High-Level Convenience Functions

JsonLite provides simple functions for loading and saving parameters:

**save_params<T, Policy>(filename, object)**

```cpp
struct Config
{
    int port;
    std::string host;
    bool debug;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, debug)

Config cfg{8080, "localhost", false};

// Save with compact format (default)
save_params("config.json", cfg);

// Save with pretty format
save_params<PrettyJsonPolicy>("config.json", cfg);
```

**load_params<T, Policy>(filename)**

```cpp
Config cfg = load_params<Config>("config.json");
std::cout << "Port: " << cfg.port << "\n";
```

**Advantages:**
- Single-line save/load operations
- Automatic serialization with macros
- Error messages include filename
- Policy-based formatting control

### Backup and Safe Saving

#### save_params_with_backup()

Creates a backup before overwriting, protecting against data loss:

```cpp
struct UserPreferences
{
    std::string theme;
    int font_size;
    std::vector<std::string> recent_files;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(UserPreferences, theme, font_size, recent_files)

UserPreferences prefs{"dark", 12, {"file1.txt", "file2.txt"}};

// Creates prefs.json and prefs.json.bak (if prefs.json exists)
save_params_with_backup("prefs.json", prefs);

// Custom backup suffix
save_params_with_backup("prefs.json", prefs, ".backup");

// Timestamp-based backups
auto timestamp = std::to_string(std::time(nullptr));
save_params_with_backup("prefs.json", prefs, "." + timestamp);
```

**Behavior:**
1. If target file exists, copy it to `filename + backup_suffix`
2. Save new data to target file
3. If save fails, backup remains intact
4. If file doesn't exist, no backup is created (first save)

**Use cases:**
- Critical configuration files that shouldn't be lost
- User preferences and settings
- Game save files
- Any data that changes frequently

**Important**: This is not atomic. If the program crashes during save, the main file may be 
partially written, but the backup will be intact. For true atomic saves, use FatPJsonLite's 
`try_save_atomic()`.

### Low-Level File Operations

For more control, use the lower-level functions:

**save_json_to_file(filename, JsonValue)**

```cpp
JsonObject obj;
obj["name"] = to_json(std::string("Alice"));
obj["age"] = to_json(30);
JsonValue j = obj;

save_json_to_file("data.json", j);
```

**load_json_from_file(filename)**

```cpp
JsonValue j = load_json_from_file("data.json");
const auto& obj = std::get<JsonObject>(j);
std::string name = from_json<std::string>(obj.at("name"));
```

**With policies:**
```cpp
// Save with pretty printing
save_json_to_file<PrettyJsonPolicy>("data.json", j);

// Load with comment support
JsonValue config = load_json_from_file<ConfigJsonPolicy>("config.jsonc");
```

### Error Handling

All file operations throw `std::runtime_error` on failure:

```cpp
try
{
    Config cfg = load_params<Config>("config.json");
}
catch (const std::runtime_error& e)
{
    std::cerr << "Failed to load config: " << e.what() << "\n";
    // e.what() includes:
    // - "File error: Failed to open file 'config.json'"
    // - "JSON parse error at position 15..."
    // - "Type mismatch: expected integer..."
}
```

**Common errors:**
- File not found: "Failed to open file for reading"
- Permission denied: "Failed to open file for writing"
- Parse errors: "JSON parse error at position..."
- Type errors: "Type mismatch: expected ... but got ..."
- Missing fields: "Required field 'name' missing"

---

## Policy System

JsonLite uses compile-time policies to control JSON parsing and formatting behavior. Policies are 
template parameters that affect how JSON is generated and parsed.

### StandardJsonPolicy

The default policy for standard JSON compliance:

**Characteristics:**
- Compact output with minimal whitespace
- Strict JSON compliance (rejects NaN, Infinity)
- No comments allowed
- UTF-8 characters are escaped
- Locale-independent number formatting

```cpp
JsonValue j = parse_json(R"({"key": "value"})");
std::string output = to_json_string(j);  // Uses StandardJsonPolicy
// {"key":"value"}
```

**Use when:**
- Strict JSON compliance required
- Interfacing with other systems
- Minimizing file size

### PrettyJsonPolicy

Human-readable formatting with indentation:

**Characteristics:**
- Indented with 2 spaces per level
- Newlines after commas and braces
- Strict JSON compliance (like StandardJsonPolicy)
- No comments allowed

```cpp
JsonObject obj;
obj["name"] = to_json(std::string("Alice"));
obj["age"] = to_json(30);
obj["active"] = to_json(true);

std::string pretty = to_json_string<PrettyJsonPolicy>(JsonValue(obj));
// {
//   "name": "Alice",
//   "age": 30,
//   "active": true
// }
```

**Use when:**
- Configuration files edited by humans
- Debugging and development
- Documentation and examples

### CompatJsonPolicy

Maximum compatibility with non-standard JSON:

**Characteristics:**
- Allows `NaN`, `Infinity`, `-Infinity` as values
- Parses and generates these special values
- Useful for scientific computing
- Still rejects comments

```cpp
// Generate special values
double nan_val = std::numeric_limits<double>::quiet_NaN();
double inf_val = std::numeric_limits<double>::infinity();

JsonValue j_nan = to_json(nan_val);
JsonValue j_inf = to_json(inf_val);

std::string nan_str = to_json_string<CompatJsonPolicy>(j_nan);  // "NaN"
std::string inf_str = to_json_string<CompatJsonPolicy>(j_inf);  // "Infinity"

// Parse special values
JsonValue parsed_nan = parse_json<CompatJsonPolicy>("NaN");
double restored_nan = from_json<double>(parsed_nan);  // NaN

JsonValue parsed_inf = parse_json<CompatJsonPolicy>("Infinity");
double restored_inf = from_json<double>(parsed_inf);  // Infinity
```

**Use when:**
- Working with floating-point data that may contain NaN/Infinity
- Scientific computing and numerical analysis
- Interfacing with systems that use extended JSON

**Important**: NaN and Infinity are **not** standard JSON. Many JSON parsers will reject them.

### ConfigJsonPolicy

JSONC (JSON with Comments) support for configuration files:

**Characteristics:**
- Supports `//` line comments
- Supports `/* */` block comments
- Comments are stripped during parsing
- Output does not include comments (use PrettyJsonPolicy for output)

```cpp
std::string jsonc = R"(
{
    // Server configuration
    "port": 8080,
    "host": "localhost",  // Bind address
    
    /* Database settings
       for production environment */
    "database": {
        "url": "postgresql://localhost",
        "pool_size": 10  // Connection pool
    }
}
)";

// Parse with comment support
JsonValue config = parse_json<ConfigJsonPolicy>(jsonc);

// Extract values normally
const int port = query_json_as<int>(config, "/port");
std::cout << "Port: " << port << "\n";  // 8080

// Save without comments
save_json_to_file<PrettyJsonPolicy>("config_clean.json", config);
```

**Comment rules:**
- Line comments: `// comment` extends to end of line
- Block comments: `/* comment */` can span multiple lines
- Comments can appear anywhere whitespace is allowed
- Comments inside strings are not treated as comments
- Nested block comments are not supported

**Use when:**
- Human-edited configuration files
- Need to document JSON settings
- Working with existing JSONC files

**Limitations:**
- Comments are not preserved when loading and saving
- If you need to maintain comments, use a specialized JSONC library

### Custom Policies

You can create custom policies by defining a struct with the required static functions:

```cpp
struct MyCustomPolicy
{
    // Control compact vs pretty printing
    static constexpr bool is_compact() { return false; }
    static constexpr int indent_spaces() { return 4; }
    
    // Control special value handling
    static constexpr bool allow_nan_inf() { return false; }
    
    // Control comment handling
    static constexpr bool allow_comments() { return false; }
    
    // Control UTF-8 handling
    static constexpr bool escape_unicode() { return true; }
};

// Use custom policy
JsonValue j = to_json(config);
std::string output = to_json_string<MyCustomPolicy>(j);
```

**Policy interface:**
```cpp
struct Policy
{
    // Pretty printing (false = compact, true = formatted)
    static constexpr bool is_compact();
    static constexpr int indent_spaces();  // Only used if !is_compact()
    
    // Special value support
    static constexpr bool allow_nan_inf();  // NaN, Infinity
    
    // Comment support (parsing only)
    static constexpr bool allow_comments();  // JSONC comments
    
    // UTF-8 handling
    static constexpr bool escape_unicode();  // Escape non-ASCII
};
```

---

## Error Handling

### Exception Types

JsonLite throws `std::runtime_error` for all error conditions:

```cpp
try
{
    JsonValue j = parse_json(invalid_json);
}
catch (const std::runtime_error& e)
{
    std::cerr << "Error: " << e.what() << "\n";
}
```

### Example Error Messages

**Parse errors:**
```
JSON parse error at position 15: expected ':' but got ','
Context: "name": "Alice", "age"
                        ^
```

**Type errors:**
```
Type mismatch: expected integer but got string at field 'port'
```

**Range errors:**
```
Numeric cast overflow: value 999999 exceeds maximum for int8_t (127)
```

**Missing field errors:**
```
Required field 'name' missing in JSON object
```

**File errors:**
```
File error: Failed to open file 'config.json' for reading
```

**JSON Pointer errors:**
```
JSON Pointer error: Key 'database' not found in JSON object at pointer '/database/port'
```

### Catching Errors

**Basic parse_json error handling:**
```cpp
std::string user_input = get_user_json();  // Could be malformed

try
{
    JsonValue data = parse_json(user_input);
    // Process valid JSON...
    auto name = from_json<std::string>(data.at("name"));
}
catch (const std::runtime_error& e)
{
    // Catches both parse errors and type conversion errors
    std::cerr << "JSON error: " << e.what() << "\n";
    // e.what() includes position info for parse errors, e.g.:
    // "JSON parse error at line 3, column 15: Expected ':' after object key"
}
```

**Specific error handling:**
```cpp
try
{
    Config cfg = load_params<Config>("config.json");
}
catch (const std::runtime_error& e)
{
    std::string msg = e.what();
    
    if (msg.find("File error") != std::string::npos)
    {
        std::cerr << "Config file not found, using defaults\n";
        return Config{};  // Return default config
    }
    else if (msg.find("JSON parse error") != std::string::npos)
    {
        std::cerr << "Config file corrupted: " << msg << "\n";
        return Config{};
    }
    else
    {
        std::cerr << "Unexpected error: " << msg << "\n";
        throw;  // Re-throw if unknown
    }
}
```

**With validation:**
```cpp
Config load_validated_config()
{
    Config cfg = load_params<Config>("config.json");
    
    if (cfg.port < 1024 || cfg.port > 65535)
    {
        throw std::runtime_error("Port must be 1024-65535");
    }
    
    if (cfg.host.empty())
    {
        throw std::runtime_error("Host cannot be empty");
    }
    
    return cfg;
}
```

---

## Supported Types Reference

### Fundamental Types

| C++ Type | JSON Type | Notes |
|----------|-----------|-------|
| `bool` | boolean | `true` or `false` |
| `int8_t`, `int16_t`, `int32_t` | integer | Stored as `int64_t` |
| `int64_t`, `int`, `long` | integer | Native representation |
| `uint8_t`, `uint16_t`, `uint32_t` | integer | Converted to `int64_t` |
| `uint64_t`, `unsigned long long` | integer/number | Values <= INT64_MAX stored as `int64_t`; larger values become `double` |
| `float` | number | Converted to `double` |
| `double` | number | Native representation |
| `std::string` | string | UTF-8 encoded |
| `const char*` | string | Converted to `std::string` |
| `std::string_view` | string | Converted to `std::string` |
| `std::nullptr_t` | null | JSON null |

> **Warning:** Values of `uint64_t` greater than `INT64_MAX` (9,223,372,036,854,775,807)
> are stored as `double`, which may lose precision for very large integers. If you need
> exact representation of such values, consider storing them as strings.

**Important notes:**
- All integer types are stored as `int64_t` internally
- All floating-point types are stored as `double` internally
- Overflow detection on conversion back to narrower types
- Full int64_t and double range supported (no artificial margins)

### Container Types

| C++ Type | JSON Type | Notes |
|----------|-----------|-------|
| `std::vector<T>` | array | Most common, recommended |
| `std::deque<T>` | array | Random access |
| `std::list<T>` | array | Linked list |
| `std::array<T, N>` | array | Fixed size |
| `T[N]` | array | C-style arrays |
| `std::set<T>` | array | Ordered, no duplicates |
| `std::unordered_set<T>` | array | Unordered |
| `std::map<std::string, T>` | object | Ordered by key |
| `std::unordered_map<std::string, T>` | object | Hash-based |
| `std::optional<T>` | T or null | `nullopt`  `null` |
| `std::pair<T, U>` | array | 2-element array |
| `std::tuple<Ts...>` | array | N-element array |

### Notes on Container Support

**Maps must have string keys:**
```cpp
std::map<std::string, int> ok;  //  Works
std::map<int, std::string> no;  //  Won't compile
```

**Nested containers work:**
```cpp
std::vector<std::vector<int>> matrix;  //  OK
std::map<std::string, std::vector<int>> data;  //  OK
std::vector<std::map<std::string, double>> records;  //  OK
```

**Optional fields:**
```cpp
struct Config
{
    int port;
    std::optional<std::string> cert_path;  // May be null
};
```

---

## Advanced Patterns

### Nested Structures

Complex nested types work automatically with macros:

```cpp
struct Address
{
    std::string street;
    std::string city;
    int zip;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Address, street, city, zip)

struct Person
{
    std::string name;
    int age;
    Address address;  // Nested struct
    std::vector<std::string> hobbies;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age, address, hobbies)

Person p{"Alice", 30, {"123 Main St", "Springfield", 12345}, {"reading", "cycling"}};
std::string json = to_json_string<PrettyJsonPolicy>(to_json(p));
// {
//   "name": "Alice",
//   "age": 30,
//   "address": {
//     "street": "123 Main St",
//     "city": "Springfield",
//     "zip": 12345
//   },
//   "hobbies": ["reading", "cycling"]
// }
```

### Heterogeneous Arrays

JSON arrays can contain mixed types using `JsonValue`:

```cpp
JsonArray mixed;
mixed.push_back(to_json(42));
mixed.push_back(to_json(std::string("hello")));
mixed.push_back(to_json(true));
mixed.push_back(to_json(nullptr));

JsonValue j = mixed;
std::string json = to_json_string(j);
// [42,"hello",true,null]
```

Access mixed arrays using `std::visit` or `std::get_if`:

```cpp
for (const auto& elem : mixed)
{
    std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>)
        {
            std::cout << "Integer: " << arg << "\n";
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            std::cout << "String: " << arg << "\n";
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            std::cout << "Bool: " << arg << "\n";
        }
    }, elem);
}
```

### Custom Serialization

For types that need custom logic, write your own `to_json` and `from_json`:

```cpp
#include <charconv>  // C++17

struct Date
{
    int year, month, day;
    
    std::string to_string() const
    {
        std::ostringstream oss;
        oss << year << "-" << std::setw(2) << std::setfill('0') << month
            << "-" << std::setw(2) << std::setfill('0') << day;
        return oss.str();
    }
    
    static Date from_string(const std::string& s)
    {
        // Parse "YYYY-MM-DD" using C++17 std::from_chars
        Date d{};
        const char* ptr = s.data();
        const char* end = s.data() + s.size();
        
        auto [p1, ec1] = std::from_chars(ptr, end, d.year);
        if (ec1 != std::errc{} || p1 >= end || *p1 != '-')
        {
            throw std::runtime_error("Invalid date format: " + s);
        }
        
        auto [p2, ec2] = std::from_chars(p1 + 1, end, d.month);
        if (ec2 != std::errc{} || p2 >= end || *p2 != '-')
        {
            throw std::runtime_error("Invalid date format: " + s);
        }
        
        auto [p3, ec3] = std::from_chars(p2 + 1, end, d.day);
        if (ec3 != std::errc{})
        {
            throw std::runtime_error("Invalid date format: " + s);
        }
        
        return d;
    }
};

// Custom serialization
inline void to_json(JsonValue& j, const Date& d)
{
    j = to_json(d.to_string());  // Serialize as string
}

inline void from_json(const JsonValue& j, Date& d)
{
    std::string s = from_json<std::string>(j);
    d = Date::from_string(s);
}

// Usage
Date d{2024, 1, 15};
JsonValue j = to_json(d);
std::string json = to_json_string(j);  // "2024-01-15"

JsonValue parsed = parse_json(R"("2024-01-15")");
Date restored = from_json<Date>(parsed);
```

### Polymorphism

JsonLite doesn't support automatic polymorphic serialization, but you can implement it manually:

```cpp
struct Shape
{
    virtual ~Shape() = default;
    virtual std::string type() const = 0;
    virtual void to_json_impl(JsonObject& obj) const = 0;
};

struct Circle : Shape
{
    double radius;
    
    std::string type() const override { return "circle"; }
    
    void to_json_impl(JsonObject& obj) const override
    {
        obj["radius"] = to_json(radius);
    }
};

struct Rectangle : Shape
{
    double width, height;
    
    std::string type() const override { return "rectangle"; }
    
    void to_json_impl(JsonObject& obj) const override
    {
        obj["width"] = to_json(width);
        obj["height"] = to_json(height);
    }
};

// Manual serialization with type tag
inline void to_json(JsonValue& j, const Shape& s)
{
    JsonObject obj;
    obj["type"] = to_json(s.type());
    s.to_json_impl(obj);
    j = obj;
}

// Usage
Circle c{5.0};
JsonValue j = to_json(c);
// {"type": "circle", "radius": 5.0}
```

For deserialization, use a factory pattern:

```cpp
std::unique_ptr<Shape> shape_from_json(const JsonValue& j)
{
    const auto& obj = std::get<JsonObject>(j);
    std::string type = from_json<std::string>(obj.at("type"));
    
    if (type == "circle")
    {
        auto c = std::make_unique<Circle>();
        c->radius = from_json<double>(obj.at("radius"));
        return c;
    }
    else if (type == "rectangle")
    {
        auto r = std::make_unique<Rectangle>();
        r->width = from_json<double>(obj.at("width"));
        r->height = from_json<double>(obj.at("height"));
        return r;
    }
    
    throw std::runtime_error("Unknown shape type: " + type);
}
```

---

## Comparison with Other Libraries

### JsonLite vs nlohmann/json

| Feature | JsonLite | nlohmann/json |
|---------|----------|---------------|
| **C++ Version** | C++17 | C++11 |
| **Dependencies** | None | None |
| **Type Safety** | Explicit | Implicit conversions |
| **Numeric Checking** | Overflow detection | No checking |
| **Parsing Speed** | Moderate | Moderate |
| **Memory Usage** | std containers | std containers |
| **JSON Pointer** |  RFC 6901 |  RFC 6901 + JSON Patch |
| **Struct Macros** |  Non-intrusive |  Intrusive/non-intrusive |
| **Error Messages** | Detailed with position | Good |
| **Special Values** | Policy-based (NaN/Inf) | Configurable |
| **Comments** |  Via ConfigJsonPolicy |  |

**Choose nlohmann/json if:**
- Need C++11 compatibility
- Want implicit conversions
- Need JSON Patch or JSON Schema
- Prefer more "magic" over explicit

**Choose JsonLite if:**
- Want explicit type safety
- Need overflow checking
- Using C++17+
- Prefer clarity over convenience

### JsonLite vs RapidJSON

| Feature | JsonLite | RapidJSON |
|---------|----------|-----------|
| **C++ Version** | C++17 | C++03/11 |
| **Parsing Speed** | ~150 MB/s | ~400-1000 MB/s |
| **API Style** | Modern (variant) | Classic (pointers) |
| **Memory** | std allocator | Custom allocators |
| **SAX Parser** |  |  |
| **JSON Pointer** |  |  |
| **Safety** | High (overflow checks) | Lower (manual) |
| **Ease of Use** | High | Moderate |

**Choose RapidJSON if:**
- Speed is critical (>100MB files)
- Need custom allocators
- Need SAX parsing
- Performance > safety

**Choose JsonLite if:**
- Safety is critical
- Want modern C++ API
- Configuration files (<10MB)
- Simplicity matters

### JsonLite vs simdjson

| Feature | JsonLite | simdjson |
|---------|----------|----------|
| **Parsing Speed** | ~150 MB/s | ~2-4 GB/s |
| **API** | Read/Write | Read-only |
| **SIMD** | No | Yes (AVX2, SSE4) |
| **Modification** |  Can modify |  Read-only |
| **Use Case** | General purpose | High-throughput parsing |

**Choose simdjson if:**
- Parsing multi-GB files
- Read-only workload
- Maximum speed critical

**Choose JsonLite if:**
- Need to modify JSON
- Need to generate JSON
- Configuration management

### JsonLite vs Boost.PropertyTree

| Feature | JsonLite | Boost.PropertyTree |
|---------|----------|-------------------|
| **Dependencies** | None | Boost |
| **JSON Semantics** | True JSON | Approximate |
| **Type Safety** | Strong | Weak |
| **Formats** | JSON only | JSON/XML/INI |
| **Array Support** | Native | Limited |

**Choose Boost.PropertyTree if:**
- Need XML or INI support
- Already using Boost
- Simple key-value configs

**Choose JsonLite if:**
- True JSON semantics needed
- Want zero dependencies
- Need proper array support

### JsonLite vs FatPJsonLite

| Feature | JsonLite | FatPJsonLite |
|---------|----------|--------------|
| **Error Handling** | Exceptions | Expected<T, E> |
| **Data Structures** | std::vector/map | SmallVector/FlatMap |
| **Dependencies** | Zero | fat_p components |
| **Performance** | Moderate | 2-5x faster |
| **Memory** | Standard | Optimized (30-50% savings) |
| **Large Files** | Standard I/O | Memory-mapped I/O |
| **Binary Size** | Smaller | Larger (+20-30%) |
| **JSON Pointer** |  Exception-based |  Expected-based |

**Choose FatPJsonLite if:**
- Need exception-free code
- Performance is critical
- Large files (>10MB)
- Memory efficiency matters

**Choose JsonLite if:**
- Simplicity is paramount
- Exceptions are acceptable
- Minimal dependencies desired
- Binary size matters

---

## Use Case Guide

### Configuration Files

** Ideal for JsonLite**

Configuration files are JsonLite's primary use case:

```cpp
struct ServerConfig
{
    int port = 8080;
    std::string host = "localhost";
    int max_connections = 100;
    std::optional<std::string> cert_path;
    bool enable_logging = true;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(ServerConfig, port, host, max_connections, 
                               cert_path, enable_logging)

// Load config with fallback to defaults
ServerConfig load_config()
{
    try
    {
        return load_params<ServerConfig>("server.json");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Using default config: " << e.what() << "\n";
        return ServerConfig{};
    }
}

// Save with backup
void update_config(const ServerConfig& cfg)
{
    save_params_with_backup<PrettyJsonPolicy>("server.json", cfg);
}
```

**Benefits:**
- Automatic struct serialization with macros
- Optional fields with `std::optional`
- Backup on save prevents data loss
- Pretty printing for human editing
- Detailed error messages aid debugging

### REST API Responses

** Possible but not optimal**

JsonLite works for REST APIs but isn't optimized for it:

```cpp
struct UserResponse
{
    int id;
    std::string username;
    std::string email;
    std::vector<std::string> roles;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(UserResponse, id, username, email, roles)

std::string handle_get_user(int user_id)
{
    UserResponse user = get_user_from_db(user_id);
    return to_json_string(to_json(user));
}
```

**Limitations:**
- Moderate parsing speed (~150 MB/s)
- Exception-based error handling
- Not ideal for high-frequency APIs

**Better alternatives:**
- FatPJsonLite for exception-free code
- RapidJSON for maximum speed
- simdjson for read-heavy workloads

### Game Save Files

** Good fit for JsonLite**

Game save data benefits from JsonLite's safety and ease of use:

```cpp
struct PlayerData
{
    std::string name;
    int level;
    int health;
    int gold;
    std::vector<std::string> inventory;
    std::map<std::string, int> quest_progress;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(PlayerData, name, level, health, 
                                   gold, inventory, quest_progress)

void save_game(const PlayerData& player)
{
    save_params_with_backup<PrettyJsonPolicy>("save.json", player);
}

PlayerData load_game()
{
    try
    {
        return load_params<PlayerData>("save.json");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Failed to load save: " << e.what() << "\n";
        return PlayerData{};  // New game
    }
}
```

**Benefits:**
- Backup prevents losing player progress
- Human-readable for debugging
- Optional fields for backward compatibility
- Type safety prevents save corruption

### High-Frequency Trading / Real-Time

** Not suitable**

JsonLite is **not** designed for high-frequency or real-time systems:

**Issues:**
- Exception-based error handling (unpredictable timing)
- Moderate parsing speed
- Allocation patterns not optimized for latency
- No SIMD acceleration

**Better alternatives:**
- FatPJsonLite (exception-free, 2-5x faster)
- RapidJSON (custom allocators, SAX parsing)
- FlatBuffers or Protocol Buffers (binary formats)

### Large Data Processing

** Depends on file size**

- **<10 MB files**: JsonLite is fine
- **10-100 MB files**: Consider FatPJsonLite with memory-mapped I/O
- **>100 MB files**: Use simdjson or streaming parsers

### Embedded Systems

** Possible with caveats**

JsonLite can work on embedded systems:

**Requirements:**
- C++17 compiler support
- std::variant, std::optional available
- Sufficient memory for parsed JSON tree

**Concerns:**
- Exceptions require stack unwinding support
- No control over allocations
- Binary size ~30-50KB

**Better alternatives:**
- FatPJsonLite (exception-free)
- ArduinoJson (designed for embedded)
- Custom lightweight parsers

### Cross-Platform Tools

** Excellent fit**

JsonLite works across all major platforms:

- **Windows**: MSVC 2017+, MinGW
- **Linux**: GCC 7.3+, Clang 5.0+
- **macOS**: Xcode 10+
- **BSD**: Clang 5.0+

**Benefits:**
- Header-only (no build system needed)
- Zero external dependencies
- Standard C++17 only
- Same code on all platforms

---

## Migration Guide

### From nlohmann/json to JsonLite

**1. Change includes:**
```cpp
// Before
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// After
#include "JsonLite.h"
USING_JSON_LITE()
```

**2. Update struct macros:**
```cpp
// Before
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)

// After
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Person, name, age)
```

**3. Change implicit conversions:**
```cpp
// Before
json j;
j["port"] = 8080;
int port = j["port"];  // Implicit conversion

// After
JsonObject obj;
obj["port"] = to_json(8080);
int port = from_json<int>(obj.at("port"));  // Explicit conversion
```

**4. Update file I/O:**
```cpp
// Before
std::ifstream f("config.json");
json j = json::parse(f);

// After
JsonValue j = load_json_from_file("config.json");
```

**5. Handle JSON Pointer:**
```cpp
// Before
int port = j["/database/port"_json_pointer];

// After
int port = query_json_as<int>(j, "/database/port");
```

### From RapidJSON to JsonLite

**1. Replace document/value:**
```cpp
// Before
rapidjson::Document doc;
doc.Parse(json_str.c_str());

// After
JsonValue doc = parse_json(json_str);
```

**2. Simplify value access:**
```cpp
// Before
const rapidjson::Value& port = doc["port"];
int port_val = port.GetInt();

// After
const auto& obj = std::get<JsonObject>(doc);
int port_val = from_json<int>(obj.at("port"));
```

**3. Update array iteration:**
```cpp
// Before
const rapidjson::Value& arr = doc["items"];
for (auto& v : arr.GetArray()) {
    int val = v.GetInt();
}

// After
const auto& obj = std::get<JsonObject>(doc);
const auto items = from_json<std::vector<int>>(obj.at("items"));
for (int val : items) {
    // use val
}
```

### From Boost.PropertyTree to JsonLite

**1. Replace ptree:**
```cpp
// Before
boost::property_tree::ptree pt;
boost::property_tree::read_json("config.json", pt);
int port = pt.get<int>("port");

// After
JsonValue j = load_json_from_file("config.json");
int port = query_json_as<int>(j, "/port");
```

**2. Handle arrays properly:**
```cpp
// Before (arrays are awkward in ptree)
for (auto& item : pt.get_child("items")) {
    int val = item.second.get_value<int>();
}

// After (arrays are first-class)
const auto items = query_json_as<std::vector<int>>(j, "/items");
for (int val : items) {
    // use val
}
```

---

## Compiler Requirements

### Minimum Version

JsonLite requires C++17 or later.

### Tested Compilers

| Compiler | Minimum Version | Notes |
|----------|----------------|-------|
| **GCC** | 7.3 | Full support |
| **Clang** | 5.0 | Full support |
| **MSVC** | 2017 (15.7) | Full support |
| **Apple Clang** | 10.0 | Full support |

### Compilation Flags

**Required:**
```bash
-std=c++17
```

**Recommended:**
```bash
-std=c++17 -O2 -Wall -Wextra
```

**For maximum performance:**
```bash
-std=c++17 -O3 -DNDEBUG -march=native
```

**Debug with sanitizers:**
```bash
-std=c++17 -g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer
```

### Dependencies

JsonLite has **zero external dependencies**.

Required standard library headers:
- `<algorithm>`
- `<array>`
- `<charconv>`
- `<cmath>`
- `<cstddef>`
- `<cstdint>`
- `<deque>`
- `<fstream>`
- `<iomanip>`
- `<iterator>`
- `<limits>`
- `<list>`
- `<map>`
- `<optional>`
- `<set>`
- `<sstream>`
- `<stdexcept>`
- `<string>`
- `<string_view>`
- `<tuple>`
- `<type_traits>`
- `<unordered_map>`
- `<unordered_set>`
- `<utility>`
- `<variant>`
- `<vector>`

All headers are part of the C++17 standard library.

---

## Performance Characteristics

### Parsing Throughput

JsonLite achieves **100-200 MB/s** parsing throughput on typical hardware:

**Test system:**
- CPU: Intel Core i7-8850H @ 2.60GHz
- RAM: 32 GB
- OS: Windows 10

**Typical performance:**
- Small objects (<1KB): ~650 ns per parse
- Medium objects (1-10KB): ~5-50 us per parse
- Large objects (>100KB): ~500 us - 5 ms per parse

This is sufficient for:
- Configuration files at application startup
- Occasional parameter updates
- Game save files
- Log file processing (<100MB)

This is insufficient for:
- High-frequency API responses (>1000 req/s)
- Real-time systems with tight latency requirements
- Large data processing (>100MB files)
- Streaming data ingestion

### Memory Usage

JsonLite uses standard containers:
- **Arrays**: std::vector with heap allocation
- **Objects**: std::map (red-black tree)
- **Values**: std::variant (inline for small types)

Memory overhead per JsonValue: ~32-64 bytes depending on type and platform.

For memory-critical applications, consider FatPJsonLite with SmallVector (inline storage for 8 
elements) and FlatMap (contiguous storage, better cache locality).

### Optimization Tips

1. **Use value-returning API**: No performance difference, but cleaner code
2. **Reserve vector capacity**: If you know array size, call `reserve()`
3. **Reuse JsonValue objects**: Avoids allocation churn for repeated operations
4. **Use compact policy**: Default StandardJsonPolicy produces minimal JSON
5. **Profile first**: Don't optimize without measurements

**Example - reserve capacity:**
```cpp
JsonArray arr;
arr.reserve(1000);  // Pre-allocate space
for (int i = 0; i < 1000; ++i)
{
    arr.push_back(to_json(i));
}
```

**Example - reuse objects:**
```cpp
JsonObject config;
// Reuse config object for multiple operations
config["port"] = to_json(8080);
save_json_to_file("config.json", config);

config["port"] = to_json(9000);  // Modify and save again
save_json_to_file("config.json", config);
```

---

## Best Practices

### Design Patterns

**1.  Use std::optional for optional fields:**

```cpp
struct Config
{
    int port;
    std::string host;
    std::optional<int> timeout;        // May be missing
    std::optional<std::string> cert;   // May be missing
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, timeout, cert)
```

**2.  Provide defaults with OPTIONAL macro:**

```cpp
struct Config
{
    int port = 8080;           // Default if missing
    std::string host = "localhost";
    int timeout = 30;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(Config, port, host, timeout)
```

**3.  Validate after loading:**

```cpp
Config cfg = load_params<Config>("config.json");

if (cfg.port < 1024 || cfg.port > 65535)
{
    throw std::runtime_error("Port must be 1024-65535");
}
if (cfg.timeout <= 0)
{
    throw std::runtime_error("Timeout must be positive");
}
```

**4.  Use backup for critical files:**

```cpp
// Always save with backup for config files
save_params_with_backup<PrettyJsonPolicy>("config.json", cfg);
```

**5.  Use pretty printing for human-edited files:**

```cpp
// Make config readable for manual editing
save_params<PrettyJsonPolicy>("config.json", cfg);
```

**6.  Use JSON Pointer for deep access:**

```cpp
// Instead of nested get() calls
const int port = query_json_as<int>(config, "/database/server/port");

// Rather than
const auto& obj = std::get<JsonObject>(config);
const auto& db = std::get<JsonObject>(obj.at("database"));
const auto& srv = std::get<JsonObject>(db.at("server"));
int port = from_json<int>(srv.at("port"));
```

### Error Handling

**At application startup:**

```cpp
int main()
{
    try
    {
        Config cfg = load_params<Config>("config.json");
        // Continue with cfg...
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Fatal: Failed to load config: " << e.what() << "\n";
        return 1;
    }
}
```

**With fallback to defaults:**

```cpp
Config load_config_or_default()
{
    try
    {
        return load_params<Config>("config.json");
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Warning: Using default config: " << e.what() << "\n";
        return Config{};  // Return default-constructed config
    }
}
```

**With validation:**

```cpp
Config load_and_validate()
{
    Config cfg = load_params<Config>("config.json");
    
    if (cfg.port < 1024)
    {
        throw std::runtime_error("Port must be >= 1024");
    }
    
    if (cfg.host.empty())
    {
        throw std::runtime_error("Host cannot be empty");
    }
    
    return cfg;
}
```

### Testing

**Round-trip testing:**

```cpp
void test_round_trip()
{
    Config original{8080, "localhost", 30};
    
    std::string json = to_json_string(to_json(original));
    JsonValue parsed = parse_json(json);
    Config restored = from_json<Config>(parsed);
    
    assert(original.port == restored.port);
    assert(original.host == restored.host);
    assert(original.timeout == restored.timeout);
}
```

**Schema validation:**

```cpp
void validate_person(const Person& p)
{
    if (p.name.empty())
    {
        throw std::runtime_error("Name cannot be empty");
    }
    if (p.age < 0 || p.age > 150)
    {
        throw std::runtime_error("Age must be 0-150");
    }
}

Person load_person()
{
    Person p = load_params<Person>("person.json");
    validate_person(p);
    return p;
}
```

---

## Summary

JsonLite is a **safety-first, modern C++17 JSON library** designed for applications that 
prioritize correctness and simplicity over maximum performance.

**Key Characteristics:**
-  Zero external dependencies (standard library only)
-  Explicit type safety (no silent conversions)
-  Checked arithmetic (no overflow)
-  Detailed error messages with source positions
-  Macro-based struct serialization
-  Both reference-based and value-returning APIs
-  JSON Pointer (RFC 6901) support
-  JSONC comment support via policies
-  Backup on save for data protection

**Best For:**
-  Configuration file management
-  Parameter persistence
-  Game save files
-  Small to medium JSON (<10MB)
-  Applications where safety matters more than speed
-  Cross-platform tools and utilities

**Not Ideal For:**
-  High-frequency API responses (>1000 req/s)
-  Real-time systems with tight latency requirements
-  Very large JSON files (>100MB)
-  Need for JSON Patch or JSON Schema
-  C++11 compatibility required

**Choose JsonLite when:**
- You want explicit type safety
- You need zero external dependencies
- Configuration correctness is critical
- You're building on C++17 or later
- Simplicity and clarity matter
- Human-readable config files are important

**Choose alternatives when:**
- Maximum parsing speed required  RapidJSON or simdjson
- More features needed  nlohmann/json
- Exception-free code required  FatPJsonLite
- C++11 compatibility required  nlohmann/json or RapidJSON

JsonLite fills a specific niche in the C++ JSON ecosystem. It's not trying to be the fastest or 
most feature-complete library. It's designed to make configuration file handling safe, simple, 
and dependency-free for modern C++ applications, with the added power of JSON Pointer for 
navigating complex structures and backup saves for data protection.
