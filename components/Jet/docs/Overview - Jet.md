---
doc_id: OV-JET-001
doc_type: "Overview"
title: "Jet"
fatp_components: ["Jet"]
topics: ["forward-mode automatic differentiation", "dual numbers", "gradient", "Jacobian", "chain rule", "exact derivatives", "finite-difference error"]
constraints: ["finite-difference truncation and roundoff", "step-size tuning", "hand-derived derivative maintenance", "third-party dependency ban"]
cxx_standard: "C++20"
std_equivalent: null
std_since: null
boost_equivalent: "Boost.Math autodiff (boost::math::differentiation)"
build_modes: ["Debug", "Release"]
last_verified: "2026-06-05"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "draft"
---

# Overview - Jet

*Fat-P Library — June 2026*

---

## Executive Summary

Jet is a fixed-size forward-mode automatic differentiation scalar: it carries a value together with its partial derivatives with respect to N independent variables, and propagates them through arithmetic and the elementary functions by the chain rule. A function written once over its scalar type, evaluated on seeded Jets, returns both its result and its gradient (or, for vector outputs, its full Jacobian) at the evaluation point — with derivatives correct to floating-point rounding, not the truncation-plus-roundoff error that finite differences carry. It is the forward-mode dual-number design familiar from `ceres::Jet`, carried into Fat-P without that design's Eigen dependency: storage is a `std::array<double, N>`, propagation runs through `std::ranges`, and the arithmetic is `constexpr`.

---

## Overview Card

**Component:** Jet  
**Problem solved:** Exact first-order derivatives (gradients and Jacobians) from ordinary C++ code, without finite-difference error or hand-maintained derivative formulas  
**When to use:** First-order gradients/Jacobians of functions with a modest number of inputs, especially feeding gradient-based solvers that need only first derivatives  
**When NOT to use:** Scalar objectives in very high dimension (reverse mode is asymptotically cheaper); higher-order derivatives; sparse Hessians  
**Key guarantee:** Derivatives are exact to rounding (no step-size error); fixed storage with no heap allocation; arithmetic is `constexpr`  
**std equivalent:** None. No standard equivalent exists or is planned.  
**Boost equivalent:** `boost::math::differentiation::autodiff` (Taylor-series forward mode, arbitrary order; pulls in Boost.Math)  
**Other alternatives:** `ceres::Jet`, Eigen `AutoDiffScalar` (both require Eigen); CppAD, ADOL-C (separate libraries; tape-based, support reverse mode)  
**Read next:** User Manual - Jet, Companion Guide - Jet

---

## The Problem Domain

### What Goes Wrong Without It

A gradient-based optimizer needs the derivatives of an objective. The path most reach for first is central finite differences: perturb each input by a small step and subtract.

```cpp
// Central finite differences: one gradient costs N+1 evaluations of f,
// and the answer depends on the step h you happened to pick.
double grad_fd(double (*f)(const double*), const double* x, int n, int i, double h)
{
    double xp[64], xm[64];
    for (int k = 0; k < n; ++k) { xp[k] = xm[k] = x[k]; }
    xp[i] += h;
    xm[i] -= h;
    return (f(xp) - f(xm)) / (2.0 * h);
}
```

The result is never the derivative; it is an estimate whose error is governed by two terms pulling in opposite directions. The truncation term falls as the step shrinks (order `h^2` for the central rule), while the roundoff term — the subtraction of two nearly equal values divided by a tiny `h` — grows as the step shrinks (order `eps/h`). The best achievable accuracy sits near `h ~ eps^(1/3)`, around six significant digits, and only if you tuned `h` for this function at this point. Change the scale of an input and the good step changes with it.

| Issue | HPC impact |
|-------|------------|
| Truncation vs. roundoff tradeoff | Best case loses about half the mantissa; no step is right everywhere |
| N+1 evaluations per gradient | A 30-variable Jacobian row costs 31 evaluations of the model |
| Catastrophic cancellation | Near a flat region the subtraction yields noise, and the optimizer chases it |
| Step-size tuning | `h` becomes a per-input, per-scale knob that has to be maintained |

The alternative most teams fall back to — deriving every partial by hand and coding it alongside the function — removes the numerical error but replaces it with a maintenance liability: every change to the model is now two edits that must be kept in agreement, and a sign error in a derivative is invisible until the solver quietly fails to converge.

### The Standard's Limitation

The C++ standard library has no automatic differentiation facility, and none is planned. `<cmath>` gives you the functions but not their derivatives; nothing in `std` propagates a gradient through an expression. The language leaves you with finite differences or hand-written derivatives, which is exactly the choice above.

---

## Architecture: Forward-Mode Jet (Dual Numbers, N Directions)

Jet generalizes the dual number. Where a dual number carries one derivative alongside a value, a Jet carries N of them — one per independent variable — so a single evaluation produces a full gradient.

```cpp
template <std::size_t N>
    requires(N >= 1)
struct Jet
{
    double mValue{0.0};
    std::array<double, N> mPartials{};
};
```

**The mechanism.** Every operation computes the result value and applies the chain rule to the stored partials. For a unary function `y = f(x)` the partials become `f'(value) * mPartials`; for a binary `z = g(x, y)` they become `gx * x.mPartials + gy * y.mPartials`. Both reduce to a single elementwise pass over the partial arrays, so each elementary operation is O(N) in the number of directions and touches no memory beyond the fixed `std::array`. Because that pass is expressed with `std::ranges::transform` over `std::array`, the arithmetic is usable in constant expressions: a derivative can be computed at compile time. The elementary functions (`sin`, `cos`, `exp`, `sqrt`, `pow`, `atan2`, …) are named to match `<cmath>` so that generic code calling `sin(x)` resolves to the Jet overload by argument-dependent lookup — the same source text differentiates whether `x` is a `double` or a `Jet`.

To start a differentiation you mark which variable is which by *seeding*: `Jet<N>::seed(value, k)` sets the value and places a unit in partial direction `k`. From there the gradient assembles itself as the expression evaluates.

---

## Feature Inventory

### Seeding the Independent Variables

A computation differentiates with respect to whatever you seed. Each seeded variable claims one direction; everything else flows through the chain rule.

```cpp
using fat_p::autodiff::Jet;

Jet<2> x = Jet<2>::seed(3.0, 0); // d/dx lives in direction 0
Jet<2> y = Jet<2>::seed(4.0, 1); // d/dy lives in direction 1

Jet<2> f = x * x + sin(y);       // one evaluation, value and gradient
// f.mValue        == 9 + sin(4)
// f.mPartials[0]  == 2*x         (d f / d x)
// f.mPartials[1]  == cos(y)      (d f / d y)
```

### Whole-Jacobian Assembly

A vector-valued function evaluated on the same seeded inputs yields one gradient per output — the rows of the Jacobian — from a single pass over the code, with no per-output re-derivation.

```cpp
using fat_p::autodiff::Jet;

Jet<2> px = Jet<2>::seed(1.0, 0);
Jet<2> py = Jet<2>::seed(2.0, 1);

Jet<2> r     = hypot(px, py);   // distance from origin
Jet<2> theta = atan2(py, px);   // bearing
// r.mPartials and theta.mPartials are the two Jacobian rows of (r, theta)
// with respect to (px, py).
```

### Constant Folding of Derivatives

Because the arithmetic is `constexpr`, a derivative that depends only on compile-time inputs is computed by the compiler and leaves nothing to run.

```cpp
constexpr fat_p::autodiff::Jet<2> p =
    fat_p::autodiff::Jet<2>::seed(3.0, 0) * fat_p::autodiff::Jet<2>::seed(4.0, 1);
static_assert(p.mPartials[0] == 4.0); // d(xy)/dx == y, evaluated at compile time
```

### Value-Only Comparison

Comparisons act on the value alone, through a `std::partial_ordering` three-way operator, so a Jet orders the way its value does and remains usable in branchy numerical code. The ordering is partial because a NaN value compares unordered, as it should.

---

## Why Not Alternatives?

**No standard equivalent exists.** The decision is therefore between Jet and third-party automatic differentiation, and the deciding constraint is Fat-P's dependency policy: no third-party libraries.

The closest design match, `ceres::Jet`, is the same fixed-size forward-mode jet — but it lives inside Ceres and is built on Eigen, as is Eigen's own `AutoDiffScalar`. Both are ruled out by the dependency ban, not by their design. Jet is that design without the dependency.

Boost.Math offers `boost::math::differentiation::autodiff`, a forward-mode facility that computes derivatives to arbitrary order through Taylor coefficients. It is genuinely more capable on the order axis, and the honest reason to prefer Jet over it is scope: Jet is first-order only and carries no Boost dependency.

CppAD and ADOL-C are tape-based libraries that record an evaluation and replay it, which buys them reverse-mode differentiation and sparse-derivative support. For the gradient of a scalar objective in high dimension, reverse mode is asymptotically cheaper than any forward method, Jet included. Those libraries remain the right tool there; Jet does not attempt to compete on that axis.

| If you need... | Why not the alternative | Jet's position |
|----------------|-------------------------|----------------|
| Zero third-party dependencies | `ceres::Jet` and Eigen `AutoDiffScalar` require Eigen | `std`-only, single header |
| First-order gradient/Jacobian, modest input count | CppAD/ADOL-C add a tape and a library to manage | Direct value-and-derivative evaluation |
| `constexpr` derivatives | None of the above are constant-evaluable | Arithmetic is `constexpr` |
| Higher-order derivatives | Jet is first-order | Use Boost.Math autodiff |
| Reverse mode for high-dimensional scalar objectives | Forward mode costs O(N) directions | Use CppAD/ADOL-C |

**When to use Jet:** first-order gradients and Jacobians, in dependency-free C++20, for functions whose input count is modest — including objectives and constraints handed to solvers that consume only first derivatives.

---

## The "Forever Stuck" Reality

Scientific clusters routinely pin a toolchain — RHEL with a fixed GCC, a CUDA driver that dictates the compiler — and a project may be held at C++20 for years regardless of what later standards ship. Pulling Eigen or Boost onto such a system to obtain derivatives is often the very dependency that build and procurement rules forbid. Jet is a single header against the standard library: it provides exact first-order derivatives in that environment without adding anything to the dependency surface, and it remains the appropriate first-order tool after any later compiler upgrade.

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|------------|-----------|
| Arithmetic / elementary function | O(N) in directions | One elementwise pass over the partial array; no allocation |
| Gradient of one scalar output | O(N) per evaluation | Forward propagation of N seeded directions |
| Compile-time evaluation | None at runtime | `constexpr` arithmetic folds when inputs are constant |

### Where Fat-P Wins

When the input count is modest and you need the value and an exact gradient together, Jet produces both in one evaluation, with no step-size to tune and no allocation. The `constexpr` path lets derivatives that depend only on constants disappear before the program runs.

### Where Fat-P Loses

Forward mode carries one direction per input, so the gradient of a scalar objective in high dimension costs O(N) propagation that reverse-mode tools (CppAD, ADOL-C) avoid. Higher-order derivatives are out of scope — Boost.Math autodiff covers that axis. Sparse Hessians belong to the tape-based libraries. No Jet benchmarks are published yet; when they exist they will live under `components/Jet/results/` rather than in this prose (per the evidence standard, specific numbers do not belong here).

---

## Integration Points

```
Jet.h
    → uses: standard library only (<array>, <cmath>, <ranges>, <compare>)
    → used by: (none yet) intended for gradient and Jacobian assembly in
               solver-facing numerics that consume first derivatives
```

Jet sits in the Foundation layer and pulls in nothing from Fat-P, so it composes with any higher layer without creating a dependency cycle.

---

## Final Assessment

Jet delivers on the Fat-P promise:

**Permanence.** No standard automatic differentiation is coming, and the forward-mode jet is a settled design rather than a stopgap. Jet is the solution, not a wait for one.

**Specialization.** Exact first-order derivatives with no step-size error, fixed storage with no allocation, and `constexpr` arithmetic — the properties numerical-optimization code actually needs from a differentiation primitive.

**Control.** The direction count N is a compile-time parameter, and seeding chooses exactly which variables a computation differentiates against, with no runtime configuration.

For first-order gradients and Jacobians in dependency-free C++20 — the derivatives a gradient-based solver needs — Jet is the solution.

---

*Jet.h — Fat-P Library — June 2026*
