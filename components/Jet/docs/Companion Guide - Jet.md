---
doc_id: CG-JET-001
doc_type: "Companion Guide"
title: "Jet"
fatp_components: ["Jet"]
topics: ["forward-mode automatic differentiation", "dual numbers", "finite-difference error", "Jacobian assembly", "solver convergence", "forward vs reverse mode"]
constraints: ["truncation vs roundoff", "derivative maintenance", "Eigen dependency ban", "gradient noise floor in solvers"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.Math autodiff (boost::math::differentiation)"
build_modes: ["Debug", "Release"]
last_verified: "2026-06-05"
audience: ["C++ developers", "performance engineers", "AI assistants"]
status: "draft"
---

# **Derivatives That Ride Along**

### *A Companion Guide to Fat-P's Jet forward-mode AD scalar*

**Scope:** This guide is about the problem of obtaining first-order derivatives from C++ code, and how the `Jet` forward-mode scalar resolves it. Reverse-mode differentiation, higher-order derivatives, and sparse Hessians are named where they belong but are not the subject here.

---

## Companion Guide Card

**Component:** Jet  
**Design question:** How do you get exact gradients and Jacobians out of ordinary numeric code without finite-difference error, without a second hand-written formula, and without a third-party dependency?  
**Key tradeoff:** Forward mode carries one derivative direction per input — exact and dependency-free, but O(N) in the input count, where reverse mode is asymptotically cheaper for high-dimensional scalar objectives  
**Decision made:** A fixed-size forward-mode jet over `double`, stored in a `std::array<double, N>`, with `constexpr` arithmetic and `<cmath>`-named functions found by ADL  
**Rejected alternatives:** `ceres::Jet` / Eigen `AutoDiffScalar` (require Eigen); CppAD / ADOL-C (tape-based, heavier); a runtime-sized partial vector (allocation)  
**Historical context:** Fat-P's dependency policy forbids Eigen and Boost, which rules out the established forward-mode jets by dependency rather than by design — leaving the design itself worth carrying in directly

---

## Part I — The Problems

### The Finite-Difference Bargain

**The obvious approach.** You need the gradient of a function, so you perturb it. Nudge each input by a small step `h`, re-evaluate, subtract, divide. It takes a few lines and no thought about the function's internals, and that is its appeal.

**The hidden constraint.** A finite difference is not the derivative; it is an estimate caught between two errors that pull against each other. The truncation error — the part the Taylor expansion drops — shrinks as `h` shrinks. The roundoff error — born when you subtract two nearly equal function values and divide by a tiny number — grows as `h` shrinks. There is no step that removes both. The central rule balances them near `h ~ eps^(1/3)` and bottoms out around `eps^(2/3)`, and that floor exists no matter how carefully you code.

**The symptoms.** A solver using finite-difference gradients converges to a point and then stalls, unable to tighten its tolerance below the gradient's noise floor. In a flat region the subtraction of two almost-identical values returns mostly noise, and the optimizer chases it. Rescale an input — change its units — and a step that was adequate becomes either too coarse or too fine.

**The cost.** Each gradient costs two evaluations per variable under the central rule, so a Jacobian row for a 12-variable model is 24 evaluations of that model — and at the end of all that work the answer still carries an error floor near `eps^(2/3)`. For an iterative solver calling back thousands of times, this is both the dominant cost and the limit on final accuracy.

**The solution preview.** If the derivative travelled through the computation alongside the value, there would be no subtraction, no step, and no floor above rounding.

**Forward reference.** Part IV — Foundations explains why a single number carried beside the value is enough to make this exact.

### The Hand-Derivative Liability

**The obvious approach.** Finite differences are inexact, so you differentiate the function by hand and code the partials next to it. Now the derivative is exact.

**The hidden constraint.** There are now two expressions of the same function — the one that computes the value and the one that computes its derivative — and nothing binds them together. They are two sources of truth that must agree by your vigilance alone.

**The symptoms.** Someone edits the model — adds a term, changes a coefficient — and updates the value but not the derivative, or updates the derivative with a wrong sign. The code compiles. The value is right. The gradient is subtly wrong, and the only outward sign is a solver that converges more slowly than it should, or to the wrong place, for no reason anyone can point to.

**The cost.** The defect is invisible to review because both functions look plausible in isolation, and it is expensive to find because the failure surfaces far downstream as poor convergence rather than as a wrong number.

**The solution preview.** If the derivative came from the same code that computes the value, the two could not drift apart, because there would be only one.

**Forward reference.** Part IV — Foundations covers why templating the model on its scalar type collapses the two sources into one.

---

## Part II — The Solutions

### Forward-Mode Jet: One Pass, Value and Gradient

**Problem link.** Part I showed two failures — the finite-difference error floor and the divergence of a hand-written derivative from its function. Both come from computing the derivative *separately* from the value: either by re-running the function, or by maintaining a parallel formula. Forward mode removes the separation.

**The mechanism.** A Jet generalizes the dual number. A dual number pairs a value with a single derivative and defines arithmetic so the derivative obeys the chain rule automatically: for a product, `(a + a' e)(b + b' e) = ab + (a'b + ab') e`, and the second component is exactly the product rule. A `Jet<N>` pairs a value with N such derivative slots — one per independent variable — so a single evaluation produces a whole gradient. Every operation computes the result value and applies the chain rule to the slots: a unary `f` maps the partials to `f'(value) * partials`; a binary `g` maps them to `gx * x.partials + gy * y.partials`. You mark which variable is which by seeding — `seed(value, k)` puts a one in slot `k` — and from that single unit the gradient grows as the expression runs. Because the per-operation work is one elementwise pass over a `std::array`, it allocates nothing; because that pass is written with `std::ranges`, the arithmetic folds under `constexpr`.

```cpp
// THE TRAP: the derivative computed apart from the value -- estimate, with a floor
grad[i] = (f(xp) - f(xm)) / (2.0 * h);   // ~eps^(2/3) error, one extra eval per input

// THE FIX: the derivative carried with the value -- exact, one pass
Jet<2> x = Jet<2>::seed(px, 0);
Jet<2> y = Jet<2>::seed(py, 1);
Jet<2> f = x * x + sin(y);               // f.mValue and f.mPartials together, exact to rounding
```

**Guarantees and non-guarantees.**

| Jet guarantees | Jet does not guarantee |
|----------------|------------------------|
| First-order partials exact to floating-point rounding | Second or higher derivatives |
| Value and gradient from a single evaluation | A gradient cost independent of input count |
| No heap allocation (fixed `std::array`) | Sparse-derivative exploitation |
| `constexpr` for the arithmetic operators | `constexpr` through the elementary functions (`<cmath>` limit) |
| `<cmath>`-named functions resolved by ADL | Reverse-mode accumulation |

**Decision guide.** Choose forward mode — Jet — when inputs are few and you want exact first derivatives beside the value, with no dependency. The choice tips away from it as the input count grows while the output collapses to a single scalar.

**Where it loses.** Forward mode propagates one direction per input, so the gradient of a scalar objective in dimension `N` carries `N` directions through every operation. Reverse mode computes that same gradient in a small constant number of passes by accumulating sensitivities backward. For high-dimensional scalar objectives the asymptotics favor reverse mode decisively, and Jet does not contend there.

---

## Part III — Case Study: A Path-Optimization Jacobian

**Context.** A motion-profile optimizer fits a smooth actuator path — a quintic Hermite segment with a handful of free parameters at each knot — by handing an objective and a set of constraints to a gradient-based solver. The solver calls back for values and for the constraint Jacobian on every iteration.

**Initial approach.** The Jacobian was supplied by central differences. For each constraint and each free parameter, the model was re-evaluated at a perturbed point and subtracted.

```cpp
// THE TRAP: central-difference Jacobian column -- one extra model eval per parameter
for (int j = 0; j < numParams; ++j) {
    auto pp = p; auto pm = p;
    pp[j] += h; pm[j] -= h;
    for (int i = 0; i < numConstraints; ++i)
        J[i][j] = (constraint_i(pp) - constraint_i(pm)) / (2.0 * h);
}
```

**Observing the symptoms.** Two things showed up. First, the per-parameter evaluation count: a segment with twelve free parameters cost twenty-four model evaluations for a single Jacobian (two per parameter under the central rule), repeated every solver iteration. Second, and more damaging, the solver could not tighten its optimality tolerance below the gradient's noise floor — the central-difference error near `eps^(2/3)` set a wall the iterations could not pass, so the optimizer declared convergence at a looser tolerance than the problem warranted, and the chosen step `h` had to be retuned whenever a parameter's scale changed.

**The fix.** The model was templated on its scalar type so the same code computed values on `double` and derivatives on `Jet<N>`. Each free parameter was seeded into its own direction; one evaluation per constraint then produced an exact Jacobian row.

```cpp
// THE FIX: seed the parameters once, evaluate each constraint once -- exact rows
template <class T> T constraint_i(const T* p);   // model written once

Jet<NP> jp[NP];
for (std::size_t j = 0; j < NP; ++j) jp[j] = Jet<NP>::seed(p[j], j);
for (int i = 0; i < numConstraints; ++i) {
    Jet<NP> c = constraint_i<Jet<NP>>(jp);
    // c.mPartials is row i of the Jacobian -- exact to rounding
}
```

**Results.** The evaluation count for the Jacobian fell from `2 * numParams` model evaluations to one, a theoretical and exact reduction rather than a measured speedup. The gradient error fell from the central-difference floor near `eps^(2/3)` (about ten digits, at a tuned step) to rounding near `eps` (full working precision), which let the solver reach the optimality tolerance the problem actually called for instead of stalling above it. The per-parameter step constant disappeared, so rescaling a parameter no longer required retuning. Absolute timing numbers are not quoted here — no Jet benchmark has been run for this case — but the evaluation-count and accuracy changes are properties of the method, not of a particular machine.

**Components used.** `Jet<N>` — sole component; supplied the seeded directions and the exact partials that became the Jacobian rows.

**Transferable lessons.** Template the model on its scalar type and you collapse the value and derivative into one implementation that cannot drift. When a solver stalls above its tolerance, suspect the gradient's noise floor before the solver itself. And count evaluations: forward mode delivers the whole Jacobian from one evaluation where central differences need `2 * numParams`, and the result is exact rather than step-limited — the right trade when inputs are few.

---

## Part IV — Foundations

### Why a Dual Number Is Enough

The dual number works because of a single algebraic fact: define a symbol `e` with `e^2 = 0` and carry quantities as `a + a' e`. Then multiplication gives `(a + a' e)(b + b' e) = ab + (a'b + ab') e` — the `e^2` term vanishes — and the coefficient of `e` is precisely the product rule. Every differentiation rule falls out of this the same way: the value lives in the real part, the derivative in the `e` part, and ordinary algebra propagates both. A Jet is this idea with N independent infinitesimals instead of one, so N derivatives ride along at once. Nothing about it is an approximation; it is the chain rule executed by the type system.

### Why ceres::Jet's Design, but Not Ceres

The fixed-size forward-mode jet is a settled, well-understood construction, and `ceres::Jet` is its best-known C++ embodiment. The reason Fat-P does not simply use it is not the design — it is the dependency. `ceres::Jet` is part of Ceres and built on Eigen, and Eigen's own `AutoDiffScalar` is the same story. Fat-P's policy forbids third-party libraries, so the established jets are excluded by what they drag in, not by how they work. The decision was therefore to carry the design itself into a single standard-library header: a `std::array<double, N>` for the partials, `std::ranges` for the propagation, and `constexpr` arithmetic — the jet, without Eigen.

### Why Comparison Looks at the Value Only

A Jet has a value and a gradient, and an ordering has to decide what it compares. Comparing gradients would make two Jets with the same value but different seeded directions unequal, which would break the branchy numeric code that expects a number to order by its magnitude. So the three-way operator compares `mValue` alone and returns `std::partial_ordering` — partial precisely because a NaN value must compare unordered, the same way `double` does. A Jet orders the way its value orders, and the derivative comes along for the ride without disturbing it.

### The Out-of-Domain Partial Decision

At in-domain points the gradient is exact. At the domain edges there were two behaviors to reconcile. Most functions already agreed with themselves — `asin` outside `[-1, 1]` returns NaN in both value and partial — but `sqrt` and `log` of a negative argument originally returned a NaN value beside a non-NaN partial: `sqrt(-1)` a guarded zero slope, `log(-2)` the finite `1/(-2)`. A caller reading only the gradient there saw a clean number where the point was undefined.

The decision was to make the rule uniform: when a function's value is NaN, its partials are NaN too. The gate is on a NaN value specifically, not on a non-finite one, and that distinction is the point — it preserves the two boundary behaviors that are mathematically right rather than flattening them. `log` at 0 keeps its `-inf` value and `+inf` derivative, because the slope there really is unbounded; `sqrt` and `abs` at exactly 0 keep their convention of a finite 0 slope, chosen so a single singular point does not poison an otherwise finite gradient. The invariant a caller relies on is the same as before — read the value — now backed consistently by NaN partials whenever the value is NaN.

A further choice sits beside this one and was decided the other way. Suppressing every `coefficient * 0` product in the chain-rule step — treating a zero partial as contributing nothing even when its coefficient is infinite or NaN — would make a function of a *constant* at a singularity report a zero gradient: `asin(Jet{1.0})` would give `0` rather than NaN, and `pow(base, exponent)` with a non-positive constant base would recover its base-side derivative. That suppression was considered and declined. It is global by nature, rewriting the meaning of every singular and out-of-domain point at once, and it can hide a genuine NaN — a coefficient that is NaN because the function truly has no derivative there would be silenced wherever its partial happened to be zero, weakening the very invariant the paragraph above establishes. So `asin` and `acos` at a constant `±1` yield NaN partials, and `pow(Jet, Jet)` on a non-positive base needs `pow(x, double)` for a constant exponent. The poison is left visible rather than masked.

### When to Look Elsewhere

Jet is first-order forward mode, and three neighboring needs belong to other tools. The gradient of a scalar objective in high dimension is reverse mode's territory — CppAD and ADOL-C accumulate it backward in a small constant number of passes that forward mode cannot match. Second and higher derivatives belong to Boost.Math autodiff, which carries Taylor coefficients to arbitrary order. Sparse Hessians, where most second partials are structurally zero, are what the tape-based libraries are built to exploit. Choosing Jet means choosing the first-order, few-input, dependency-free corner deliberately, with these alternatives named for when the problem moves out of it.

---

## Read Next

- **User Manual - Jet** — the recipes and the production failure model: seeding, Jacobian assembly, the solver pattern, and troubleshooting by symptom.
- **Overview - Jet** — the positioning summary and the comparison table against `ceres::Jet`, Boost.Math autodiff, and the tape-based libraries.

---

*Jet.h — Fat-P Library — June 2026*
