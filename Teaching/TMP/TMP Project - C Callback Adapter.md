# TMP Project - C Callback Adapter

## The Problem This Solves

Every C codebase uses the same callback pattern: a function pointer plus a `void*` context. The C API registers your function, stashes your context pointer, and later calls your function with that pointer cast back:

```c
// The C API you cannot modify
typedef void (*event_handler_fn)(void* ctx, int event_id, const char* payload);
void register_handler(event_handler_fn fn, void* ctx);
```

When porting to C++, you want to pass a lambda:

```cpp
auto handler = [&logger](int event_id, const char* payload) {
    logger.log(event_id, payload);
};
// How do I pass this lambda to register_handler?
```

The gap: `register_handler` wants a raw function pointer (no captures allowed) and a `void*`. A capturing lambda is neither. You need a bridge that stores the lambda, presents a C-compatible function pointer as a thunk, and routes the callback back to the lambda with the `void*` stripped out. This project builds that bridge using the TMP patterns from the ArrayView developer manual.

## What You Will Build

A `CallbackAdapter<CSignature>` that:

1. Accepts any C++ callable (lambda, functor, `std::function`, bound method)
2. Stores it in a type-erased wrapper
3. Exposes a C-compatible `(function_pointer, void*)` pair
4. Routes callbacks from C back to C++ with the `void*` stripped and the real arguments forwarded

Zero modifications to the C API. Zero heap allocation for small callables. Fully type-safe on the C++ side.

## Prerequisites

- Completed the abstract TMP exercises or the Tuple project
- Familiar with function pointers, `void*` casting, and `static_cast`
- Understand why capturing lambdas cannot convert to function pointers

---

## Phase 1: FunctionTraits — Decomposing a C Signature

### The Problem

Before we can build the adapter, we need to take apart a C function pointer type like `void(*)(void*, int, const char*)` and extract its pieces: the return type, the parameter types, and the arity.

### What Is the Compiler Figuring Out?

**Types** (return type, each parameter type) and a **value** (parameter count). This is multiple answers from one input, so we define a struct with several members.

### Pattern

Partial specialization on the function pointer type. This is not recursive — it is a single pattern match that decomposes the function pointer in one step, using a parameter pack to capture all argument types. It parallels the CTAD guides from ArrayView: one pattern match that extracts all the information at once.

### Why Partial Specialization Works Here

A function pointer type `R(*)(A, B, C)` has structure that the compiler can pattern-match: `R` is the return type, `A, B, C` is a parameter pack. A partial specialization of the form `FunctionTraits<R(*)(Args...)>` decomposes the type in a single step with no recursion needed — unlike array types which must be peeled one extent at a time.

### Solution

```cpp
#pragma once

#include <cstddef>
#include <tuple>
#include <type_traits>

namespace bridge {

// Primary template — not defined. Forces users to pass a function pointer type.
template<typename T>
struct FunctionTraits;

// Specialization for function pointers: R(*)(Args...)
template<typename R, typename... Args>
struct FunctionTraits<R(*)(Args...)>
{
    using return_type = R;
    using args_tuple  = std::tuple<Args...>;

    static constexpr std::size_t arity = sizeof...(Args);

    // Extract the I-th parameter type
    template<std::size_t I>
    using arg_type = std::tuple_element_t<I, args_tuple>;
};

} // namespace bridge

// Tests
using Handler = void(*)(void*, int, const char*);
using Traits  = bridge::FunctionTraits<Handler>;

static_assert(std::is_same_v<Traits::return_type, void>);
static_assert(Traits::arity == 3);
static_assert(std::is_same_v<Traits::arg_type<0>, void*>);
static_assert(std::is_same_v<Traits::arg_type<1>, int>);
static_assert(std::is_same_v<Traits::arg_type<2>, const char*>);
```

### What to Notice

No recursion. The function pointer type has enough structure that one partial specialization decomposes it completely. The parameter pack `Args...` captures all parameters in one shot. This is the TMP equivalent of a regex with capture groups — one match, all pieces extracted.

We reuse `std::tuple_element_t` for `arg_type<I>` rather than writing our own. The lesson: use existing standard machinery when it fits. We built `tuple_element` from scratch in the Tuple project; here we consume the standard version.

---

## Phase 2: StripContext — Removing void* From the Signature

### The Problem

The C callback signature is `void(void*, int, const char*)`. The C++ callable's signature should be `void(int, const char*)` — the `void*` context parameter is an implementation detail that the C++ side should never see. We need a trait that removes the first parameter (the `void*`) from the parameter list and produces the "clean" C++ signature.

### What Is the Compiler Figuring Out?

A **type** — a function signature with the context parameter removed.

### Pattern

Pack manipulation. We decompose the C function pointer into `R(*)(void*, Rest...)` using partial specialization, then reconstruct `R(*)(Rest...)` by discarding the first parameter. This is a single-step transformation, not recursive — the partial specialization peels `void*` off the front and `Rest...` captures everything else.

### Why This Is Not Recursive

Unlike array extents (which must be peeled one at a time because each `[N]` is a separate syntactic layer), function parameters are a flat pack. The compiler can match `R(*)(First, Rest...)` in one step, binding `First = void*` and `Rest = int, const char*`. No recursion needed.

### Solution

```cpp
namespace bridge {

// Primary template — not defined
template<typename FnPtr>
struct StripContext;

// Match: R(*)(void*, Rest...) → produce R(*)(Rest...)
template<typename R, typename... Rest>
struct StripContext<R(*)(void*, Rest...)>
{
    using c_signature    = R(*)(void*, Rest...);    // Original C signature
    using clean_signature = R(*)(Rest...);           // C++ signature without void*
    using return_type    = R;
    using clean_args     = std::tuple<Rest...>;      // Parameter types sans void*

    static constexpr std::size_t clean_arity = sizeof...(Rest);
};

} // namespace bridge

// Tests
using Handler = void(*)(void*, int, const char*);
using Stripped = bridge::StripContext<Handler>;

static_assert(std::is_same_v<Stripped::c_signature, void(*)(void*, int, const char*)>);
static_assert(std::is_same_v<Stripped::clean_signature, void(*)(int, const char*)>);
static_assert(std::is_same_v<Stripped::return_type, void>);
static_assert(Stripped::clean_arity == 2);
static_assert(std::is_same_v<std::tuple_element_t<0, Stripped::clean_args>, int>);
static_assert(std::is_same_v<std::tuple_element_t<1, Stripped::clean_args>, const char*>);
```

### What to Notice

The specialization `R(*)(void*, Rest...)` is a material implication (the same model from the CTAD section): **if** the function pointer has `void*` as its first parameter, **then** decompose it as shown. If the first parameter is not `void*`, the specialization does not match and the primary template (undefined) causes a compile error — which is exactly what we want, because a callback without a `void*` context parameter does not follow the C convention and should not be adapted.

---

## Phase 3: The Thunk — Generating a C-Compatible Bridge Function

### The Problem

We need a static function with the C signature `void(void*, int, const char*)` that the C API can call. This function must cast the `void*` back to our callable type and forward the remaining arguments. The function must be a real function pointer (not a lambda with captures), and it must be generated at compile time for each callable type.

### What Is the Compiler Figuring Out?

A **function** — specifically, a static member function template whose signature matches the C callback type. The compiler instantiates this function for each `Callable` type, producing a unique function pointer per callable type.

### Pattern

This is not a `::type` or `::value` computation. It is a **function template instantiation** — the compiler generates a concrete function for each `Callable` type. The parameter forwarding uses `std::index_sequence` expansion, the same pattern as `tuple_cat` and `ExtentStorage::apply`.

### Why a Static Function Template?

A capturing lambda cannot convert to a function pointer. A non-capturing lambda can, but it cannot access the stored callable. A static member function template of a class that knows the `Callable` type can do both: it has no captures (it is a regular function), and it knows the `Callable` type as a template parameter (so it can cast `void*` back to `Callable*`).

### Solution

```cpp
namespace bridge {

template<typename CSignature, typename Callable>
class CallbackAdapter;

template<typename R, typename... Rest, typename Callable>
class CallbackAdapter<R(*)(void*, Rest...), Callable>
{
public:
    using c_signature = R(*)(void*, Rest...);

    explicit CallbackAdapter(Callable callable)
        : callable_(std::move(callable))
    {}

    // The C function pointer — pass this to the C API
    static c_signature c_function() noexcept
    {
        return &thunk;
    }

    // The void* context — pass this to the C API alongside c_function()
    void* context() noexcept
    {
        return static_cast<void*>(&callable_);
    }

private:
    Callable callable_;

    // The thunk: C calls this, we forward to the C++ callable
    static R thunk(void* ctx, Rest... args)
    {
        auto* self = static_cast<Callable*>(ctx);
        return (*self)(args...);
    }
};

} // namespace bridge
```

### Paper Trace

For `CallbackAdapter<void(*)(void*, int, const char*), MyLambda>`:

1. The partial specialization matches with `R = void`, `Rest = int, const char*`, `Callable = MyLambda`
2. `c_function()` returns `&thunk` — a pointer to `static void thunk(void*, int, const char*)`
3. `context()` returns the address of the stored `MyLambda` cast to `void*`
4. When the C API calls `thunk(ctx, 42, "hello")`:
   - `ctx` is cast back to `MyLambda*`
   - `(*self)(42, "hello")` calls the original lambda

### Usage

```cpp
#include <cstdio>

// The C API (cannot be modified)
typedef void (*event_handler_fn)(void* ctx, int event_id, const char* payload);
void register_handler(event_handler_fn fn, void* ctx);

// C++ usage
void setup()
{
    int call_count = 0;

    auto handler = [&call_count](int event_id, const char* payload) {
        ++call_count;
        std::printf("Event %d: %s (call #%d)\n", event_id, payload, call_count);
    };

    using Adapter = bridge::CallbackAdapter<event_handler_fn, decltype(handler)>;
    Adapter adapter(handler);

    register_handler(adapter.c_function(), adapter.context());

    // CRITICAL: adapter must outlive the registration.
    // The void* points into adapter's storage.
}
```

### What to Notice

`thunk` is a static function — it has no `this` pointer and no captures. It is a plain function that the C API can store as a function pointer. The `Callable` type is a template parameter of the enclosing class, so the thunk knows what to cast `void*` back to without any runtime type information.

The `Rest...` parameter pack does the forwarding work. For `void(*)(void*, int, const char*)`, `Rest` is `int, const char*`. The thunk's signature becomes `static void thunk(void* ctx, int, const char*)` — which matches the C signature exactly. The forwarding `(*self)(args...)` passes the `int` and `const char*` to the callable, stripping the `void*`.

---

## Phase 4: Convenience — make_callback

### The Problem

The usage from Phase 3 requires the user to spell out the adapter type with `decltype`. A factory function can deduce the callable type automatically.

### Solution

```cpp
namespace bridge {

template<typename CSignature, typename Callable>
auto make_callback(Callable&& callable)
{
    using Adapter = CallbackAdapter<CSignature, std::decay_t<Callable>>;
    return Adapter(std::forward<Callable>(callable));
}

} // namespace bridge

// Usage becomes:
void setup()
{
    int call_count = 0;

    auto adapter = bridge::make_callback<event_handler_fn>(
        [&call_count](int event_id, const char* payload) {
            ++call_count;
            std::printf("Event %d: %s\n", event_id, payload);
        }
    );

    register_handler(adapter.c_function(), adapter.context());
}
```

### What to Notice

`CSignature` is specified explicitly (the user must name the C callback type — there is no way to deduce it). `Callable` is deduced from the argument. `std::decay_t` strips references and cv-qualifiers so the adapter stores a value, not a reference to a temporary.

---

## Phase 5: Static Adapter — Zero Storage for Stateless Callables

### The Problem

For stateless callables (non-capturing lambdas, plain function pointers), the `CallbackAdapter` from Phase 3 stores a callable object and passes its address as the `void*` context. But stateless lambdas carry no state — the `void*` is wasted. Worse, some C APIs pass `NULL` as the context (or the context is used for something else, not the callback). We need a specialization that works without a stored callable.

### What Is the Compiler Figuring Out?

A **compile-time decision**: is the callable stateless? If so, use a simpler adapter.

### Pattern

`if constexpr` or constrained specialization based on `std::is_empty_v<Callable>`. A stateless callable (like a non-capturing lambda) has `sizeof(Callable) == 0` (modulo C++ empty-object rules) and `std::is_empty_v<Callable> == true`.

### Solution

```cpp
namespace bridge {

// Stateless specialization: the callable is reconstructed from nothing
template<typename CSignature, typename Callable>
class StaticCallbackAdapter;

template<typename R, typename... Rest, typename Callable>
class StaticCallbackAdapter<R(*)(void*, Rest...), Callable>
{
    static_assert(std::is_empty_v<Callable>,
        "StaticCallbackAdapter requires a stateless callable (non-capturing lambda)");

public:
    using c_signature = R(*)(void*, Rest...);

    // No stored callable — the type IS the callable
    static c_signature c_function() noexcept
    {
        return &thunk;
    }

    // Context can be nullptr — we don't need it
    static void* context() noexcept
    {
        return nullptr;
    }

private:
    static R thunk(void* /*ctx*/, Rest... args)
    {
        // Default-construct the callable — it's stateless, so this is free
        Callable callable{};
        return callable(args...);
    }
};

// Factory that picks the right adapter
template<typename CSignature, typename Callable>
auto make_callback(Callable&& callable)
{
    using Decayed = std::decay_t<Callable>;
    if constexpr (std::is_empty_v<Decayed>)
    {
        // Stateless: don't even store it
        (void)callable;
        return StaticCallbackAdapter<CSignature, Decayed>{};
    }
    else
    {
        return CallbackAdapter<CSignature, Decayed>(std::forward<Callable>(callable));
    }
}

} // namespace bridge
```

### What to Notice

`std::is_empty_v<Callable>` is a value computed at compile time. The `if constexpr` in `make_callback` selects the adapter type — the non-taken branch is not instantiated. This is the same `if constexpr` pattern as `ExtentStorage::get<I>()`, which selects between a compile-time constant and a runtime lookup based on whether the dimension is static.

The `thunk` in the stateless adapter default-constructs the callable inside the function body. This looks wasteful, but for an empty type the constructor is a no-op — the compiler generates zero instructions for it. The callable exists only as a type; its value carries no information.

---

## What You Built

A zero-overhead C callback bridge that:

- Decomposes C function pointer types at compile time (Phase 1 — partial specialization)
- Strips `void*` context from the parameter list (Phase 2 — pack manipulation)
- Generates a C-compatible thunk function for each callable type (Phase 3 — static function template)
- Provides a convenient factory (Phase 4 — type deduction)
- Optimizes stateless callables to zero storage (Phase 5 — compile-time decision)

| Phase | What It Does | TMP Pattern | ArrayView Parallel |
|-------|-------------|-------------|-------------------|
| 1. FunctionTraits | Decompose function pointer type | Single partial specialization | CTAD guide pattern-matching `T(&)[A][B]` |
| 2. StripContext | Remove `void*` from parameter list | Pack decomposition `(void*, Rest...)` | Peeler stripping one `[N]` from array type |
| 3. Thunk | Generate C-compatible bridge function | Static function template + pack forwarding | `ExtentStorage::apply` forwarding dynamic extents |
| 4. make_callback | Deduce callable type | Template argument deduction | `makeArrayView` deducing array type |
| 5. Static adapter | Compile-time callable classification | `if constexpr` on `is_empty_v` | `ExtentStorage::get<I>()` choosing static vs dynamic |

This adapter solves a real problem that appears in every C-to-C++ migration: wrapping C callback registrations so that C++ lambdas, functors, and bound methods can participate without modifying the C API.

---

*TMP Project — C Callback Adapter for C-to-C++ Migration*
