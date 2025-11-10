# JsonLite v0.1.0 User Manual

## Table of Contents

1. [Integration](#integration)
2. [Core Concepts](#core-concepts)
3. [Type System](#type-system)
4. [Serialization API](#serialization-api)
5. [Parsing API](#parsing-api)
6. [Macro System](#macro-system)
7. [Policy System](#policy-system)
8. [Type Conversion](#type-conversion)
9. [File I/O](#file-io)
10. [Error Handling](#error-handling)
11. [Performance](#performance)
12. [Memory Model](#memory-model)
13. [Thread Safety](#thread-safety)
14. [Platform Compatibility](#platform-compatibility)
15. [Common Patterns](#common-patterns)
16. [Anti-Patterns](#anti-patterns)
17. [Troubleshooting](#troubleshooting)
18. [Compiler Requirements](#compiler-requirements)

---

## Integration

### Single-File Header

```cpp
#include "JsonLite.h"
using namespace cpp_utilities;
```

That's it. No linking, no build configuration, no dependencies.

### Compiler Flags

**Minimum:**
```bash
g++ -std=c++17 -I/path/to/jsonlite main.cpp
```

**Recommended:**
```bash
g++ -std=c++17 -O2 -Wall -Wextra -I/path/to/jsonlite main.cpp
```

**What you get:**
- Zero external dependencies
- Single translation unit
- Header-only (no .cpp file)
- Compiles in ~1-2 seconds on modern hardware

---

## Core Concepts

### Design Philosophy

JsonLite makes specific trade-offs:

1. **Correctness > Speed**: Range checks on every conversion
2. **Simplicity > Features**: No JSON Pointer, no streaming, no binary formats
3. **Zero Dependencies > Convenience**: Uses std::map (O(log n)) instead of requiring a hash library
4. **Compile-Time > Runtime**: Policy-based design, templates everywhere
5. **Explicit > Implicit**: 3.7 → int throws error (no silent truncation)

### What It's Good At

- Configuration files (1KB - 10MB)
- Game save files
- Parameter persistence
- Structured logging output
- Settings management
- Data interchange with controlled sources

### What It's Bad At

- High-frequency parsing (millions of ops/sec)
- Untrusted network input (no size limits)
- Streaming gigabyte files
- Scientific computing with extreme exponents (±1e100)
- Real-time systems with bounded latency requirements

---

## Type System

### JsonValue

`JsonValue` is a `std::variant` holding 7 types:

```cpp
std::variant<
    std::nullptr_t,  // null
    bool,            // true/false
    int64_t,         // integers
    double,          // floating-point
    std::string,     // strings
    JsonArray,       // arrays
    JsonObject       // objects
>
```

**Memory Layout:**
- Size: 40-48 bytes (platform-dependent)
- Discriminator: 4-8 bytes for variant tag
- Data: 32-40 bytes for largest alternative

**Type Checking:**

```cpp
JsonValue j = 42;

j.is_null();    // false
j.is_bool();    // false
j.is_int();     // true
j.is_number();  // true (int64_t OR double)
j.is_string();  // false
j.is_array();   // false
j.is_object();  // false
```

**Accessing Values:**

```cpp
// Option 1: std::get<T> (throws std::bad_variant_access if wrong type)
int64_t i = std::get<int64_t>(j);

// Option 2: std::get_if<T> (returns nullptr if wrong type)
if (auto* ptr = std::get_if<int64_t>(&j)) {
    // use *ptr
}

// Option 3: std::visit
std::visit([](auto&& val) {
    using T = std::decay_t<decltype(val)>;
    if constexpr (std::is_same_v<T, int64_t>) {
        // handle integer
    }
}, j);
```

### JsonObject

```cpp
using JsonObject = std::map<std::string, JsonValue>;
```

**Why std::map not std::unordered_map:**
- Deterministic iteration order
- No hash function required
- Consistent diff output (git, etc.)
- For 10-100 fields, O(log n) vs O(1) doesn't matter

**Access:**

```cpp
JsonObject obj;
obj["key"] = 42;  // Insert or assign

// Check existence
if (auto it = obj.find("key"); it != obj.end()) {
    JsonValue& val = it->second;
}

// Range-based for
for (const auto& [key, value] : obj) {
    // keys are sorted alphabetically
}
```

### JsonArray

```cpp
using JsonArray = std::vector<JsonValue>;
```

**Operations:**

```cpp
JsonArray arr;
arr.push_back(42);
arr.push_back("hello");
arr.push_back(nullptr);

arr[0];        // Access by index
arr.size();    // Element count
arr.empty();   // Check if empty
```

---

## Serialization API

### Basic Types

```cpp
// Integers
JsonValue j1 = 42;                    // int → int64_t
JsonValue j2 = 42U;                   // unsigned int → int64_t
JsonValue j3 = 42L;                   // long → int64_t
JsonValue j4 = 42UL;                  // unsigned long → int64_t or double
JsonValue j5 = 42LL;                  // long long → int64_t
JsonValue j6 = 42ULL;                 // unsigned long long → int64_t or double

// Floating-point
JsonValue j7 = 3.14f;                 // float → double
JsonValue j8 = 3.14;                  // double

// Strings
JsonValue j9 = std::string("hello");  // std::string
JsonValue j10 = "hello";              // const char*

// Boolean
JsonValue j11 = true;                 // bool
JsonValue j12 = false;

// Null
JsonValue j13 = nullptr;              // nullptr_t
```

### Unsigned Long/Long Long Edge Case

**Critical behavior:**

```cpp
// On 64-bit systems where unsigned long is 64-bit:
unsigned long huge = 0xFFFFFFFFFFFFFFFFUL;  // 18,446,744,073,709,551,615

JsonValue j = huge;
// If huge > INT64_MAX (9,223,372,036,854,775,807):
//   → Stored as DOUBLE (precision loss possible)
// Else:
//   → Stored as int64_t
```

**Why:** int64_t max is 2^63-1. Unsigned long can be 2^64-1.

**Implication:** Values > INT64_MAX lose precision:

```cpp
unsigned long val = 9223372036854775808UL;  // INT64_MAX + 1
JsonValue j = val;
assert(j.is_number());  // true (could be int OR double)
// j is stored as double
// Precision: ~15-17 significant decimal digits
```

**Solution if you need exact unsigned 64-bit:**
Store as string and parse manually.

### Containers

```cpp
// Vector
std::vector<int> vec = {1, 2, 3};
JsonValue j = to_json(vec);
// Result: [1,2,3]

// Map
std::map<std::string, int> map = {{"a", 1}, {"b", 2}};
JsonValue j = to_json(map);
// Result: {"a":1,"b":2}

// Set
std::set<int> set = {3, 1, 2};
JsonValue j = to_json(set);
// Result: [1,2,3]  (sorted)

// Nested
std::vector<std::vector<int>> nested = {{1,2}, {3,4}};
JsonValue j = to_json(nested);
// Result: [[1,2],[3,4]]
```

### Optional

```cpp
std::optional<int> opt1 = 42;
JsonValue j1 = to_json(opt1);  // 42

std::optional<int> opt2;
JsonValue j2 = to_json(opt2);  // null
```

### Tuples and Pairs

```cpp
// Pair
std::pair<int, std::string> p = {42, "answer"};
JsonValue j = to_json(p);
// Result: [42,"answer"]

// Tuple
std::tuple<int, std::string, bool> t = {1, "test", true};
JsonValue j = to_json(t);
// Result: [1,"test",true]
```

### to_json_string

```cpp
// Compact (default)
std::string json = to_json_string(42);
// "42"

std::vector<int> vec = {1, 2, 3};
std::string json = to_json_string(vec);
// "[1,2,3]"

// Pretty-printed
std::string json = to_json_string(vec, true);
// [
//   1,
//   2,
//   3
// ]
```

### to_json_stream

```cpp
std::ostringstream oss;
to_json_stream(oss, vec);
std::string json = oss.str();

// Direct to file
std::ofstream file("output.json");
to_json_stream(file, vec, true);  // pretty-print
```

---

## Parsing API

### parse_json

```cpp
std::string json = R"({"key": 42})";
JsonValue val = parse_json(json);

assert(val.is_object());
const JsonObject& obj = std::get<JsonObject>(val);
int64_t key_val = std::get<int64_t>(obj.at("key"));
```

### Number Parsing

**Integer vs Double Decision:**

```cpp
parse_json("42");       // int64_t
parse_json("42.0");     // double (has decimal point)
parse_json("42e0");     // double (has exponent)
parse_json("42.5");     // double
parse_json("-42");      // int64_t
```

**Large Integers:**

```cpp
parse_json("9223372036854775807");   // int64_t (INT64_MAX)
parse_json("9223372036854775808");   // double (out of int64 range)
```

### Special Values

```cpp
// NaN and Infinity (policy-controlled)
parse_json("NaN");       // quiet_NaN (if policy allows)
parse_json("Infinity");  // +infinity (if policy allows)
parse_json("-Infinity"); // -infinity (if policy allows)
```

With `StandardJsonPolicy` (default), these throw errors.
With `CompatJsonPolicy`, they parse successfully.

### Unicode Escapes

```cpp
parse_json(R"("\u0048\u0065\u006C\u006C\u006F")");
// Result: "Hello"

// Surrogate pairs (UTF-16 to UTF-8)
parse_json(R"("\uD834\uDD1E")");
// Result: "𝄞" (musical symbol G clef, U+1D11E)
```

**Supported:**
- `\uXXXX` for BMP codepoints (U+0000 to U+FFFF)
- `\uHHHH\uLLLL` for surrogate pairs (U+10000 to U+10FFFF)
- Automatic UTF-8 encoding

**Not Supported:**
- Invalid surrogate sequences (throws error)
- Overlong UTF-8 (not validated in input)

### Error Messages

```cpp
try {
    parse_json(R"({"key": )");
} catch (const std::runtime_error& e) {
    // e.what() = "JSON parse error: unexpected end of input"
}

try {
    parse_json(R"({"key": 42,})");  // Trailing comma
} catch (const std::runtime_error& e) {
    // e.what() = "JSON parse error: invalid value at position X"
}
```

All parse errors include position information.

---

## Macro System

### CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE

**Usage:**

```cpp
struct Point {
    int x;
    int y;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)

// Generates:
// inline void to_json(JsonValue& j, const Point& value);
// inline void from_json(const JsonValue& j, Point& value);
```

**Complete Example:**

```cpp
#include "JsonLite.h"
using namespace cpp_utilities;

struct Config {
    int port;
    std::string host;
    std::vector<std::string> allowed_ips;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host, allowed_ips)

int main() {
    Config cfg{8080, "localhost", {"127.0.0.1", "::1"}};
    
    // Serialize
    std::string json = to_json_string(cfg);
    // {"port":8080,"host":"localhost","allowed_ips":["127.0.0.1","::1"]}
    
    // Deserialize
    Config loaded;
    from_json(parse_json(json), loaded);
}
```

**Required Fields:**

All fields are required unless they're `std::optional`:

```cpp
struct Config {
    int port;
    std::optional<int> timeout;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, timeout)

// Valid:
from_json(parse_json(R"({"port":8080})"), cfg);  // timeout is optional

// Invalid:
from_json(parse_json(R"({"timeout":30})"), cfg); // Missing required field 'port'
// Throws: "Required field missing: 'port'"
```

**Field Order:**

Fields appear in JSON in the order listed in the macro:

```cpp
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)
// Always: {"x":1,"y":2}

CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, y, x)
// Always: {"y":2,"x":1}
```

**Limitations:**

- Max 20 fields per struct
- Field names in JSON match C++ field names exactly
- No custom JSON field names (use manual implementation for that)

### CPP_JSON_DEFINE_TYPE_OPTIONAL

**Use when all fields should be optional:**

```cpp
struct Settings {
    int window_width = 1920;
    int window_height = 1080;
    bool fullscreen = false;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(Settings, window_width, window_height, fullscreen)

// All valid:
from_json(parse_json(R"({})"), settings);                    // Uses defaults
from_json(parse_json(R"({"fullscreen":true})"), settings);  // Partial update
from_json(parse_json(R"({"window_width":800,"window_height":600})"), settings);
```

**Behavior:**
- Missing fields: Keep existing value in struct
- Present fields: Update from JSON
- No "Required field missing" errors

### CPP_JSON_DEFINE_TYPE_INTRUSIVE

**For private members:**

```cpp
class Player {
private:
    int health_;
    int score_;
    
public:
    Player() : health_(100), score_(0) {}
    
    CPP_JSON_DEFINE_TYPE_INTRUSIVE(Player, health_, score_)
    
    int health() const { return health_; }
    int score() const { return score_; }
};

// Now serializable despite private members
Player p;
std::string json = to_json_string(p);
// {"health_":100,"score_":0}
```

**Generated Functions:**
```cpp
friend void to_json(JsonValue& j, const Player& value);
friend void from_json(const JsonValue& j, Player& value);
```

---

## Policy System

### StandardJsonPolicy

```cpp
struct StandardJsonPolicy {
    static constexpr bool pretty_print = false;
    static constexpr int numeric_precision = 16;
    static constexpr int indent_step = 4;
    static constexpr bool allow_nan_inf = false;
    static constexpr bool escape_unicode = true;
    static constexpr size_t max_parse_depth = 512;
    static constexpr size_t max_dump_depth = 512;
};
```

**Default behavior:**
- Compact output
- 16 decimal places for doubles
- No NaN/Infinity (outputs null instead)
- Unicode escapes for non-ASCII: `"\u00E9"` not `"é"`
- Maximum nesting: 512 levels

### PrettyJsonPolicy

```cpp
struct PrettyJsonPolicy : StandardJsonPolicy {
    static constexpr bool pretty_print = true;
};
```

**Output:**

```cpp
to_json_string<PrettyJsonPolicy>(vec);
// [
//     1,
//     2,
//     3
// ]
```

### CompatJsonPolicy

```cpp
struct CompatJsonPolicy : StandardJsonPolicy {
    static constexpr bool allow_nan_inf = true;
    static constexpr bool escape_unicode = false;
};
```

**Use for:**
- JavaScript interoperability (allows NaN, Infinity)
- Human-readable Unicode (no escaping)

**Output:**

```cpp
double nan = std::numeric_limits<double>::quiet_NaN();
to_json_string<CompatJsonPolicy>(nan);
// "NaN"  (not "null")

std::string utf8 = "café";
to_json_string<CompatJsonPolicy>(utf8);
// "café"  (not "caf\u00E9")
```

### Custom Policies

```cpp
struct MyPolicy : StandardJsonPolicy {
    static constexpr int numeric_precision = 6;   // Less precision
    static constexpr size_t max_parse_depth = 64; // Shallow depth limit
};

std::string json = to_json_string<MyPolicy>(3.14159265359);
// "3.141593" (6 decimal places)
```

**Policy Parameters:**

| Parameter | Type | Default | Effect |
|-----------|------|---------|--------|
| `pretty_print` | bool | false | Multi-line with indentation |
| `numeric_precision` | int | 16 | Decimal places for doubles |
| `indent_step` | int | 4 | Spaces per indent level |
| `allow_nan_inf` | bool | false | Allow NaN/Infinity values |
| `escape_unicode` | bool | true | Escape non-ASCII as \uXXXX |
| `max_parse_depth` | size_t | 512 | Max nesting during parse |
| `max_dump_depth` | size_t | 512 | Max nesting during serialize |

---

## Type Conversion

### to_json Overloads

**Two forms:**

```cpp
// Output parameter (used by macros)
void to_json(JsonValue& j, const T& value);

// Return value (convenient)
JsonValue to_json(const T& value);
```

**Both are equivalent:**

```cpp
JsonValue j1;
to_json(j1, 42);

JsonValue j2 = to_json(42);

assert(j1 == j2);  // Requires operator== on JsonValue
```

### from_json Validation

**Integer Range Checking:**

```cpp
JsonValue j = 2147483648LL;  // INT_MAX + 1

int value;
try {
    from_json(j, value);
} catch (const std::runtime_error& e) {
    // "JSON value out of range for int: 2147483648"
}
```

**Happens for:**
- int64_t → int (on all platforms)
- int64_t → unsigned int
- int64_t → long (on 32-bit systems)
- int64_t → long long (rare, but checked)
- double → any integer type (also checks fractional part)

**Fractional Part Checking:**

```cpp
JsonValue j = 42.7;

int value;
try {
    from_json(j, value);
} catch (const std::runtime_error& e) {
    // "JSON value has fractional part, cannot convert to int: 42.700000"
}
```

**This is intentional:** Prevents silent data loss.

**If you want truncation:**

```cpp
double d = std::get<double>(j);
int value = static_cast<int>(d);  // Your choice to truncate
```

### Unsigned Integer Edge Cases

```cpp
JsonValue j = -1;

unsigned int value;
try {
    from_json(j, value);
} catch (const std::runtime_error& e) {
    // "JSON value is negative, cannot convert to unsigned int: -1"
}
```

**For unsigned long with double:**

```cpp
JsonValue j = 1e20;  // Stored as double

unsigned long value;
from_json(j, value);
// Works if value fits (no fractional part check though)
```

---

## File I/O

### load_json_from_file

```cpp
JsonValue val = load_json_from_file("config.json");
```

**Behavior:**
- Reads entire file into memory (std::string)
- Binary mode (preserves \r\n on Windows)
- Throws if file doesn't exist
- Throws if read error
- Throws if JSON invalid

**Error Handling:**

```cpp
try {
    JsonValue val = load_json_from_file("missing.json");
} catch (const std::runtime_error& e) {
    // "Failed to open file for reading: missing.json"
}
```

### save_json_to_file

```cpp
JsonValue val = /* ... */;
save_json_to_file("output.json", val);

// With pretty printing
save_json_to_file<PrettyJsonPolicy>("output.json", val, true);
```

**Behavior:**
- Text mode (uses platform line endings)
- Overwrites existing file
- Throws if cannot create file
- Throws if write error

### save_params / load_params

**Type-safe convenience functions:**

```cpp
struct Config {
    int port;
    std::string host;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port, host)

// Save
Config cfg{8080, "localhost"};
save_params("config.json", cfg);

// Load
Config loaded = load_params<Config>("config.json");
```

**Equivalent to:**

```cpp
save_json_to_file<PrettyJsonPolicy>("config.json", to_json(cfg), true);
Config loaded = from_json<Config>(load_json_from_file("config.json"));
```

### save_params_with_backup

```cpp
Config cfg{8080, "localhost"};
save_params_with_backup("config.json", cfg);
// Creates: config.json.bak (if config.json exists)
//    Then: config.json (new data)

// Custom backup suffix
save_params_with_backup("config.json", cfg, ".old");
// Creates: config.json.old
```

**Atomic behavior:**
1. Check if target file exists
2. If yes: Copy to backup (throws on failure)
3. Write new data (throws on failure)

**Not truly atomic:** If step 3 fails, backup exists but new file may be corrupt.

---

## Error Handling

### Exception Types

All errors throw `std::runtime_error` with descriptive messages.

**No custom exception types.** Check message content for details.

### Parse Errors

```cpp
try {
    parse_json(R"({"key": })");
} catch (const std::runtime_error& e) {
    std::string msg = e.what();
    // "JSON parse error: unexpected end of input"
}
```

**Position Information:**

All parse errors include character position:

```cpp
try {
    parse_json(R"({"a":1, "b": })");
} catch (const std::runtime_error& e) {
    // "JSON parse error: unexpected end of input"
    // Position is in the message
}
```

**Common Parse Errors:**

| Input | Error |
|-------|-------|
| `{"key":}` | "invalid value at position X" |
| `{"key": 42` | "unterminated object at position X" |
| `[1, 2, 3` | "unterminated array at position X" |
| `{"key": 01}` | "invalid number '01' at position X" |
| `"unterminated` | "unterminated string at position X" |
| `{"a":1} extra` | "extra data after JSON value at position X" |

### Conversion Errors

```cpp
try {
    JsonValue j = 3.7;
    int value;
    from_json(j, value);
} catch (const std::runtime_error& e) {
    // "JSON value has fractional part, cannot convert to int: 3.700000"
}
```

**Error Message Format:**

```cpp
"Error deserializing field 'fieldname': <underlying error>"
```

**Example:**

```cpp
struct Config {
    int port;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port)

try {
    from_json(parse_json(R"({"port": "8080"})"), cfg);
} catch (const std::runtime_error& e) {
    // "Error deserializing field 'port': JSON type mismatch: expected number"
}
```

### Depth Limit Errors

```cpp
// Create JSON with 600 nested objects
std::string deep_json = std::string(600, '{') + std::string(600, '}');

try {
    parse_json(deep_json);
} catch (const std::runtime_error& e) {
    // "JSON parse error: maximum nesting depth (512) exceeded at position X"
}
```

---

## Performance

### Time Complexity

| Operation | Complexity | Notes |
|-----------|------------|-------|
| Parse JSON | O(n) | n = input length |
| Serialize | O(n) | n = output length |
| Object field lookup | O(log k) | k = number of fields (std::map) |
| Array access | O(1) | std::vector |
| to_json (struct) | O(f) | f = number of fields |

### Space Complexity

| Structure | Size | Notes |
|-----------|------|-------|
| JsonValue | 40-48 bytes | std::variant overhead |
| Empty JsonObject | 48 bytes | std::map overhead |
| Empty JsonArray | 24 bytes | std::vector overhead |

### Allocation Pattern

**Parse:**
- One allocation per: string, array, object
- Strings reserve 64 bytes initially
- Arrays/objects grow dynamically

**Serialize:**
- Builds std::ostringstream internally
- Single allocation for final string

### Benchmark Reference

**Typical performance (modern x86_64, -O2):**

| Operation | Time | Notes |
|-----------|------|-------|
| Parse 1KB config | ~50 µs | Simple object, 10 fields |
| Parse 100KB JSON | ~5 ms | Deeply nested structures |
| Serialize 1KB | ~30 µs | Simple object |
| Serialize 100KB | ~3 ms | Complex nested data |

**These are ballpark figures. Your mileage will vary.**

### Optimization Opportunities

**If you need faster lookup:**

Fork the library and change:
```cpp
using JsonObject = std::map<std::string, JsonValue>;
```
to:
```cpp
using JsonObject = std::unordered_map<std::string, JsonValue>;
```

**Trade-off:** Lose deterministic iteration order.

**If you need faster parsing:**

You're using the wrong library. Consider:
- simdjson (SIMD-accelerated)
- rapidjson (hand-optimized)

**If you need less memory:**

- Use `std::string_view` in parse_json input (already does)
- Don't store JsonValue long-term (convert to typed structs)
- Use integer types that fit your data (int16_t if possible)

---

## Memory Model

### Stack Usage

**Parser:**
- Recursive descent: ~100-200 bytes per nesting level
- Max depth 512 → ~50-100KB stack at deepest point
- Fine for normal stacks (typically 1-8MB)

**Serializer:**
- Similar recursive structure
- Depth limit: 512 levels

**If you hit stack limits:**

Increase stack size:
```bash
ulimit -s 16384  # 16MB stack (Linux)
```

Or reduce `max_parse_depth` / `max_dump_depth` in policy.

### Heap Usage

**Parse 1MB JSON file:**
- Input string: 1MB
- Parsed JsonValue tree: ~2-4MB (depends on structure)
- Peak memory: ~3-5MB

**Why the overhead:**
- std::map nodes: 32-48 bytes each
- std::string: 32 bytes + data + padding
- JsonValue: 48 bytes per value

**Example:**

```json
{"key": "value"}
```

**Memory breakdown:**
- JsonObject: 48 bytes (map overhead)
- Map node: 40 bytes
- Key string: 32 + 3 + padding = ~40 bytes
- JsonValue (object): 48 bytes
- Value string: 32 + 5 + padding = ~40 bytes
- JsonValue (string): 48 bytes

**Total: ~256 bytes for 16 bytes of JSON text.**

**Not memory-efficient.** But correct and simple.

---

## Thread Safety

### Not Thread-Safe

**Do NOT:**

```cpp
// Thread 1
JsonValue shared;
to_json(shared, 42);

// Thread 2 (concurrent)
std::cout << std::get<int64_t>(shared);  // RACE CONDITION
```

### Safe Patterns

**Pattern 1: Thread-local storage**

```cpp
thread_local JsonValue local_json;
// Each thread has its own copy
```

**Pattern 2: External synchronization**

```cpp
std::mutex mtx;
JsonValue shared;

// Thread 1
{
    std::lock_guard<std::mutex> lock(mtx);
    to_json(shared, 42);
}

// Thread 2
{
    std::lock_guard<std::mutex> lock(mtx);
    int val = std::get<int64_t>(shared);
}
```

**Pattern 3: Message passing**

```cpp
// Thread 1: Serialize
std::string json = to_json_string(data);
queue.push(json);  // Thread-safe queue

// Thread 2: Deserialize
std::string json = queue.pop();
auto data = from_json<MyType>(parse_json(json));
```

---

## Platform Compatibility

### Tested Compilers

- GCC 7.3+ (C++17 support)
- Clang 6.0+
- MSVC 2017+ (15.7+)
- Apple Clang 10.0+

### Tested Platforms

- Linux x86_64
- Windows x64
- macOS x86_64 / ARM64
- FreeBSD x86_64

**Should work on any platform with C++17 support.**

### 32-bit vs 64-bit

**Integer Types:**

| Type | 32-bit | 64-bit |
|------|--------|--------|
| int | 32-bit | 32-bit |
| long | 32-bit | 64-bit |
| long long | 64-bit | 64-bit |
| size_t | 32-bit | 64-bit |

**Range checks account for this:**

```cpp
// On 32-bit systems:
int64_t val = 3000000000LL;  // > INT32_MAX
JsonValue j = val;

int result;
from_json(j, result);  // Throws: out of range
```

**On 64-bit, `long` and `int64_t` are the same:**

```cpp
long val = 42;
JsonValue j = val;
assert(j.is_int());  // true, stored as int64_t
```

**On 32-bit, `long` may overflow when stored:**

```cpp
// 32-bit system
int64_t val = 3000000000LL;
JsonValue j = val;

long result;
from_json(j, result);  // Throws: out of range for long
```

### Endianness

**Not relevant.** JSON is text-based, no binary serialization.

### Character Encoding

**Assumption: UTF-8**

Input strings should be UTF-8 encoded. If you pass Latin-1 or Windows-1252:

```cpp
std::string latin1 = "\xE9";  // é in Latin-1
JsonValue j = latin1;
std::string json = to_json_string(j);
// Output: "\u00E9" or invalid UTF-8 depending on escaping
```

**Behavior is undefined for invalid UTF-8 input.**

---

## Common Patterns

### Pattern: Config File Management

```cpp
struct AppConfig {
    int port = 8080;
    std::string host = "localhost";
    std::optional<std::string> log_file;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(AppConfig, port, host, log_file)

class ConfigManager {
    AppConfig config_;
    std::string filename_;
    
public:
    ConfigManager(const std::string& file) : filename_(file) {
        if (std::ifstream(file).good()) {
            config_ = load_params<AppConfig>(file);
        } else {
            // Use defaults, save initial config
            save_params(file, config_);
        }
    }
    
    void save() {
        save_params_with_backup(filename_, config_);
    }
    
    AppConfig& config() { return config_; }
};
```

### Pattern: Versioned Config

```cpp
struct ConfigV1 {
    int version = 1;
    int port;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ConfigV1, version, port)

struct ConfigV2 {
    int version = 2;
    int port;
    std::string host;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ConfigV2, version, port, host)

ConfigV2 load_config(const std::string& file) {
    JsonValue json = load_json_from_file(file);
    
    if (!json.is_object()) {
        throw std::runtime_error("Invalid config format");
    }
    
    const auto& obj = std::get<JsonObject>(json);
    auto it = obj.find("version");
    
    if (it == obj.end() || !it->second.is_int()) {
        throw std::runtime_error("Missing version field");
    }
    
    int version = std::get<int64_t>(it->second);
    
    if (version == 1) {
        ConfigV1 v1;
        from_json(json, v1);
        
        // Migrate to V2
        ConfigV2 v2;
        v2.version = 2;
        v2.port = v1.port;
        v2.host = "localhost";  // Default for new field
        
        return v2;
    } else if (version == 2) {
        ConfigV2 v2;
        from_json(json, v2);
        return v2;
    } else {
        throw std::runtime_error("Unsupported config version: " + std::to_string(version));
    }
}
```

### Pattern: Partial Updates

```cpp
struct Settings {
    int width = 1920;
    int height = 1080;
    bool fullscreen = false;
};
CPP_JSON_DEFINE_TYPE_OPTIONAL(Settings, width, height, fullscreen)

// Update only specified fields
void update_settings(Settings& settings, const std::string& json_patch) {
    Settings temp = settings;  // Copy current
    from_json(parse_json(json_patch), temp);  // Apply patch
    settings = temp;  // Commit if no exception
}

// Usage:
Settings s;
update_settings(s, R"({"fullscreen": true})");
// width and height unchanged
```

### Pattern: Type Erasure for Heterogeneous Data

```cpp
struct Message {
    std::string type;
    JsonValue data;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Message, type, data)

void handle_message(const Message& msg) {
    if (msg.type == "player_update") {
        PlayerData pd;
        from_json(msg.data, pd);
        // handle player update
    } else if (msg.type == "chat_message") {
        ChatData cd;
        from_json(msg.data, cd);
        // handle chat
    }
}
```

### Pattern: Array of Different Types

```cpp
JsonArray arr;
arr.push_back(42);
arr.push_back("hello");
arr.push_back(3.14);
arr.push_back(nullptr);

for (const auto& val : arr) {
    std::visit([](auto&& v) {
        using T = std::decay_t<decltype(v)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            std::cout << "Integer: " << v << "\n";
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::cout << "String: " << v << "\n";
        } else if constexpr (std::is_same_v<T, double>) {
            std::cout << "Double: " << v << "\n";
        } else if constexpr (std::is_same_v<T, std::nullptr_t>) {
            std::cout << "Null\n";
        }
    }, val);
}
```

---

## Anti-Patterns

### Anti-Pattern: Storing Large Data Sets

**Don't:**

```cpp
std::vector<double> huge_array(10000000);  // 10M elements
std::string json = to_json_string(huge_array);
// Generates 100MB+ JSON string
// Parses slowly
// Wastes memory
```

**Do:**

Use binary formats for large numeric arrays:
- HDF5
- NumPy .npy
- Protocol Buffers
- Custom binary format

### Anti-Pattern: Real-Time Parsing

**Don't:**

```cpp
// In game loop (60 FPS)
while (running) {
    std::string json = receive_network_data();
    auto data = from_json<GameState>(parse_json(json));
    render(data);
}
```

**Do:**

Parse once, send binary:

```cpp
// Sender
GameState state;
std::string json = to_json_string(state);
send_once(json);  // During initialization

// Receiver  
GameState state = from_json<GameState>(parse_json(json));
// Then use binary protocol for updates
```

### Anti-Pattern: Using JSON for Large Matrices

**Don't:**

```cpp
std::vector<std::vector<double>> matrix(1000, std::vector<double>(1000));
save_params("matrix.json", matrix);
// Result: ~50MB JSON file
// Parse time: seconds
```

**Do:**

Use NumPy, HDF5, or Eigen binary formats.

### Anti-Pattern: Parsing Untrusted Input Without Validation

**Don't:**

```cpp
// User-supplied JSON from web
std::string user_json = http_request.body();
auto data = parse_json(user_json);
// No size limits!
// No depth validation beyond 512!
// Could be gigabytes!
```

**Do:**

```cpp
// Check size
if (user_json.size() > 1024 * 1024) {  // 1MB limit
    throw std::runtime_error("JSON too large");
}

// Check depth in policy
struct SafePolicy : StandardJsonPolicy {
    static constexpr size_t max_parse_depth = 32;  // Shallow
};

auto data = parse_json<SafePolicy>(user_json);
```

### Anti-Pattern: Comparing Floats for Equality

**Don't:**

```cpp
JsonValue j1 = 0.1 + 0.2;
JsonValue j2 = 0.3;

// Doesn't work:
assert(std::get<double>(j1) == std::get<double>(j2));  // May fail
```

**Do:**

```cpp
double a = std::get<double>(j1);
double b = std::get<double>(j2);
assert(std::abs(a - b) < 1e-10);
```

### Anti-Pattern: Serializing Raw Pointers

**Don't:**

```cpp
struct Node {
    int value;
    Node* next;  // Pointer!
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Node, value, next)
// Won't compile (no to_json for Node*)
```

**Do:**

```cpp
struct Node {
    int value;
    std::optional<int> next_value;  // Or use ID
};

// Or serialize graph as adjacency list
```

---

## Troubleshooting

### Compilation Errors

**Error: `std::variant` not found**

```
error: 'variant' is not a member of 'std'
```

**Solution:** Compiler doesn't support C++17.

```bash
g++ -std=c++17 ...  # Add this flag
```

**Error: Macro expansion issues**

```
error: expected ';' after struct definition
```

**Check:** Did you forget semicolon after macro?

```cpp
struct Point { int x, y; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)  // No semicolon!

// Correct:
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Point, x, y)  // Still no semicolon (macro adds it)
```

**Error: Ambiguous overload**

```
error: call to 'to_json' is ambiguous
```

**Cause:** You defined your own `to_json` that conflicts.

**Solution:** Use namespaces or fully qualify:

```cpp
cpp_utilities::to_json(j, value);
```

### Runtime Errors

**"JSON parse error: unexpected end of input"**

Your JSON is truncated:

```cpp
parse_json(R"({"key": )");  // Missing value and closing brace
```

**"JSON type mismatch: expected number"**

You're trying to deserialize a string as a number:

```json
{"port": "8080"}  // String, not number
```

**Fix:**

```json
{"port": 8080}
```

**"Required field missing: 'X'"**

Your JSON doesn't have field X:

```cpp
struct Config { int port; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Config, port)

from_json(parse_json(R"({})"), cfg);  // Missing 'port'
```

**Fix:** Add field to JSON or make it optional:

```cpp
struct Config { std::optional<int> port; };
```

**"JSON value has fractional part, cannot convert to int"**

Your JSON has `42.5` and you're deserializing to `int`.

**Fix:** Use `double` or change JSON to `42`.

### Performance Issues

**Parsing is slow**

- Check JSON size (>10MB?)
- Check nesting depth (>100 levels?)
- Profile with `-pg` flag
- Consider streaming parser (not this library)

**Serialization is slow**

- Same checks as parsing
- Check if you're pretty-printing (slow)
- Consider binary serialization for hot paths

**Memory usage is high**

- JsonValue is 48 bytes minimum
- std::map overhead is ~40 bytes per entry
- Consider flatter structures

---

## Compiler Requirements

### C++17 Features Used

| Feature | Requirement |
|---------|-------------|
| std::variant | C++17 |
| std::optional | C++17 |
| std::string_view | C++17 |
| constexpr if | C++17 |
| std::void_t | C++17 |
| Fold expressions | C++17 |
| [[nodiscard]] | C++17 |
| Structured bindings | C++17 |

**None of these are optional.** You must use C++17 or later.

### Compiler Version Matrix

| Compiler | Min Version | Notes |
|----------|-------------|-------|
| GCC | 7.3 | Full C++17 support |
| Clang | 6.0 | Full C++17 support |
| MSVC | 19.14 (2017 15.7) | Requires /std:c++17 flag |
| Apple Clang | 10.0 | Ships with Xcode 10+ |
| Intel C++ | 19.0 | With -std=c++17 |

### Standard Library Requirements

You need:
- `<variant>`
- `<optional>`
- `<string_view>`
- All standard headers (no additional libraries)

**Portable to:**
- libstdc++ (GCC)
- libc++ (Clang)
- MSVC STL
- Any C++17-compliant standard library

---

## Advanced Topics

### Custom Type Serialization

**Don't want to use macros? Write your own:**

```cpp
struct Color {
    uint8_t r, g, b, a;
};

// Custom serialization
inline void to_json(JsonValue& j, const Color& c) {
    JsonObject obj;
    obj["r"] = c.r;
    obj["g"] = c.g;
    obj["b"] = c.b;
    obj["a"] = c.a;
    j = std::move(obj);
}

inline void from_json(const JsonValue& j, Color& c) {
    if (!j.is_object()) throw std::runtime_error("Expected object");
    const auto& obj = std::get<JsonObject>(j);
    
    c.r = static_cast<uint8_t>(std::get<int64_t>(obj.at("r")));
    c.g = static_cast<uint8_t>(std::get<int64_t>(obj.at("g")));
    c.b = static_cast<uint8_t>(std::get<int64_t>(obj.at("b")));
    c.a = static_cast<uint8_t>(std::get<int64_t>(obj.at("a")));
}
```

### Custom Field Names

```cpp
struct User {
    int id;
    std::string username;
};

// Custom JSON field names
inline void to_json(JsonValue& j, const User& u) {
    JsonObject obj;
    obj["userId"] = u.id;          // camelCase
    obj["userName"] = u.username;  // camelCase
    j = std::move(obj);
}

inline void from_json(const JsonValue& j, User& u) {
    const auto& obj = std::get<JsonObject>(j);
    u.id = static_cast<int>(std::get<int64_t>(obj.at("userId")));
    u.username = std::get<std::string>(obj.at("userName"));
}
```

### Enum Serialization

```cpp
enum class Status {
    Idle,
    Running,
    Error
};

inline void to_json(JsonValue& j, Status s) {
    switch (s) {
        case Status::Idle: j = "idle"; break;
        case Status::Running: j = "running"; break;
        case Status::Error: j = "error"; break;
    }
}

inline void from_json(const JsonValue& j, Status& s) {
    const auto& str = std::get<std::string>(j);
    if (str == "idle") s = Status::Idle;
    else if (str == "running") s = Status::Running;
    else if (str == "error") s = Status::Error;
    else throw std::runtime_error("Invalid status: " + str);
}
```

### Polymorphism

```cpp
struct Shape {
    virtual ~Shape() = default;
    virtual std::string type() const = 0;
};

struct Circle : Shape {
    double radius;
    std::string type() const override { return "circle"; }
};

struct Rectangle : Shape {
    double width, height;
    std::string type() const override { return "rectangle"; }
};

// Serialize with type tag
inline void to_json(JsonValue& j, const Shape& s) {
    JsonObject obj;
    obj["type"] = s.type();
    
    if (auto* c = dynamic_cast<const Circle*>(&s)) {
        obj["radius"] = c->radius;
    } else if (auto* r = dynamic_cast<const Rectangle*>(&s)) {
        obj["width"] = r->width;
        obj["height"] = r->height;
    }
    
    j = std::move(obj);
}

// Deserialize with factory
std::unique_ptr<Shape> shape_from_json(const JsonValue& j) {
    const auto& obj = std::get<JsonObject>(j);
    const auto& type = std::get<std::string>(obj.at("type"));
    
    if (type == "circle") {
        auto c = std::make_unique<Circle>();
        c->radius = std::get<double>(obj.at("radius"));
        return c;
    } else if (type == "rectangle") {
        auto r = std::make_unique<Rectangle>();
        r->width = std::get<double>(obj.at("width"));
        r->height = std::get<double>(obj.at("height"));
        return r;
    }
    
    throw std::runtime_error("Unknown shape type: " + type);
}
```

---

## Migrating From Other Libraries

### From nlohmann/json

**nlohmann:**
```cpp
#include <nlohmann/json.hpp>
using json = nlohmann::json;

json j = {{"key", 42}};
int val = j["key"];
```

**JsonLite:**
```cpp
#include "JsonLite.h"
using namespace cpp_utilities;

JsonObject obj;
obj["key"] = 42;
JsonValue j = obj;

int val = static_cast<int>(std::get<int64_t>(
    std::get<JsonObject>(j).at("key")
));
```

**Or use structs:**

```cpp
struct Data { int key; };
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(Data, key)

Data d{42};
std::string json = to_json_string(d);
Data loaded = from_json<Data>(parse_json(json));
```

### From RapidJSON

**RapidJSON:**
```cpp
#include "rapidjson/document.h"

rapidjson::Document d;
d.Parse(json_str.c_str());
int val = d["key"].GetInt();
```

**JsonLite:**
```cpp
JsonValue j = parse_json(json_str);
const auto& obj = std::get<JsonObject>(j);
int val = static_cast<int>(std::get<int64_t>(obj.at("key")));
```

### From Boost.PropertyTree

**Boost:**
```cpp
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

boost::property_tree::ptree pt;
boost::property_tree::read_json(file, pt);
int val = pt.get<int>("key");
```

**JsonLite:**
```cpp
JsonValue j = load_json_from_file(file);
const auto& obj = std::get<JsonObject>(j);
int val = static_cast<int>(std::get<int64_t>(obj.at("key")));
```

---

## FAQ

**Q: Why std::map instead of std::unordered_map?**

A: Deterministic iteration order. For config files, you want consistent output for version control. The performance difference is negligible for typical use (10-100 fields).

**Q: Why no streaming API?**

A: Complexity vs benefit. Config files fit in memory. If you need streaming, use a specialized library (rapidjson, simdjson).

**Q: Why throw exceptions for fractional values?**

A: Explicit is better than implicit. Silent truncation (`3.7` → `3`) is a bug waiting to happen. If you want truncation, cast explicitly.

**Q: Can I use this with C++14?**

A: No. Requires C++17 for `std::variant`, `std::optional`, `std::string_view`.

**Q: Is it faster than X?**

A: Probably not. Optimized for correctness and simplicity. If you need maximum speed, use simdjson or rapidjson.

**Q: Can I contribute?**

A: This is a single-file, zero-dependency library. Scope is intentionally limited. Fork if you need features.

**Q: Why can't I serialize pointers?**

A: Pointers are memory addresses. Meaningless in JSON. Use object IDs or serialize pointed-to values.

**Q: How do I handle circular references?**

A: You don't. Use DAG (directed acyclic graph) or break cycles with IDs.

**Q: What about JSON Schema validation?**

A: Not supported. Validate after deserialization in your application code.

**Q: Can I use this in production?**

A: Yes, for config files and similar use cases. No, for high-frequency trading or parsing untrusted gigabyte inputs.

---

## Complete Example: RESTful API Client

```cpp
#include "JsonLite.h"
#include <curl/curl.h>  // External dependency for example

using namespace cpp_utilities;

struct User {
    int id;
    std::string name;
    std::string email;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(User, id, name, email)

struct CreateUserRequest {
    std::string name;
    std::string email;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(CreateUserRequest, name, email)

class ApiClient {
    std::string base_url_;
    
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t new_length = size * nmemb;
        s->append((char*)contents, new_length);
        return new_length;
    }
    
    std::string http_post(const std::string& endpoint, const std::string& body) {
        CURL* curl = curl_easy_init();
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, (base_url_ + endpoint).c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = curl_easy_perform(curl);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            throw std::runtime_error("HTTP request failed");
        }
        
        return response;
    }
    
public:
    ApiClient(const std::string& base_url) : base_url_(base_url) {}
    
    User create_user(const std::string& name, const std::string& email) {
        CreateUserRequest req{name, email};
        std::string json_req = to_json_string(req);
        
        std::string json_resp = http_post("/users", json_req);
        
        JsonValue resp = parse_json(json_resp);
        return from_json<User>(resp);
    }
    
    std::vector<User> list_users() {
        std::string json_resp = http_post("/users/list", "{}");
        
        JsonValue resp = parse_json(json_resp);
        
        std::vector<User> users;
        from_json(resp, users);
        return users;
    }
};

int main() {
    ApiClient client("https://api.example.com");
    
    // Create user
    User new_user = client.create_user("Alice", "alice@example.com");
    std::cout << "Created user ID: " << new_user.id << "\n";
    
    // List all users
    auto users = client.list_users();
    for (const auto& user : users) {
        std::cout << user.id << ": " << user.name << " <" << user.email << ">\n";
    }
    
    return 0;
}
```

---

## Summary

**What you learned:**

- Integration (single header)
- Core concepts (JsonValue, JsonObject, JsonArray)
- Serialization (to_json, to_json_string)
- Parsing (parse_json, from_json)
- Macros (CPP_JSON_DEFINE_TYPE_*)
- Policies (Standard, Pretty, Compat)
- Type conversion (with validation)
- File I/O (load/save)
- Error handling (exceptions with context)
- Performance (O(n) parse, O(log k) lookup)
- Memory model (heap allocations, stack usage)
- Thread safety (not thread-safe)
- Platform compatibility (32/64-bit, endianness)
- Common patterns (config management, versioning)
- Anti-patterns (large data, real-time, untrusted input)
- Troubleshooting (compilation, runtime errors)

**You're now equipped to:**

- Integrate JsonLite into your project
- Serialize/deserialize complex data structures
- Handle errors appropriately
- Make informed performance decisions
- Avoid common pitfalls
- Write maintainable JSON-handling code

**Go build something.**
