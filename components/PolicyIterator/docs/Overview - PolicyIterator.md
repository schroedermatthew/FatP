---
doc_id: OV-POLICYITERATOR-001
doc_type: "Overview"
title: "PolicyIterator"
fatp_components: ["PolicyIterator"]
topics: ["policy-based design", "iterator abstraction", "compile-time dispatch", "traversal patterns"]
constraints: ["virtual dispatch overhead", "iterator boilerplate", "STL algorithm compatibility", "debug-mode bounds checking"]
cxx_standard: "C++20"
last_verified: "2025-12-30"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Overview - PolicyIterator

*Fat-P Library — December 2025 — Benchmarks: AMD Ryzen 9 5900X, GCC 12.2, -O3*

---

## Overview Card

**Component:** PolicyIterator  
**Problem solved:** Eliminates iterator boilerplate through compile-time policy dispatch.
**When to use:** Multiple traversal patterns over the same data; need STL algorithm compatibility; want debug-mode bounds checking  
**When NOT to use:** Single-use iteration; need runtime-variable traversal strategy; need random-access iterator  
**Key guarantee:** Policy dispatch is resolved at compile time; exact code generation remains compiler- and workload-dependent.
**Alternatives:** Manual iterators, Boost.Iterator, C++20 ranges, range-v3  
**Read next:** User Manual - PolicyIterator, Companion Guide - PolicyIterator, Overview - TensorStridePolicy

---

## Scope

This document introduces PolicyIterator, Fat-P's policy-based iterator framework. It covers the problem PolicyIterator solves (iterator boilerplate explosion), the architectural approach (compile-time policy dispatch), available policies (Standard, Stride, Filter, Transform), performance characteristics, and guidance on when to use or avoid PolicyIterator.

## Not Covered

- Detailed API reference (see User Manual - PolicyIterator)
- Design rationale and tradeoff analysis (see Companion Guide - PolicyIterator)
- TensorStridePolicy and multi-dimensional iteration (see Overview - TensorStridePolicy)

## Prerequisites

- Familiarity with C++ iterators and the STL iterator model
- Understanding of template-based generic programming

---

## Executive Summary

PolicyIterator is a **policy-based iterator** that separates traversal strategy from iteration mechanics through compile-time dispatch. A single `PolicyIterator<T, Policy>` template adapts to standard, strided, filtered, or transformed iteration—all with **zero runtime overhead** compared to hand-written loops.

---

## The Problem

```cpp
// THE TRAP: Every iterator flavor requires its own class
class StrideIterator { /* 50 lines */ };
class FilterIterator { /* 60 lines */ };
class TransformIterator { /* 55 lines */ };
// Want stride + filter? Write ANOTHER class.
// The combinations explode.
```

| Constraint | Why Manual Iterators Hurt |
|------------|---------------------------|
| Boilerplate explosion | Each pattern needs 50-100 lines |
| Maintenance burden | Bug fixes must propagate to every variant |
| Abstraction tax | Virtual dispatch or std::function kills performance |

---

## The Solution

PolicyIterator factors out common mechanics and delegates traversal to lightweight policy structs:

```cpp
template <typename T, typename Policy = StandardPolicy<T>>
class PolicyIterator {
    T* mPtr;
    Policy mPolicy;
    
    PolicyIterator& operator++() {
        mPolicy.advance(mPtr);  // Inlined at compile time
        return *this;
    }
};
```

Because policies are template parameters, dispatch is resolved at compile time—the optimizer sees through the abstraction completely.

---

## Feature Summary

| Feature | Mechanism | Benefit |
|---------|-----------|---------|
| Static-dispatch abstraction | Policy templates | Optimizer-visible policy operations |
| Bounds-safe by default | Debug-mode `enforce` checks | Catches past-end dereference |
| STL algorithm compatible | Proper iterator category tags | Works with `std::accumulate`, etc. |
| Policy composition | Nest policies or combine at call site | Filter + transform without new class |

---

## Performance

Static dispatch avoids a required virtual call, but target code generation and
runtime cost depend on the compiler, flags, selected policy, predicate state,
and workload. Benchmark hot paths against the corresponding manual traversal.

---

## Where PolicyIterator Wins

- **Code reuse with compile-time dispatch**
- **Debug-mode safety** via `enforce()` checks
- **STL compatibility** for algorithms

## Where PolicyIterator Loses

- **Complex compositions** — C++20 ranges offer better syntax
- **Runtime-variable strategies** — need variant or virtual dispatch
- **Random-access iteration** — PolicyIterator is at most bidirectional

---

## Why Not Alternatives?

| Criterion | Manual | C++20 Ranges | Boost.Iterator | PolicyIterator |
|-----------|--------|--------------|----------------|----------------|
| Compile-time dispatch | N/A | Usually | Depends on adaptor | **Yes** |
| No dependencies | Yes | C++20 | Boost | **Header-only** |
| Debug safety | Manual | No | No | **Yes** |

---

*PolicyIterator.h: ~510 lines — See User Manual for complete usage guide*
