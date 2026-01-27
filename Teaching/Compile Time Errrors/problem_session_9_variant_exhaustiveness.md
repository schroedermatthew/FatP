# Problem-Solving Session 9: The Unhandled Alternative

## Exhaustive Variant Visitation with the Overloaded Pattern

**Estimated time:** 45–60 minutes  
**Prerequisites:** std::variant basics, lambdas, parameter packs  
**Fat-P components:** None (standard library technique)

---

## Guarantee Legend

| Mark | Meaning |
|------|---------|
| ✅ **Compile-time** | Missing variant handler causes compile error |

---

## The Bug

Your configuration system supports multiple value types:

```cpp
using ConfigValue = std::variant<int, double, std::string, bool>;

std::string to_display_string(const ConfigValue& value) {
    return std::visit([](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        }
        // Forgot bool!
        return std::string{"<unknown>"};
    }, value);
}
```

Six months later, someone adds a new type:

```cpp
using ConfigValue = std::variant<int, double, std::string, bool, std::vector<int>>;
```

The code still compiles. For `bool` and `std::vector<int>`, it silently returns `"<unknown>"`. Users see wrong values. Nobody notices until a customer complains.

**The problem:** The generic lambda `[](auto&& arg)` accepts any type. There's no exhaustiveness check because every type has a valid code path (the fallback return).

---

## Questions to Consider

Before reading further, think about:

1. **Q1:** Why doesn't the compiler warn about missing type handlers?
2. **Q2:** How does the "overloaded" pattern enable exhaustiveness?
3. **Q3:** How do you handle default/catch-all cases safely?
4. **Q4:** How does this compare to enum switches?
5. **Q5:** When should you use variant vs enum?

---

## Q1: Why No Compiler Warning?

When you use a generic lambda with `std::visit`, the lambda is instantiated for each variant alternative. If all instantiations compile, there's no error:

```cpp
std::visit([](auto&& arg) {
    // This lambda can be instantiated with ANY type
    // The compiler generates:
    //   - operator()(int&&)
    //   - operator()(double&&)
    //   - operator()(std::string&&)
    //   - operator()(bool&&)
    // All compile successfully, even if logic is wrong
}, value);
```

The `if constexpr` chain handles some types explicitly, but the implicit fallback handles the rest—silently doing the wrong thing.

**Contrast with enums:** With `-Werror=switch-enum`, a missing case is an error. But variant visitation has no equivalent built-in check.

---

## Q2: The Overloaded Pattern

The solution is to make each type require an explicit handler. The "overloaded" pattern creates a callable that inherits from multiple lambdas:

```cpp
// The overloaded template
template<class... Ts>
struct overloaded : Ts... { 
    using Ts::operator()...; 
};

// Deduction guide (C++17)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;
```

Now use it:

```cpp
std::string to_display_string(const ConfigValue& value) {
    return std::visit(overloaded{
        [](int i) { return std::to_string(i); },
        [](double d) { return std::to_string(d); },
        [](const std::string& s) { return s; },
        [](bool b) { return b ? "true" : "false"; }
    }, value);
}
```

If someone adds `std::vector<int>` to the variant:

```cpp
using ConfigValue = std::variant<int, double, std::string, bool, std::vector<int>>;

// Now to_display_string fails to compile:
// error: no matching function for call to 'overloaded<...>::operator()(std::vector<int>&)'
```

The compiler forces you to handle the new type.

---

## How Overloaded Works

The `overloaded` struct inherits from each lambda and brings all their `operator()` into scope:

```cpp
overloaded{
    [](int i) { return std::to_string(i); },
    [](double d) { return std::to_string(d); }
}

// Effectively creates:
struct __anonymous {
    // Inherits from both lambdas
    // Has:
    //   std::string operator()(int i)    { return std::to_string(i); }
    //   std::string operator()(double d) { return std::to_string(d); }
};
```

When `std::visit` calls `operator()` with a `bool`, no overload matches → compile error.

```mermaid
flowchart TD
    A[std::visit calls operator()] --> B{Which overload?}
    B -->|int| C["[](int i)"]
    B -->|double| D["[](double d)"]
    B -->|string| E["[](const string& s)"]
    B -->|bool| F["??? No match → compile error"]
    
    style F fill:#FFB6C1
```

---

## Q3: Handling Default Cases

Sometimes you want a catch-all for some types. Add a generic lambda last:

```cpp
std::string describe(const ConfigValue& value) {
    return std::visit(overloaded{
        [](int i) { return "integer: " + std::to_string(i); },
        [](double d) { return "floating: " + std::to_string(d); },
        [](const auto& other) { return "other type"; }  // Catch-all
    }, value);
}
```

The generic lambda matches anything not explicitly handled. But now you've lost exhaustiveness checking—adding a new type won't cause a compile error.

**Compromise: Use static_assert in catch-all:**

```cpp
std::string describe(const ConfigValue& value) {
    return std::visit(overloaded{
        [](int i) { return "integer: " + std::to_string(i); },
        [](double d) { return "floating: " + std::to_string(d); },
        [](const std::string& s) { return "string: " + s; },
        [](bool b) { return std::string(b ? "true" : "false"); },
        [](const auto& other) {
            // This will fail to compile if reached
            static_assert(sizeof(decltype(other)) == 0, 
                         "Unhandled type in visitor");
            return std::string{};
        }
    }, value);
}
```

Now adding a new type causes a compile error in the static_assert.

---

## Q4: Comparison with Enums

| Aspect | Enum + switch | Variant + visit |
|--------|---------------|-----------------|
| Exhaustiveness | `-Werror=switch-enum` | overloaded pattern |
| Per-value data | ❌ No | ✅ Yes |
| Type safety | Moderate | Strong |
| Adding values | Update switches | Update visitors |
| Pattern | Known at compile time | Known at compile time |
| Memory | sizeof(enum) | sizeof(largest alternative) + discriminant |

**When to use enum:**
- Simple state with no associated data
- C interop needed
- Memory-constrained

**When to use variant:**
- Different types need different data
- Type-safe operations on each alternative
- Replacing inheritance hierarchies

---

## Q5: Real-World Example: Expression Trees

```cpp
// Forward declarations
struct Literal;
struct BinaryOp;
struct UnaryOp;
struct Variable;

// Expression is one of these types
using Expr = std::variant<Literal, BinaryOp, UnaryOp, Variable>;

struct Literal {
    double value;
};

struct Variable {
    std::string name;
};

struct BinaryOp {
    char op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct UnaryOp {
    char op;
    std::unique_ptr<Expr> operand;
};

// Evaluate expression
double evaluate(const Expr& expr, const std::map<std::string, double>& vars) {
    return std::visit(overloaded{
        [](const Literal& lit) { 
            return lit.value; 
        },
        [&vars](const Variable& var) { 
            return vars.at(var.name); 
        },
        [&vars](const BinaryOp& bin) {
            double l = evaluate(*bin.left, vars);
            double r = evaluate(*bin.right, vars);
            switch (bin.op) {
                case '+': return l + r;
                case '-': return l - r;
                case '*': return l * r;
                case '/': return l / r;
                default: throw std::invalid_argument("Unknown operator");
            }
        },
        [&vars](const UnaryOp& un) {
            double v = evaluate(*un.operand, vars);
            switch (un.op) {
                case '-': return -v;
                case '+': return v;
                default: throw std::invalid_argument("Unknown operator");
            }
        }
    }, expr);
}

// Pretty-print expression
std::string to_string(const Expr& expr) {
    return std::visit(overloaded{
        [](const Literal& lit) { 
            return std::to_string(lit.value); 
        },
        [](const Variable& var) { 
            return var.name; 
        },
        [](const BinaryOp& bin) {
            return "(" + to_string(*bin.left) + " " + bin.op + " " + to_string(*bin.right) + ")";
        },
        [](const UnaryOp& un) {
            return std::string(1, un.op) + to_string(*un.operand);
        }
    }, expr);
}
```

Adding a new expression type (e.g., `FunctionCall`) requires updating all visitors—the compiler enforces this.

---

## Complete Example: JSON-like Value

```cpp
#include <variant>
#include <string>
#include <vector>
#include <map>
#include <memory>

// Forward declaration for recursion
struct JsonValue;

using JsonNull = std::monostate;
using JsonBool = bool;
using JsonNumber = double;
using JsonString = std::string;
using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

struct JsonValue : std::variant<JsonNull, JsonBool, JsonNumber, JsonString, 
                                std::unique_ptr<JsonArray>, 
                                std::unique_ptr<JsonObject>> {
    using variant::variant;
};

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

std::string to_json(const JsonValue& value) {
    return std::visit(overloaded{
        [](JsonNull) { return std::string("null"); },
        [](JsonBool b) { return b ? std::string("true") : std::string("false"); },
        [](JsonNumber n) { return std::to_string(n); },
        [](const JsonString& s) { return "\"" + s + "\""; },
        [](const std::unique_ptr<JsonArray>& arr) {
            std::string result = "[";
            for (size_t i = 0; i < arr->size(); ++i) {
                if (i > 0) result += ", ";
                result += to_json((*arr)[i]);
            }
            return result + "]";
        },
        [](const std::unique_ptr<JsonObject>& obj) {
            std::string result = "{";
            bool first = true;
            for (const auto& [key, val] : *obj) {
                if (!first) result += ", ";
                first = false;
                result += "\"" + key + "\": " + to_json(val);
            }
            return result + "}";
        }
    }, static_cast<const JsonValue::variant&>(value));
}
```

---

## Migration: if-else Chains to Overloaded

### Before: Fragile if-else

```cpp
std::string process(const std::variant<int, double, std::string>& v) {
    if (auto* i = std::get_if<int>(&v)) {
        return std::to_string(*i);
    } else if (auto* d = std::get_if<double>(&v)) {
        return std::to_string(*d);
    } else if (auto* s = std::get_if<std::string>(&v)) {
        return *s;
    }
    return "";  // What if a new type is added?
}
```

### After: Exhaustive overloaded

```cpp
std::string process(const std::variant<int, double, std::string>& v) {
    return std::visit(overloaded{
        [](int i) { return std::to_string(i); },
        [](double d) { return std::to_string(d); },
        [](const std::string& s) { return s; }
    }, v);
}
```

---

## Summary

| Problem | Solution |
|---------|----------|
| Generic lambda hides missing cases | Use overloaded{} with specific lambdas |
| Adding variant type silently breaks code | Overloaded gives compile error |
| Need catch-all but want safety | Use static_assert in generic lambda |
| Want enum exhaustiveness for variants | Overloaded pattern provides it |

### Key Principles

1. **Use overloaded{} instead of generic lambdas** — each type gets explicit handler

2. **Adding types breaks compilation** — forces update to all visitors

3. **Catch-all should be explicit** — either handle generically or static_assert

4. **Variant > inheritance for closed sets** — compiler enforces exhaustiveness

### The Guideline in One Sentence

> Use the overloaded pattern to get compile-time exhaustiveness checking for std::variant.

---

## Exercises

1. **Basic:** Create a `Shape` variant with `Circle`, `Rectangle`, `Triangle`. Implement `area()` and `perimeter()` using overloaded visitors.

2. **Exhaustiveness test:** Add a `Polygon` type to your variant. Verify the compiler catches the missing handlers.

3. **Expression evaluator:** Extend the expression tree example to support `FunctionCall` with named functions like `sin`, `cos`, `sqrt`.

4. **JSON type checker:** Write a function that returns the type name of a JsonValue ("null", "boolean", "number", "string", "array", "object").

---

## Further Reading

- [std::visit](https://en.cppreference.com/w/cpp/utility/variant/visit)
- [std::variant](https://en.cppreference.com/w/cpp/utility/variant)
- Session 2: Enum Exhaustiveness — similar pattern for enums
- Handbook: Phantom Types — related type-level patterns
