# PipeOperator: A Fat-P Library Showcase

## Executive Summary

PipeOperator provides **Unix-style function composition** via the `|` operator with automatic Expected integration. Unlike method chaining (requires builder pattern), nested function calls (inside-out reading), or manual Expected handling (verbose conditionals), PipeOperator enables `value | transform | validate | process` syntax where **errors propagate automatically** through the chain. The operator overloads resolve at compile time with zero runtime overhead, generating code identical to hand-written conditionals.

---

## The Problem Domain

### What Goes Wrong Without It

```cpp
// The nested call nightmare (reads inside-out)
auto result = format(validate(parse(normalize(input))));
// What does this do? Read from innermost to outermost.
// Error handling? Hidden in each function.

// The verbose Expected handling
auto parsed = parse(input);
if (!parsed) return parsed.error();

auto validated = validate(*parsed);
if (!validated) return validated.error();

auto formatted = format(*validated);
if (!formatted) return formatted.error();

return *formatted;
// 10 lines for 4 operations. Repeated pattern. Easy to forget a check.
```

| Issue | HPC Impact |
|-------|------------|
| Inside-out reading | Hard to follow data flow |
| Manual error propagation | Verbose, repetitive, error-prone |
| No composition syntax | Can't build pipelines declaratively |
| Exception-based alternatives | Hidden control flow, stack unwinding cost |

### The Standard's Limitation

C++23 adds `std::expected` with monadic operations (`and_then`, `transform`), but:
- Method chaining requires the result type to have the methods
- No operator syntax for function composition
- Can't compose free functions elegantly

The pipe operator (`|`) is used by C++20 ranges but not generalized for function composition.

---

## Architecture: Operator Overloading with Expected Integration

### The Mechanism: Three Pipe Variants

```cpp
// 1. Raw value | function → apply function
template<typename T, typename F>
auto operator|(T&& value, F&& func) -> std::invoke_result_t<F, T> {
    return std::invoke(std::forward<F>(func), std::forward<T>(value));
}

// 2. Expected<T, E> | function (T→U) → Expected<U, E> (map)
template<typename T, typename E, typename F>
auto operator|(Expected<T, E>&& exp, F&& func) 
    -> Expected<std::invoke_result_t<F, T>, E> 
{
    if (exp) {
        return std::invoke(std::forward<F>(func), *std::forward<Expected<T,E>>(exp));
    }
    return {unexpect, std::move(exp).error()};
}

// 3. Expected<T, E> | function (T→Expected<U, E>) → Expected<U, E> (bind/flatMap)
template<typename T, typename E, typename F>
auto operator|(Expected<T, E>&& exp, F&& func)
    -> std::invoke_result_t<F, T>  // Already Expected<U, E>
{
    if (exp) {
        return std::invoke(std::forward<F>(func), *std::forward<Expected<T,E>>(exp));
    }
    return {unexpect, std::move(exp).error()};
}
```

**Key insight:** The return type of `F` determines which overload applies:
- `F` returns `U` → wrap in `Expected<U, E>` (map semantics)
- `F` returns `Expected<U, E>` → return as-is (bind/flatMap semantics)

### Generated Code

```cpp
// This pipeline:
auto result = input 
    | parse 
    | validate 
    | format;

// Compiles to essentially:
auto parsed = parse(input);
if (!parsed) return Expected<string, Error>{unexpect, parsed.error()};
auto validated = validate(*parsed);
if (!validated) return Expected<string, Error>{unexpect, validated.error()};
auto formatted = format(*validated);
if (!formatted) return Expected<string, Error>{unexpect, formatted.error()};
return formatted;
```

Zero overhead. The pipe operators inline completely.

---

## Feature Inventory

### 1. Left-to-Right Reading

```cpp
// Before: inside-out
auto result = step3(step2(step1(input)));

// After: left-to-right
auto result = input | step1 | step2 | step3;
```

Data flows left-to-right, matching how you describe the process.

### 2. Automatic Expected Propagation

```cpp
Expected<int, Error> parse(std::string_view s);
Expected<int, Error> validate(int value);
int double_value(int v) { return v * 2; }

auto result = std::string("42") 
    | parse           // Expected<int, Error>
    | validate        // Expected<int, Error> - error propagates if parse failed
    | double_value;   // Expected<int, Error> - wrapped automatically
    
// If any step fails, result contains that error
// No manual if (!x) return x.error() boilerplate
```

### 3. Mixed Function Types

```cpp
// Free functions, lambdas, member functions, functors—all work
auto result = input
    | parse                              // Free function
    | [](int x) { return x * 2; }        // Lambda
    | std::mem_fn(&Value::validate)      // Member function
    | Formatter{};                        // Functor with operator()
```

### 4. Expected<void, E> Support

```cpp
Expected<void, Error> validate(const Input& input);
Expected<Output, Error> process(const Input& input);

auto result = input
    | validate      // Expected<void, Error>
    | [&input]() { return process(input); };  // Continue after void
```

### 5. Error Transformation

```cpp
auto result = input
    | parse
    | validate
    | [](Expected<int, ParseError>&& exp) {
        return exp.transform_error([](ParseError e) {
            return ApplicationError{e};  // Convert error type
        });
    }
    | next_step;
```

### 6. Pipe with Arguments (Currying)

```cpp
// Partially applied function
auto multiply_by = [](int factor) {
    return [factor](int x) { return x * factor; };
};

auto result = 5 
    | multiply_by(3)   // 15
    | multiply_by(2);  // 30
```

---

## Why Not Alternatives?

| If You Need... | Why Not Method Chaining | Why Not and_then/transform | Why Not Nested Calls | Fat-P Advantage |
|----------------|------------------------|---------------------------|---------------------|-----------------|
| Free function composition | ❌ Requires methods | ❌ Requires methods | ✅ Works but ugly | ✅ Clean syntax |
| Left-to-right reading | ✅ Works | ✅ Works | ❌ Inside-out | ✅ Natural flow |
| Mixed function types | ❌ Fixed method | ❌ Fixed method | ✅ Works | ✅ Any callable |
| Zero overhead | ✅ Inlines | ✅ Inlines | ✅ Direct calls | ✅ Inlines |
| Error propagation | Manual | ✅ Automatic | Manual | ✅ Automatic |

**The Sweet Spot:** PipeOperator is the only option combining free function composition, left-to-right syntax, automatic Expected propagation, and support for mixed callable types.

---

## The "Forever Stuck" Reality

**Standard Reality:** C++20 uses `|` for ranges but not general function composition:
- Ranges pipeline is domain-specific (views)
- General `|` overloading is controversial
- Composition operators like `>>` aren't standard

Fat_p's pipe operator provides Haskell/F#-style composition for C++, integrated with Expected for error handling.

---

## Performance Characteristics

| Scenario | Cost | Notes |
|----------|------|-------|
| `value | func` | 0 overhead | Direct function call |
| `Expected | func (success)` | ~1 ns | Branch + call |
| `Expected | func (failure)` | ~1 ns | Branch + error copy |
| 5-stage pipeline | ~5 ns | 5 branches (well-predicted) |

### Compiler Optimization

```cpp
auto result = x | f1 | f2 | f3;

// Optimizes to:
if (auto r1 = f1(x); r1) {
    if (auto r2 = f2(*r1); r2) {
        return f3(*r2);
    } else return r2;
} else return r1;
```

Branch prediction works well because success is the common case.

### Where Fat-P Wins
- Data transformation pipelines
- Parsing and validation chains
- Functional programming style in C++

### Where Fat-P Loses (Honesty Builds Trust)
- Debugging → pipeline steps harder to breakpoint
- Complex branching → explicit conditionals clearer
- Team unfamiliar with FP → may find syntax confusing

---

## Integration Points

```
PipeOperator.h
    ↓ uses
Expected.h       (Expected type for error propagation)
TypeTraits.h     (SFINAE helpers for overload selection)
    ↓ used by
Data processing pipelines
Configuration parsing
Validation chains
```

---

## Final Assessment

PipeOperator delivers on the fat_p promise through three pillars:

### 1. Permanence
C++ will not generalize `|` beyond ranges—too controversial, too many overload conflicts. PipeOperator provides function composition permanently in the fat_p namespace.

### 2. Specialization
Expected-aware overloads automatically propagate errors through pipelines. Map vs. bind semantics are selected automatically based on function return type. No manual error checking at each step.

### 3. Control
You choose which functions enter the pipeline. Error transformation is explicit when needed. Pipeline construction is declarative—data flow is visible in the code structure.

**Architectural Verdict:** PipeOperator transforms function composition from **nested calls** or **verbose Expected handling** to **declarative left-to-right pipelines**. It's `|` for functions, with error propagation built in.

---

*PipeOperator.h (427 lines) — Fat-P Library*
