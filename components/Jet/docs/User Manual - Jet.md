---
doc_id: UM-JET-001
doc_type: "User Manual"
title: "Jet"
fatp_components: ["Jet"]
topics: ["forward-mode automatic differentiation", "gradient", "Jacobian", "seeding", "chain rule", "solver derivatives", "dual numbers"]
constraints: ["finite-difference error", "derivative maintenance", "allocation-free derivatives", "third-party dependency ban"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.Math autodiff (boost::math::differentiation)"
build_modes: ["Debug", "Release"]
last_verified: "2026-06-07"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "complete"
---

# User Manual - Jet

**Scope:** How to compute exact first-order derivatives — gradients and Jacobians — with `fat_p::autodiff::Jet`, including seeding, vector outputs, the domain/NaN model, and migration from finite differences, hand-written derivatives, and `ceres::Jet`.

**Not covered:** Reverse-mode differentiation, higher-order derivatives, and sparse Hessians. Jet is first-order forward mode; the closing section points to the tools that own those axes.

**Prerequisites:** Working C++20, the chain rule, and a passing acquaintance with floating-point rounding. No prior exposure to automatic differentiation is assumed.

---

## User Manual Card

**Component:** Jet  
**Primary use case:** First-order gradient or Jacobian of a function with a modest number of inputs, evaluated alongside the function's value in a single pass  
**Integration pattern:** Include `fat_p/Jet.h`; template your numeric code on its scalar type, or write it against `Jet<N>` directly; seed inputs and read `mPartials`  
**Key API:** `Jet<N>`, `Jet<N>::seed(value, k)`, the arithmetic operators, and the `<cmath>`-named elementary functions  
**std equivalent:** None.  
**Migration from std::** Not applicable — no standard automatic differentiation exists. Migration paths here are from finite differences, hand-derivatives, and `ceres::Jet`.  
**Common mistakes:** Forgetting to seed (gradient comes back all zeros); trusting a gradient without checking the value at a domain edge; setting `N` larger than the number of inputs you actually differentiate against  
**Performance notes:** O(N) per operation in the direction count; no heap allocation; `constexpr` when inputs are compile-time constants

---

## Why Derivatives Are Hard to Get Right

Every gradient-based method — a nonlinear solver, a least-squares fit, a trajectory optimizer — needs the derivatives of the function it is working on. The function is yours to write; the derivatives are the problem.

The first instinct is to perturb. Nudge each input by a small step, re-evaluate, and divide the change by the step. This is the finite difference, and it never returns the derivative — only an estimate sitting between two errors that move in opposite directions. Shrink the step and the truncation error falls but the roundoff error, born of subtracting two nearly equal numbers, rises. The central rule does best near a step of `eps^(1/3)`, where it bottoms out around `eps^(2/3)` — roughly ten good digits, and only when that step happens to suit this function at this point. Change the scale of an input and the right step moves. Land in a flat region and the subtraction returns noise the solver then chases.

The second instinct is to differentiate by hand and code the result. This is exact, and it creates a second source of truth. From then on every change to the function is two edits that must agree, and a single wrong sign in a partial is invisible until a solver quietly fails to converge for reasons no one can see.

Forward-mode automatic differentiation removes the choice. You write the function once. As it evaluates, each operation carries the derivative along with the value by the chain rule, and what comes out the far end is the value and its exact gradient — exact meaning correct to rounding, the same precision as the function itself. There is no step to tune and no second formula to maintain. That is what Jet is.

---

## Architecture / How It Works

A Jet is a value travelling with its derivatives. Where a dual number carries one derivative beside a value, `Jet<N>` carries N of them, one per independent variable, so a single evaluation yields a whole gradient.

```cpp
template <std::size_t N>
    requires(N >= 1)
struct Jet
{
    double mValue{0.0};
    std::array<double, N> mPartials{};
};
```

Each operation does two things: it computes the result value, and it applies the chain rule to the partials. A unary function `y = f(x)` sends the partials to `f'(value) * mPartials`; a binary `z = g(x, y)` sends them to `gx * x.mPartials + gy * y.mPartials`. Both are one elementwise pass over the fixed `std::array`, so the cost of an operation is O(N) in the number of directions and touches no memory beyond the array already in the Jet. Because that pass is written with `std::ranges`, the arithmetic is usable in constant expressions — a derivative built from compile-time inputs is computed by the compiler.

The elementary functions carry the names from `<cmath>` — `sin`, `cos`, `exp`, `sqrt`, `pow`, `atan2`, and the rest — and live in the same namespace as `Jet`. When generic code calls `sin(x)` on a Jet, argument-dependent lookup resolves to the Jet overload, so the identical source text differentiates whether `x` is a `double` or a `Jet<N>`.

You decide which variables a computation differentiates against by *seeding*. `Jet<N>::seed(value, k)` makes a Jet whose value is `value` and whose partial in direction `k` is one, every other direction zero. That single unit is the derivative of the variable with respect to itself; from there the chain rule grows the rest of the gradient as the expression runs.

---

## Quick Start

Jet is a single header against the standard library. Include it and you have the type:

```cpp
#include "fat_p/Jet.h"
using fat_p::autodiff::Jet;
```

To differentiate `f(x) = x^2 + 3x` at `x = 2`, seed `x` in the one direction you have and evaluate the function as written. The result holds both `f(2)` and `f'(2)`:

```cpp
Jet<1> x = Jet<1>::seed(2.0, 0); // one variable, direction 0
Jet<1> f = x * x + 3.0 * x;
// f.mValue       == 10.0   (2^2 + 3*2)
// f.mPartials[0] ==  7.0   (2x + 3 at x = 2)
```

Nothing about `x * x + 3.0 * x` mentions a derivative. The derivative rode through the arithmetic on its own. That is the whole idea, and the recipes below are variations on it.

---

## Recipes

### Choosing and Seeding Your Variables

The direction count `N` is the number of variables you want derivatives for, and seeding is how you assign each variable its direction. A variable that is not seeded — one built with the ordinary constructor — is a constant as far as differentiation is concerned: its partials are zero, and it contributes to values but not to gradients.

This matters because the most common first mistake is a gradient that comes back all zeros. The cause is almost always a variable that was constructed instead of seeded. Construction makes a constant; seeding makes a variable.

```cpp
Jet<2> x = Jet<2>::seed(3.0, 0); // variable in direction 0
Jet<2> y = Jet<2>::seed(4.0, 1); // variable in direction 1
Jet<2> c{10.0};                  // a CONSTANT: all partials zero

Jet<2> f = x * y + c;
// f.mPartials[0] == 4.0  (= y), f.mPartials[1] == 3.0 (= x)
// c contributed 10 to the value and nothing to the gradient
```

Seed only the variables you need. If a quantity is genuinely fixed for this computation, leave it a constant — it costs nothing and keeps `N` down.

### The Gradient of a Scalar Function

For a function that returns one number, the gradient is the vector of its partials. Seed each input in its own direction, evaluate, and read `mPartials`.

Consider a quadratic form `q(x, y) = x^2 + x*y + 2*y^2`, whose gradient is `(2x + y, x + 4y)`. Written over Jets, the gradient assembles itself:

```cpp
Jet<2> x = Jet<2>::seed(1.5, 0);
Jet<2> y = Jet<2>::seed(-0.5, 1);

Jet<2> q = x * x + x * y + 2.0 * y * y;
// q.mValue       == 2.25 - 0.75 + 0.5 == 2.0
// q.mPartials[0] == 2*1.5 + (-0.5)    == 2.5   (dq/dx)
// q.mPartials[1] == 1.5 + 4*(-0.5)    == -0.5  (dq/dy)
```

The gotcha here is index discipline. Direction `0` means whatever you seeded into direction `0`; the partials are returned in seed order, not in any order the function implies. Keep a single, fixed mapping from variable to direction across the whole computation.

### Assembling a Jacobian for Vector Output

When a function produces several outputs from the same inputs, evaluate all the outputs on the same seeded Jets. Each output's `mPartials` is one row of the Jacobian, and you get every row from a single pass over the code — no per-row re-derivation, no extra evaluations.

A common case is converting Cartesian coordinates to polar: `r = hypot(x, y)` and `theta = atan2(y, x)`. The two outputs share the two inputs, so the Jacobian is two rows of two:

```cpp
Jet<2> x = Jet<2>::seed(3.0, 0);
Jet<2> y = Jet<2>::seed(4.0, 1);

Jet<2> r     = hypot(x, y);  // value 5
Jet<2> theta = atan2(y, x);

// Jacobian rows:
//   d r     / d(x,y) == { r.mPartials[0],     r.mPartials[1] }     == { 0.6, 0.8 }
//   d theta / d(x,y) == { theta.mPartials[0], theta.mPartials[1] } == { -4/25, 3/25 }
```

The structural point is that forward mode pays for inputs, not outputs. Adding a third output here costs one more evaluation of that output, not another sweep of the inputs — which is exactly why forward mode suits functions with few inputs and many outputs, and why it does not suit the reverse case (covered under When to Use).

### Feeding a Gradient-Based Solver

Solvers such as SNOPT call back into your code for the objective and constraint values and for their Jacobian. The Jacobian is where finite differences hurt most: each row costs an extra evaluation per variable, and the gradient noise floor caps how tightly the solver can converge.

The pattern is to template the model on its scalar type so the same code serves both the value call (instantiated on `double`) and the derivative call (instantiated on `Jet<N>`), with no second implementation to keep in step.

```cpp
template <class T>
T residual(const T* p)                 // model written once, over a scalar type
{
    return p[0] * p[0] + sin(p[1]) - p[2]; // sin() resolves by ADL for both double and Jet
}

// Value call from the solver:
double r = residual<double>(point);

// Jacobian call: seed each of the 3 inputs, evaluate once, read the row.
Jet<3> jp[3] = { Jet<3>::seed(point[0], 0),
                 Jet<3>::seed(point[1], 1),
                 Jet<3>::seed(point[2], 2) };
Jet<3> jr = residual<Jet<3>>(jp);
// jr.mPartials is the full gradient row: { 2*p0, cos(p1), -1 }
```

The gotcha is matching the solver's expected derivative layout. A solver has its own convention for how Jacobian entries are ordered and which are structurally zero; map `mPartials[k]` into that layout deliberately, and let known-zero entries stay zero rather than seeding directions you do not need.

### Derivatives at Compile Time

When the inputs are compile-time constants, the arithmetic is `constexpr`, so a derivative can be folded by the compiler and verified with `static_assert`. This is the recipe for a derivative that is fixed by the program rather than the data — a coefficient, a calibration constant, a unit-test expectation.

```cpp
constexpr Jet<2> a = Jet<2>::seed(3.0, 0) * Jet<2>::seed(4.0, 1);
static_assert(a.mPartials[0] == 4.0); // d(xy)/dx == y, computed during compilation
```

The limit to know: the elementary functions (`sin`, `exp`, `sqrt`, …) call into `<cmath>`, which is not `constexpr` in C++20, so a `constexpr` Jet computation may use the arithmetic operators but not those functions. Plain arithmetic folds; transcendental calls do not.

---

## Error Handling Model

Jet does not throw and does not allocate, so its failure model is the floating-point one: out-of-range inputs produce NaN or infinity in the value, and those propagate. The rule for a caller is therefore short — check the value before you trust the gradient. For finite, in-domain inputs a finite value means the gradient beside it is the propagated derivative; a NaN or infinite value means the point was outside the domain and the gradient is not to be relied on. One documented case runs the other way: the exponent-differentiated power forms — `pow(Jet, Jet)` and `pow(double, Jet)` — require a positive base, and on a non-positive base they may keep the real value where it is defined while returning a NaN gradient under that positive-base contract. The constant-exponent overload `pow(Jet, double)` is not subject to this: it differentiates a non-positive base correctly wherever the real power is defined (for example `pow(x, 2.0)` gives a finite gradient at a negative base). Where a function names a restriction, honor it before trusting the partials.

A few boundary cases are handled deliberately rather than left to raw `<cmath>` behavior. `sqrt` at exactly zero, where the true slope is infinite, returns a value of zero and a guarded derivative of zero rather than an infinity that would poison the rest of a gradient; the constant-exponent `pow(x, p)` follows the same convention for `0 < p < 1` at `x == 0`, so `pow(x, 0.5)` and `sqrt(x)` agree everywhere. `abs` at zero, where no derivative exists, returns zero as its subgradient. These are documented conventions, not accidents, and they keep a single awkward point from contaminating an otherwise finite gradient.

One numerical limitation is worth naming. Division guards the common extreme-scale cases — a tiny denominator no longer poisons a structurally-zero direction, and a finite derivative is not lost to a divide-first intermediate overflow — but one case is not recovered: when the quotient *value* itself overflows to infinity and the denominator carries a nonzero partial in an active direction, that direction's derivative is returned as infinite even where the true value would be finite (roughly `-1e300` for a denominator near `1e-310`). This is a property of the quotient as a whole, not of one overload, and recovering it would need an exponent-scaled helper; it is documented and regression-pinned rather than silently hidden.

> **Critical: trust the value, then inspect the gradient.** At ordinary in-domain points the gradient is exact to rounding; read `mValue` first at any edge. Outside the real domain — `sqrt` or `log` of a negative argument, `asin`/`acos` past `±1` — the value and every partial are NaN, so a NaN value never sits beside a finite-looking gradient. Two kinds of singular boundary then differ. Where the value itself is unbounded, value and partial are both infinite: `log` at `+0.0` gives `-inf` with a `+inf` derivative (signed `-0.0` flips the sign, following IEEE). Where the value is finite but the slope is unbounded — `asin(1)`, `acos(±1)` — the value is ordinary (`π/2`, `0`) while the derivative coefficient is infinite, so a seeded (active) direction carries `±inf` and any unseeded (inactive) direction carries `NaN` through IEEE `inf * 0`; read the value, confirm you are sitting on the edge, and treat that gradient as unusable. The guarded points are `sqrt` (and constant-exponent `pow` with `0 < p < 1`) and `abs` at exactly 0, and `hypot` and `atan2` at the origin `(0, 0)`: where the slope is undefined or unbounded but a single point should not poison the rest of a gradient, these return a finite 0 by convention rather than an infinity or a NaN.

Jet's results do not depend on optimization level or sanitizers. The one compile-time contract — that `N` is at least one — is enforced by a `requires` clause, so `Jet<0>` fails to compile regardless of build mode. There is one deliberate debug/release split: the runtime `seed(value, k)` checks its precondition `k < N` with `assert` in debug builds and leaves it unchecked in release, matching the indexing policy of the standard containers; for a direction fixed at compile time, `seed<K>()` enforces `K < N` with no runtime check at all.

---

## Performance Rules of Thumb

The cost of every operation scales with `N`, the direction count, because each operation sweeps the partial array once. The first rule follows directly: make `N` the number of variables you actually differentiate against, and leave everything else a constant. A constant carries no direction and adds nothing to the sweep.

The second rule is about which way your function points. Forward mode propagates one direction per input, so it is inexpensive when inputs are few and outputs many, and expensive when inputs are many and the output is a single scalar. For a scalar objective in high dimension, the work grows with the dimension in a way reverse mode avoids — see When to Use.

The third rule is to let the compiler do the work it can. Plain-arithmetic derivatives over constant inputs fold under `constexpr` and cost nothing at run time. There is no allocation anywhere in Jet, so there is no allocation behavior to tune. Specific timings are not quoted here; when Jet benchmarks exist they will live under `components/Jet/results/`.

---

## When to Use / When Not To

Reach for Jet when you need first-order derivatives, exact to rounding, of a function with a modest number of inputs, and you want the value and the gradient from one pass without a step to tune or a second formula to maintain. The fit is strongest when the dependency policy forbids Eigen and Boost, when the input count is small enough that O(N) propagation is comfortable, and when the same model code can serve the value and derivative calls by being templated on its scalar type.

Do not reach for Jet when the function maps many inputs to a single scalar and you need its gradient — that is reverse mode's home, where the gradient costs a small constant number of passes regardless of dimension, and CppAD or ADOL-C are the right tools. Do not reach for it when you need second or higher derivatives; Boost.Math autodiff carries Taylor coefficients to arbitrary order. And do not reach for it for sparse Hessians, which the tape-based libraries are built to exploit.

---

## Migration Guide

### From Finite Differences

A finite-difference gradient loop has the shape of 2N evaluations (two per input) and a step constant. Replace the perturb-and-subtract with seeded Jets and a single evaluation; the step constant disappears, and the result is exact rather than an estimate.

```cpp
// THE TRAP: central differences -- 2N evals, a step to tune, ~eps^(2/3) error
for (int i = 0; i < n; ++i) {
    auto xp = x; auto xm = x;
    xp[i] += h; xm[i] -= h;
    grad[i] = (f(xp) - f(xm)) / (2.0 * h);
}

// THE FIX: one evaluation over seeded Jets -- exact to rounding, no step
Jet<N> jx[N];
for (std::size_t i = 0; i < N; ++i) jx[i] = Jet<N>::seed(x[i], i);
Jet<N> jf = f(jx);            // f templated on its scalar type
// jf.mPartials is the gradient
```

### From Hand-Written Derivatives

Where a hand-derivative function sits beside the model, delete it and template the model on its scalar type. The derivative then comes from the same code that computes the value, and the two can no longer drift apart. The migration risk is behavioral equivalence: before deleting the hand-derivative, evaluate both on the same points and confirm the Jet gradient matches, then remove the duplicate.

### From ceres::Jet

The migration that requires the least change is from `ceres::Jet`, because Jet is the same forward-mode design. The element type and member access differ in spelling, and the dependency on Eigen goes away. Code that already templates its cost functions on a scalar type ports by swapping the Jet type and reading partials from `mPartials` instead of Ceres' `v` array.

| Aspect | `ceres::Jet<T, N>` | Boost.Math autodiff | Fat-P `Jet<N>` |
|--------|--------------------|---------------------|----------------|
| Dependency | Eigen (via Ceres) | Boost.Math | Standard library only |
| Derivative order | First | Arbitrary (Taylor) | First |
| Mode | Forward | Forward | Forward |
| Element type | Templated `T` | Templated | `double` |
| Partials access | `.v` (Eigen vector) | coefficient indexing | `.mPartials` (`std::array`) |
| `constexpr` arithmetic | No | No | Yes (operators) |

**When to stay with the alternative:** keep `ceres::Jet` if you are already inside Ceres and Eigen is present anyway; choose Boost.Math autodiff if you need higher-order derivatives; choose CppAD or ADOL-C if you need reverse mode or sparse derivatives.

---

## Troubleshooting

### Compilation Errors

A failure that names the constraint `requires N >= 1` means a `Jet<0>` was instantiated somewhere. A Jet must carry at least one direction; give it the number of variables you differentiate against. If a call like `sin(x)` fails to resolve, confirm `x` is a `Jet` whose type is visible — the elementary functions are found by argument-dependent lookup, so a Jet argument is what pulls them in; calling them on a `double` falls through to `<cmath>` as usual.

### Runtime Errors

A gradient of all zeros where you expected nonzero entries almost always means the inputs were constructed rather than seeded; a constructed Jet is a constant. Re-check that every variable came from `seed`. A NaN gradient at a point you believe is in-domain usually means the value is also NaN — read `mValue` first and confirm the point is inside the function's domain. Two cases break that pattern. First, `pow(base, exponent)` with a `Jet` exponent and a non-positive base returns a correct value but a NaN gradient, because the exponent-side term carries `log(base)`; when the exponent is constant, call `pow(x, double)` instead — it differentiates a non-positive base correctly. Second, a finite-valued singular boundary such as `asin(1)` or `acos(±1)` pairs an ordinary value with an infinite derivative coefficient, so a NaN appears in inactive directions (through `inf * 0`) beside a perfectly finite value — read the value, confirm you are sitting on the edge, and treat the gradient as unusable rather than trustworthy. A partial that landed in the wrong slot is an index-mapping slip: the direction you read must be the direction you seeded.

### Performance Issues

A computation heavier than expected is usually carrying directions it does not need. Audit `N` against the variables you actually differentiate against, and demote the rest to constants. If a derivative depends only on compile-time inputs and is still being computed at run time, confirm it uses only the arithmetic operators — a transcendental call blocks `constexpr` folding in C++20.

---

## API Reference

`Jet<N>` — forward-mode AD scalar with `N >= 1` directions; a standard-layout, trivially copyable value type holding `double mValue` and `std::array<double, N> mPartials`.

Construction:
- `Jet()` — value zero, all partials zero.
- `explicit Jet(double v)` — a constant: value `v`, all partials zero.
- `static Jet seed(double v, std::size_t k)` — a variable: value `v`, partial `k` set to one.
- `template <std::size_t K> static Jet seed(double v)` — the same, with the direction `K` checked at compile time (`K < N`, so an out-of-range direction is ill-formed rather than asserted at runtime).

Comparison: a `constexpr friend operator<=>` returning `std::partial_ordering` and an `operator==`, both acting on `mValue` only; the remaining relational and reversed forms are synthesized. A NaN value compares unordered.

Arithmetic (`constexpr`): unary `-`; binary `+ - * /` for Jet-with-Jet and for either operand a `double`; and the compound assignments `+= -= *= /=` taking a Jet or a `double`.

Elementary functions (named to match `<cmath>`, found by ADL; not `constexpr` because `<cmath>` is not): `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `sqrt`, `abs`, `pow(Jet, double)`, `pow(Jet, Jet)` (positive base), `pow(double, Jet)` (positive base), `hypot`, `atan2`; `hypot` and `atan2` also accept a `double` on either side. Domain edges follow `<cmath>` for the value, with `sqrt` (and constant-exponent `pow`, `0 < p < 1`) and `abs` guarded to a finite derivative at 0 and `hypot`/`atan2` to a zero gradient at the origin; an out-of-domain value (NaN) carries NaN partials.

---

## FAQ

**Does `N` have to match the number of inputs?** `N` is the number of directions you want derivatives in. It is at least one and is fixed at compile time. Set it to the count of variables you seed; constants need no direction.

**Why is the gradient all zeros?** The inputs were constructed, not seeded. Construction makes a constant. Use `Jet<N>::seed(value, k)` for each variable.

**Can I differentiate at compile time?** Yes for the arithmetic operators, via `constexpr` and `static_assert`. Not for the elementary functions, because `<cmath>` is not `constexpr` in C++20.

**What happens outside a function's domain?** The value and every partial become NaN and propagate, so a NaN value is never paired with a finite-looking gradient. At a singular boundary the value may be infinite alongside an infinite partial (`log` at 0), or finite alongside an infinite derivative coefficient (`asin(1)`, `acos(±1)`) — the latter leaving `±inf` in seeded directions and NaN in unseeded ones. Either way, read `mValue` first.

**Why not reverse mode?** Reverse mode is asymptotically cheaper for the gradient of a high-dimensional scalar, but it needs a tape and the machinery to manage it. Jet is forward mode, which suits few inputs and many outputs and stays dependency-free. For the reverse case, use CppAD or ADOL-C.

---

*Jet.h — Fat-P Library — June 2026*
