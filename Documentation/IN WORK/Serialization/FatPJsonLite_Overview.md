# FatPJsonLite: A Fat-P Library Showcase

## Executive Summary

FatPJsonLite is the **fat_p-integrated version of JsonLite** that adds native serialization for fat_p types: `Expected`, `SmallVector`, `StrongId`, `EnumPlus` enums, and more. Unlike standalone JsonLite (requires manual from_json/to_json for fat_p types), FatPJsonLite provides **out-of-the-box serialization** where `Expected<T, E>` serializes as `{"value": T}` or `{"error": E}`, `SmallVector` serializes like `std::vector`, and `StrongId` preserves type safety through JSON round-trips.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// Using standalone JsonLite with fat_p types
Expected<Config, Error> result = loadConfig();

// Can't directly serialize!
JsonValue json = to_json(result);  // Compile error: no to_json for Expected

// Must write custom serialization
inline void to_json(JsonValue& j, const Expected<Config, Error>& exp) {
    if (exp) {
        j = JsonObject{{"value", to_json(*exp)}};
    } else {
        j = JsonObject{{"error", to_json(exp.error())}};
    }
}
// Repeat for every Expected<T, E> combination!

// SmallVector<T, N> vs std::vector<T>
SmallVector<int, 8> sv = {1, 2, 3};
// Also needs custom to_json even though it's vector-like
```

| Issue | HPC Impact |
|-------|------------|
| No fat_p type support | Manual serialization for each type |
| Expected handling | Verbose error/value discrimination |
| StrongId type safety | Lost through JSON (becomes plain int) |
| Enum string conversion | Manual for each enum |

### The Standard's Limitation

JsonLite is standalone by design (zero fat_p dependencies). Adding fat_p serialization to JsonLite would create circular dependencies. FatPJsonLite bridges this gap.

---

## Architecture: Extension via ADL Overloads

### The Mechanism: Overload Injection

```cpp
// FatPJsonLite.h includes JsonLite.h and adds overloads
#include "JsonLite.h"
#include "Expected.h"
#include "SmallVector.h"
#include "StrongId.h"
#include "EnumPlus.h"

namespace fat_p {

// Expected<T, E> serialization
template<typename T, typename E>
void to_json(JsonValue& j, const Expected<T, E>& exp) {
    if (exp) {
        j = JsonObject{{"value", to_json(*exp)}};
    } else {
        j = JsonObject{{"error", to_json(exp.error())}};
    }
}

template<typename T, typename E>
void from_json(const JsonValue& j, Expected<T, E>& exp) {
    const auto& obj = std::get<JsonObject>(j);
    if (obj.count("value")) {
        T val;
        from_json(obj.at("value"), val);
        exp = val;
    } else {
        E err;
        from_json(obj.at("error"), err);
        exp = Expected<T, E>{unexpect, err};
    }
}

} // namespace fat_p
```

**Why ADL (Argument-Dependent Lookup):**

When you call `to_json(value)`, the compiler searches:
1. Current namespace
2. Namespaces of argument types (ADL)

Since `Expected`, `SmallVector`, etc. are in `fat_p`, the overloads in `fat_p` namespace are found automatically.

---

## Feature Inventory

### 1. Expected Serialization

```cpp
Expected<int, std::string> success{42};
JsonValue json = to_json(success);
// {"value": 42}

Expected<int, std::string> failure{unexpect, "File not found"};
json = to_json(failure);
// {"error": "File not found"}

// Round-trip
auto restored = from_json<Expected<int, std::string>>(json);
```

### 2. SmallVector Serialization

```cpp
SmallVector<int, 8> sv = {1, 2, 3, 4, 5};
JsonValue json = to_json(sv);
// [1, 2, 3, 4, 5]

// Identical format to std::vector—drop-in replacement
auto restored = from_json<SmallVector<int, 8>>(json);
```

### 3. StrongId Preservation

```cpp
// Define strongly-typed IDs
using UserId = StrongId<struct UserTag, int64_t>;
using OrderId = StrongId<struct OrderTag, int64_t>;

UserId user{12345};
JsonValue json = to_json(user);
// {"type": "UserId", "value": 12345}

// Type safety through round-trip
auto restored = from_json<UserId>(json);
// from_json<OrderId>(json) → type mismatch error!
```

### 4. EnumPlus String Serialization

```cpp
enum class Status { Pending, Active, Completed };
// With EnumStringPolicy specialization

Status s = Status::Active;
JsonValue json = to_json(s);
// "Active" (not 1)

auto restored = from_json<Status>(json);
// Status::Active
```

### 5. Optional Integration

```cpp
std::optional<Expected<int, Error>> opt_result;

// None
JsonValue json = to_json(opt_result);
// null

// Some success
opt_result = Expected<int, Error>{42};
json = to_json(opt_result);
// {"value": 42}

// Some error
opt_result = Expected<int, Error>{unexpect, Error::NotFound};
json = to_json(opt_result);
// {"error": "NotFound"}
```

### 6. Nested fat_p Types

```cpp
struct ApiResponse {
    Expected<std::vector<UserId>, std::string> users;
    SmallVector<std::string, 4> warnings;
    std::optional<Status> status;
};
CPP_JSON_DEFINE_TYPE_NON_INTRUSIVE(ApiResponse, users, warnings, status)

ApiResponse resp{
    Expected<std::vector<UserId>, std::string>{{UserId{1}, UserId{2}}},
    {"deprecated field", "rate limit"},
    Status::Active
};

JsonValue json = to_json(resp);
// {
//   "users": {"value": [{"type":"UserId","value":1}, {"type":"UserId","value":2}]},
//   "warnings": ["deprecated field", "rate limit"],
//   "status": "Active"
// }
```

### 7. Error Handling

```cpp
// Type mismatch detection
JsonValue json = parse(R"({"value": 42})");

// Correct type
auto exp = from_json<Expected<int, std::string>>(json);  // OK

// Wrong StrongId type
try {
    auto user = from_json<UserId>(parse(R"({"type":"OrderId","value":1})"));
} catch (const JsonTypeError& e) {
    // "Type mismatch: expected UserId, got OrderId"
}
```

---

## Why Not Alternatives?

| If You Need... | Why Not Manual Overloads | Why Not Reflection | Why Not nlohmann | Fat-P Advantage |
|----------------|-------------------------|-------------------|-----------------|-----------------|
| Expected support | ✅ Works but verbose | Future C++ | ❌ No Expected | ✅ Built-in |
| StrongId type safety | ✅ Works but verbose | Future C++ | ❌ No StrongId | ✅ Built-in |
| EnumPlus strings | ✅ Works but verbose | Future C++ | ❌ Different API | ✅ Built-in |
| SmallVector | ✅ Works but verbose | Future C++ | ❌ Different type | ✅ Built-in |

**The Sweet Spot:** FatPJsonLite provides seamless JSON serialization for the entire fat_p type ecosystem.

---

## The "Forever Stuck" Reality

**Integration Reality:** JsonLite must remain standalone (zero dependencies) for use outside fat_p. FatPJsonLite exists specifically to bridge JsonLite with fat_p types.

This separation is permanent—JsonLite serves users who want JSON without fat_p, while FatPJsonLite serves users who want both.

---

## Performance Characteristics

| Type | Serialization | Deserialization | Notes |
|------|---------------|-----------------|-------|
| `Expected<T, E>` | +5-10 ns | +10-20 ns | Object wrapper overhead |
| `SmallVector<T, N>` | Same as vector | Same as vector | Identical format |
| `StrongId<Tag, T>` | +10-15 ns | +15-25 ns | Type field + validation |
| `EnumPlus` enum | +5-10 ns | O(n) string lookup | Via EnumStringPolicy |

### Where Fat-P Wins
- Full fat_p ecosystem serialization
- Type-safe StrongId round-trips
- Expected error/value discrimination

### Where Fat-P Loses (Honesty Builds Trust)
- Non-fat_p projects → use standalone JsonLite
- Performance-critical JSON → simdjson + manual serialization
- Different JSON schema needs → may need custom overloads anyway

---

## Integration Points

```
FatPJsonLite.h
    ↓ uses
JsonLite.h           (Base JSON functionality)
Expected.h           (Expected<T, E> support)
SmallVector.h        (SmallVector<T, N> support)
StrongId.h           (StrongId<Tag, T> support)
EnumPlus.h           (Enum string serialization)
    ↓ used by
FeatureManager.h     (Full config serialization)
DiagnosticLogger.h   (Structured logging with fat_p types)
Any fat_p application needing JSON
```

---

## Final Assessment

FatPJsonLite delivers on the fat_p promise through three pillars:

### 1. Permanence
The JsonLite/FatPJsonLite split is architectural—JsonLite stays dependency-free, FatPJsonLite provides fat_p integration. Both will exist permanently.

### 2. Specialization
Native support for fat_p's unique types: `Expected` with error/value discrimination, `StrongId` with type preservation, `SmallVector` with vector-compatible format, `EnumPlus` with string conversion.

### 3. Control
Include `JsonLite.h` for standalone JSON. Include `FatPJsonLite.h` for full fat_p integration. Choose based on your dependency needs.

**Architectural Verdict:** FatPJsonLite transforms fat_p type serialization from **manual overload boilerplate** to **include-and-use simplicity**. The entire fat_p type ecosystem becomes JSON-serializable with one header.

---

*FatPJsonLite.h — Fat-P Library*
