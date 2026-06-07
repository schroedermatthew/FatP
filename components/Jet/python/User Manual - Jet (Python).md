---
doc_id: UM-JET-003
doc_type: "User Manual"
title: "Jet (Python)"
fatp_components: ["Jet"]
implementation: "Python"
language: "Python"
python_version: "3.8 or newer"
topics: ["forward-mode automatic differentiation", "gradient", "Jacobian", "seeding", "chain rule", "solver derivatives", "scipy.optimize", "batched gradients", "dual numbers"]
constraints: ["finite-difference error", "derivative maintenance", "scalar-valued jets", "per-operation tuple rebuild", "real-domain NaN"]
python_equivalent: "autograd / JAX (tracing reverse/forward AD); SymPy (symbolic); numdifftools (finite differences)"
source_files: ["jet.py", "test_jet.py", "vjet.py", "test_vjet.py", "dubins_jet.py"]
last_verified: "2026-06-06"
audience: ["Python developers", "optimization", "robotics / motion planning", "AI assistants"]
status: "draft"
---

# User Manual - Jet (Python)

**Scope:** How to compute exact first-order derivatives — gradients and Jacobians — with the Python `Jet` class (`jet.py`), including seeding, vector outputs, the real-domain NaN model, wiring into `scipy.optimize`, the NumPy-vectorized variant for many points at once (`vjet.py`), and the differences from the C++ `Jet<N>` this class ports.

**Not covered:** Reverse-mode differentiation, higher-order derivatives, and sparse Hessians. This class is first-order forward mode; the closing section points to the Python tools that own those axes.

**Prerequisites:** Working Python (3.8 or newer), the chain rule, and an acquaintance with floating-point rounding. `jet.py` depends only on the standard library; the vectorized `vjet.py` and the solver recipe use NumPy and SciPy respectively. No prior exposure to automatic differentiation is assumed.

---

## User Manual Card

**Component:** Jet — Python port of the Fat-P `Jet<N>`  
**Primary use case:** First-order gradient or Jacobian of a function with a modest number of inputs, evaluated alongside the function's value in one pass  
**Integration pattern:** `from jet import Jet, sin, cos, sqrt, ...`; write your numeric code so it runs over the scalar type; seed the inputs with `Jet.variables` or `Jet.seed`; read `.value` and `.partials`  
**Key API:** `Jet.seed(value, k, n)`, `Jet.variables(values)`, `Jet.constant(value, n)`, the arithmetic operators, the module-level elementary functions, and `Jet.jacobian(outputs)`  
**Python equivalents:** `autograd` / JAX (tracing AD, array-first, JAX adds JIT and a NumPy surface); SymPy (symbolic differentiation); `numdifftools` (finite differences); the complex-step trick (one direction, breaks on `abs`)  
**Common mistakes:** Forgetting to seed (the gradient comes back all zeros); mixing Jets built with different `n`; expecting a complex result for `sqrt` of a negative (this class returns real NaN); reaching for a function that is not overloaded (only the listed elementary set is)  
**Performance notes:** O(N) per operation in the direction count, and each operation rebuilds an immutable partials tuple, so very large `N` or hot loops pay a pure-Python overhead; for many evaluation points use the vectorized `vjet`, and for high-dimensional scalar objectives use reverse mode

---

## Why Derivatives Are Hard to Get Right

Every gradient-based method — a nonlinear solver, a least-squares fit, a trajectory optimizer — needs the derivatives of the function it works on. The function is yours to write; the derivatives are the problem.

The first instinct is to perturb: nudge each input by a small step, re-evaluate, divide the change by the step. That finite difference never returns the derivative, only an estimate sitting between two errors that move in opposite directions. Shrink the step and the truncation error falls while the roundoff error, born of subtracting two nearly equal numbers, rises. The central rule bottoms out near a step of `eps**(1/3)`, around ten good digits, and only when that step happens to suit this function at this point. Rescale an input and the right step moves; land in a flat region and the subtraction returns noise the solver then chases.

The second instinct is to differentiate by hand and code the result. That is exact, and it creates a second source of truth: every later change to the function is two edits that must agree, and one wrong sign in a partial stays invisible until a solver quietly fails to converge.

Forward-mode automatic differentiation removes the choice. You write the function once; as it evaluates, each operation carries the derivative along with the value by the chain rule, and what comes out is the value and its gradient, correct to rounding. There is no step to tune and no second formula to maintain. That is what `Jet` is.

---

## Architecture / How It Works

A Jet is a value travelling with its derivatives. Where a dual number carries one derivative beside a value, a Jet carries `N` of them, one per independent variable, so a single evaluation yields a whole gradient. In Python it is a small value type:

```python
class Jet:
    __slots__ = ("value", "partials")   # value: float, partials: tuple of N floats
```

Each operation maps the value and propagates the partials by the rule for that operation. The operators are the dunder methods; multiplication is the product rule:

```python
def __mul__(self, other):
    av, ad, bv, bd, _ = _binary_parts(self, other)
    return Jet(av * bv, tuple(av * y + bv * x for x, y in zip(ad, bd)))
```

The helper `_binary_parts` promotes a plain number to a constant Jet (value, zero partials), so `2 * x` and `x * 2` both work (the latter through `__rmul__`). The elementary functions are module-level and dispatch on the argument type: `sin(x)` returns a Jet when `x` is a Jet and falls through to `math.sin` when `x` is a float, so the same generic code runs on Jets and on plain numbers. Because the partials are an immutable tuple and every operation returns a new Jet, the type has value semantics, mirroring the C++ version.

Two points distinguish this from the C++ version. The direction count `N` is the length of `partials`, a run-time property rather than a template parameter, so combining Jets built with different `N` is a checked error. And the elementary functions stay on the real line: where the bare `math` function would raise — `sqrt`, `log`, `asin`, `acos` outside their domain — or where `**` would return a complex number — a negative base to a fractional power — the Jet returns real NaN, so a domain excursion is visible as NaN rather than an exception or a silent switch to complex.

---

## Quick Start

```python
from jet import Jet, sin, exp

# f(x1, x2) = sin(x1) * x2 + exp(x2),  gradient at (0.7, 1.2)
x1, x2 = Jet.variables([0.7, 1.2])     # two variables, each seeded in its slot
y = sin(x1) * x2 + exp(x2)
print(y.value)        # 4.093178...
print(y.partials)     # (0.917811..., 3.964335...)  == (df/dx1, df/dx2)
```

`Jet.variables` returns a tuple of seeded Jets — element `k` has value `values[k]` and a unit derivative in direction `k`. You then write ordinary Python against `x1`, `x2`, …; the value is `y.value`, the gradient `y.partials`.

---

## Recipes

### Choosing and Seeding Your Variables

`N` is the number of inputs you differentiate against. Seed each variable in its own direction; hold everything else as a constant. Direction indices are 0-based, matching the C++ version (the MATLAB port is 1-based).

```python
n = 3
x = Jet.seed(2.0, 0, n)    # direction 0
y = Jet.seed(0.5, 1, n)    # direction 1
z = Jet.seed(-1.0, 2, n)   # direction 2
```

`Jet.variables([2.0, 0.5, -1.0])` does the same in one call and is the usual choice.

### The Gradient of a Scalar Function

```python
from jet import Jet, sin
x1, x2, x3 = Jet.variables([1.3, -0.7, 2.1])
f = x1 * x2 + sin(x3) / x1
g = f.partials    # (df/dx1, df/dx2, df/dx3)
```

### Holding Parameters Fixed

Differentiate against the variables that move and leave the rest as constants. A constant Jet has zero partials, so it contributes nothing to the gradient.

```python
from jet import Jet, hypot
n = 2
x = Jet.seed(xv, 0, n)
y = Jet.seed(yv, 1, n)
rho = Jet.constant(rho_val, n)     # a fixed parameter
f = hypot(x, y) / rho
g = f.partials                     # (df/dx, df/dy); rho does not appear
```

This is the pattern for a turning radius, a regularization weight, or any quantity held during one differentiation and varied in another.

### Assembling a Jacobian for Vector Output

Build the output components over the same seeded inputs, collect them in a list, and read the Jacobian.

```python
from jet import Jet, sin, exp, sqrt, atan2
x1, x2, x3 = Jet.variables([0.6, 1.8, -0.4])
outputs = [sin(x1) * x2 + exp(x3),
           sqrt(x2 ** 2 + 1) - atan2(x3, x1)]
values, J = Jet.jacobian(outputs)   # values: length 2, J: 2-by-3 (list of lists)
```

Row `i` of `J` is the gradient of output `i`; every output must carry the same `N`, which `Jet.jacobian` checks.

### Feeding a Gradient-Based Solver

SciPy's optimizers take user-supplied derivatives, and a Jet evaluation supplies them exactly in one pass. For a scalar objective, return `(f, grad)` from a function that seeds the decision vector:

```python
from scipy.optimize import minimize
from jet import Jet

def cost_and_grad(z):
    Z = Jet.variables(z)            # seed all decision variables
    y = my_cost(Z)                  # cost written over the scalar type
    return y.value, list(y.partials)

res = minimize(cost_and_grad, z0, jac=True, method="BFGS")
```

For a least-squares residual, give `least_squares` the residual vector and an exact Jacobian:

```python
from scipy.optimize import least_squares
from jet import Jet

def residuals(z):
    return [r.value for r in my_residuals(Jet.variables(z))]

def jacobian(z):
    _, J = Jet.jacobian(my_residuals(Jet.variables(z)))
    return J

res = least_squares(residuals, z0, jac=jacobian)
```

There is no finite-difference callback and no separate derivative routine: the same code that returns the value returns the gradient or Jacobian, to rounding.

### Differentiating at Many Points at Once

When you need the gradient at a whole batch or grid of points, the vectorized variant in `vjet.py` does it in one pass. A `VJet` holds an array of values of shape `S` with partials of shape `S + (N,)`; seed `N` input arrays and read the per-point gradient. This variant uses NumPy.

```python
import numpy as np
from vjet import VJet, exp, sin

x = np.linspace(0.0, 1.0, 1000)
(X,) = VJet.variables([x])          # one variable, 1000 points
y = exp(X) * sin(X)
# y.value has shape (1000,), y.partials has shape (1000, 1)
```

The derivative formulas and guards match the scalar class; the per-point partials equal the scalar result point by point.

### Validating Against Finite Differences

Trust comes from a cross-check. `test_jet.py` does this for the class; for your own function, compare its Jet gradient to a central difference. A cost written over the scalar type runs unchanged on plain floats — indexing a float sequence yields floats — so the same `my_cost` serves both paths:

```python
import math
from jet import Jet

z0 = [0.6, 1.8]
g = my_cost(Jet.variables(z0)).partials   # AD gradient

h = 1e-6
gfd = []
for j in range(len(z0)):
    zp, zm = list(z0), list(z0)
    zp[j] += h
    zm[j] -= h
    gfd.append((my_cost(zp) - my_cost(zm)) / (2 * h))

print(max(abs(a - b) for a, b in zip(g, gfd)))   # ~1e-8 to 1e-10
```

---

## Error Handling Model

The class separates two kinds of trouble.

Misuse raises: a direction index outside `0..N-1` or combining Jets whose partial counts differ both raise `ValueError`, and calling the constructor with one argument is an error.

A domain excursion returns NaN rather than raising. `sqrt` and `log` of a negative value, `asin` and `acos` of an argument outside `[-1, 1]`, and a non-positive base for `x ** y` or a constant base raised to a Jet all return a Jet whose value and partials are NaN. This keeps the result on the real line — where the bare `math` functions would raise `ValueError` and `(-2.0) ** 0.5` would return a complex number — and makes the excursion visible: check the value before trusting the gradient. At the edges where a derivative is genuinely infinite — `sqrt` at zero, `asin`/`acos` at `±1`, `log` at zero — the value is finite or `-inf` and the partials carry `inf`, with `0 * inf` appearing as NaN in any direction the variable does not enter. The contract `x ** 0 == 1` is honored with zero partials. The power routine uses `math.pow` so that an integer power of a negative base stays real rather than turning complex.

The overflow guards are inherited from the C++ version: `atan` of a very large argument and `atan2` at very large magnitudes compute the derivative through a reciprocal so the intermediate `x*x` or `x*x + y*y` cannot overflow and round a representable small derivative down to zero, and `hypot` uses `math.hypot` for both the value and the denominator.

---

## Performance Rules of Thumb

Each operation is O(N) in the direction count: it builds a new partials tuple of length `N`. Set `N` to the number of variables you actually differentiate against and no larger — an unused direction is a slot of zeros carried through every operation.

Pure Python adds per-operation overhead the C++ version does not have: each elementary call and operator allocates a tuple and runs a comprehension. For a one-shot gradient or a solver that evaluates a modest number of times, this is immaterial. For many evaluation points, switch to the vectorized `vjet`, which moves the per-point loop into NumPy and computes the whole batch in one pass. For a scalar objective in high dimension, forward mode is the wrong asymptotics regardless of these details — its cost grows with the number of inputs — and reverse mode (`autograd`, JAX) is the better tool.

---

## When to Use / When Not To

Use this class for a first-order gradient or Jacobian of a function with a modest number of inputs, for prototyping a differentiation, and for cross-checking the C++ or MATLAB `Jet` from Python. It depends only on the standard library and returns exact derivatives. Use the vectorized `vjet` when you need the same over many points at once.

Look elsewhere for a scalar objective in very high dimension (reverse mode is asymptotically cheaper), for higher-order derivatives or Hessians (this is first order), for a hot scalar loop where the per-operation tuple cost dominates (vectorize with `vjet`, or use C++), and for code that must stay complex-valued through `sqrt` or `log` of negatives (the real-NaN contract is the opposite of what you want there).

---

## Migration Guide

### From Finite Differences

Delete the step and the perturbation loop. Where you re-evaluated the function at `z ± h` and divided, seed the inputs once and read `.partials` or `Jet.jacobian`. The result is exact and single-pass, and the step-size question disappears. Keep a finite-difference check in your tests, not in your computation.

### From Hand-Written Derivatives

Remove the second routine. The function written over the scalar type is now the single source of truth; the gradient is whatever the function does, with no separate formula to keep in agreement.

### From the C++ or MATLAB Jet

The semantics match. Seeding is 0-based here, as in C++ (MATLAB's port is 1-based); the partial count `N` is a run-time argument rather than a template parameter; and out-of-domain `sqrt`/`log` and negative-base powers return real NaN. To confirm parity, run the same computation across implementations; `dubins_jet.py` does this for the single-segment Dubins gradient and reproduces the C++ value `dL/dx1 = 0.7071…` to rounding.

### From Python's Other Options

`autograd` and JAX trace your code to differentiate it, are array-first, and are the right choice for high-dimensional or array-heavy objectives — JAX adds JIT compilation and GPU support, at the cost of a framework and a NumPy-shaped surface. SymPy differentiates symbolically, which is exact but builds and simplifies expressions and is heavy inside a solver loop. `numdifftools` is finite differences with the error that implies. `Jet` sits apart from these: dependency-free, exact, multi-direction in one pass, at O(N) cost in the input count, and small enough to read in full.

---

## Troubleshooting

**The gradient is all zeros.** The inputs were not seeded, or they were built with `Jet.constant`. Use `Jet.variables` or `Jet.seed` for variables.

**`ValueError: partial-count mismatch`.** Two Jets built with different `N` met in one expression. Seed every variable of one computation with the same `N`, usually via a single `Jet.variables` call.

**The result is complex, or a bare function raised.** A value left the class as a plain float and went through `math.sqrt`/`log` or `**` directly. Keep the computation in the overloaded operators and the module-level functions; out-of-domain points return real NaN by design.

**The gradient is NaN.** A domain edge or a non-positive base. Check `.value` at the point: `sqrt`/`log` of a negative, `asin`/`acos` outside `[-1, 1]`, or a non-positive base for a power all return NaN gradients.

**`TypeError: unhashable type: 'Jet'`.** Jets compare by value and are intentionally not hashable, so they cannot be dict keys or set members. Use their `.value` if you need a key.

**`AttributeError` or a function that ignored the Jet.** That function is not overloaded. The supported set is in the API reference; for anything outside it, express the computation in terms of the supported functions, or add it following the same dispatch pattern.

---

## API Reference

**Construction**
- `Jet(value, partials)` — value a real scalar, partials a sequence of `N` reals.
- `Jet.constant(value, n)` — a constant: the value with `n` zero partials.
- `Jet.seed(value, k, n)` — a variable: the value with a unit derivative in direction `k` (0-based).
- `Jet.variables(values)` — a tuple of `N` seeded Jets, one per value, each in its own direction.

**Readout**
- `j.value` — the value; `j.partials` (or `j.gradient`) — the `N`-tuple of partials.
- `Jet.jacobian(outputs)` — `(values, J)` where `values` is length `M` and `J` is an `M`-by-`N` list of lists.

**Operators** — `+`, `-` (binary and unary), `*`, `/`, `**`, `abs(...)`. Comparisons `<`, `<=`, `>`, `>=`, `==`, `!=` compare values only; Jets are not hashable.

**Elementary functions** (module level, dispatch over `Jet`/`float`) — `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `sqrt`, `hypot`, `atan2`. The `**` operator covers Jet base with constant exponent, constant base with Jet exponent, and Jet base with Jet exponent.

**Real-domain NaN** — returned (value and partials) by `sqrt`/`log` of a negative, `asin`/`acos` outside `[-1, 1]`, and a non-positive base for a power. The contract `x ** 0 == 1` carries zero partials.

**Vectorized variant (`vjet.py`, requires NumPy)** — `VJet` with the same surface; value of shape `S`, partials of shape `S + (N,)`; `VJet.seed` / `VJet.variables` / `VJet.constant`; `VJet.jacobian(outputs)` returns values of shape `S + (M,)` and a per-point Jacobian of shape `S + (M, N)`.

---

## FAQ

**Why is seeding 0-based here when the MATLAB port is 1-based?** Each host language follows its own convention: Python and C++ index from 0, MATLAB from 1. A direction index lines up with a position in `partials` and with the order of `Jet.variables`.

**Why real NaN instead of an exception or a complex result?** A Jet is a real automatic-differentiation scalar. Returning NaN at a domain excursion keeps the computation real and visible; raising would interrupt a sweep, and a silent switch to complex would propagate into the gradient.

**Can the value be an array?** Not in `Jet` — the value is a scalar and the partials are an `N`-tuple. For arrays of points, use `VJet` in `vjet.py`, whose value is a NumPy array and whose partials carry a trailing direction axis. Differentiate vector outputs with `Jet.jacobian`.

**How do I differentiate against only some inputs?** Seed those inputs with `Jet.seed` (or `Jet.variables`) and build the rest with `Jet.constant`. Constants carry zero partials and drop out of the gradient.

**Which functions can I call on a Jet?** The elementary set in the API reference, plus the arithmetic and comparison operators. Anything outside it must be written in terms of the supported functions, or added as a dispatching function.

**How does this relate to the C++ and MATLAB Jets?** It is a port of the same design and uses the same derivative formulas and guards; `dubins_jet.py` reproduces the C++ Dubins gradient as a cross-check. See `OV-JET-001` / `UM-JET-001` / `CG-JET-001` for the C++ component and `UM-JET-002` for the MATLAB port.
