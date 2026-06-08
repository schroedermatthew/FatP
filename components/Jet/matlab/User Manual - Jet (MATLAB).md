---
doc_id: UM-JET-002
doc_type: "User Manual"
title: "Jet (MATLAB)"
fatp_components: ["Jet"]
implementation: "MATLAB"
language: "MATLAB"
matlab_release: "R2017b or newer (uses property-size validation)"
topics: ["forward-mode automatic differentiation", "gradient", "Jacobian", "seeding", "chain rule", "solver derivatives", "fmincon", "lsqnonlin", "dual numbers"]
constraints: ["finite-difference error", "derivative maintenance", "scalar-valued jets", "value-class copy cost", "real-domain NaN"]
matlab_equivalent: "Symbolic Math Toolbox jacobian (symbolic); dlarray / dlgradient (reverse mode, Deep Learning Toolbox); complex-step differentiation"
source_files: ["Jet.m", "test_Jet.m"]
last_verified: "2026-06-07"
audience: ["MATLAB developers", "optimization", "robotics / motion planning", "AI assistants"]
status: "complete"
---

# User Manual - Jet (MATLAB)

**Scope:** How to compute exact first-order derivatives — gradients and Jacobians — with the MATLAB `Jet` class, including seeding, vector outputs, the real-domain NaN model, wiring into `fminunc` / `fmincon` / `lsqnonlin`, and the differences from the C++ `Jet<N>` this class ports.

**Not covered:** Reverse-mode differentiation, higher-order derivatives, and sparse Hessians. This class is first-order forward mode over scalar values; the closing section points to the MATLAB tools that own those axes. A vectorized variant whose value is an array is noted as a possible extension but is not part of this class.

**Prerequisites:** Working MATLAB (R2017b or newer), the chain rule, and an acquaintance with floating-point rounding. No prior exposure to automatic differentiation is assumed.

---

## User Manual Card

**Component:** Jet — MATLAB port of the Fat-P `Jet<N>`  
**Primary use case:** First-order gradient or Jacobian of a function with a modest number of inputs, evaluated alongside the function's value in one pass  
**Integration pattern:** Put `Jet.m` on the path; write your numeric code so it runs over the scalar type; seed the inputs with `Jet.vars` or `Jet.seed`; read the value with `value` and the gradient with `grad`  
**Key API:** `Jet.seed(value, k, N)`, `Jet.vars(values)`, `Jet.constant(value, N)`, the arithmetic operators, the elementary functions, and `Jet.jacobian(Y)`  
**MATLAB equivalents:** Symbolic Math Toolbox `jacobian` (symbolic, heavy); `dlarray` / `dlgradient` (reverse mode, needs Deep Learning Toolbox); complex-step differentiation (one direction at a time, breaks on `abs`/`min`/`max`)  
**Common mistakes:** Forgetting to seed (the gradient comes back all zeros); using 0-based direction indices (this class is 1-based); mixing Jets built with different `N`; expecting a complex result for `sqrt` of a negative (this class returns real NaN)  
**Performance notes:** O(N) per operation in the direction count; each operation builds a new value object and copies its partial vector, so very large `N` or hot loops pay an interpreted-language overhead the C++ version does not

---

## Why Derivatives Are Hard to Get Right

Every gradient-based method — a nonlinear solver, a least-squares fit, a trajectory optimizer — needs the derivatives of the function it works on. The function is yours to write; the derivatives are the problem.

The first instinct is to perturb: nudge each input by a small step, re-evaluate, divide the change by the step. That finite difference never returns the derivative, only an estimate sitting between two errors that move in opposite directions. Shrink the step and the truncation error falls while the roundoff error, born of subtracting two nearly equal numbers, rises. The central rule bottoms out near a step of `eps^(1/3)`, around ten good digits, and only when that step happens to suit this function at this point. Rescale an input and the right step moves; land in a flat region and the subtraction returns noise the solver then chases.

The second instinct is to differentiate by hand and code the result. That is exact, and it creates a second source of truth: every later change to the function is two edits that must agree, and one wrong sign in a partial stays invisible until a solver quietly fails to converge.

Forward-mode automatic differentiation removes the choice. You write the function once; as it evaluates, each operation carries the derivative along with the value by the chain rule, and what comes out is the value and its gradient, correct to rounding. There is no step to tune and no second formula to maintain. That is what `Jet` is.

---

## Architecture / How It Works

A Jet is a value travelling with its derivatives. Where a dual number carries one derivative beside a value, a Jet carries `N` of them, one per independent variable, so a single evaluation yields a whole gradient. In MATLAB it is a value class with two properties:

```matlab
properties
    v (1,1) double = 0           % the value
    d (1,:) double = zeros(1,0)  % the partials, 1-by-N
end
```

Each overloaded operation maps the value and propagates the partials by the rule for that operation. Multiplication is the product rule:

```matlab
function r = times(a, b)                       % overloads .*
    [av, ad, bv, bd] = Jet.binargs(a, b);
    r = Jet(av * bv, av * bd + bv * ad);
end
```

The helper `binargs` promotes a plain scalar operand to a constant Jet (value, zero partials), so `2 .* x` and `x .* 2` both work. The elementary functions are overloaded the same way: `sin(x)` returns a Jet whose value is `sin(x.v)` and whose partials are `cos(x.v) .* x.d`. Because the class is a value type, every operation returns a new Jet and nothing is mutated in place, mirroring the C++ value semantics.

Two structural points distinguish this from the C++ version. The direction count `N` is the length of `d`, a run-time property rather than a template parameter, so combining Jets built with different `N` is a checked error. And the elementary functions stay on the real line: where the underlying MATLAB function would return a complex number — `sqrt` of a negative, `log` of a negative, a negative base raised to a fractional power — the Jet returns real NaN, so a domain excursion is visible as NaN rather than silently turning the computation complex.

---

## Quick Start

```matlab
% f(x1, x2) = sin(x1) * x2 + exp(x2),  gradient at (0.7, 1.2)
X = Jet.vars([0.7, 1.2]);          % two variables, each seeded in its slot
y = sin(X(1)) .* X(2) + exp(X(2));
fprintf('f      = %.6f\n', value(y));
fprintf('grad f = [%.6f, %.6f]\n', grad(y));
```

`Jet.vars` returns a `1-by-N` array of seeded Jets — element `k` has value `values(k)` and a unit derivative in direction `k`. You then write ordinary MATLAB against `X(1)`, `X(2)`, …; the value comes back through `value`, the gradient through `grad`.

---

## Recipes

### Choosing and Seeding Your Variables

`N` is the number of inputs you differentiate against. Seed each variable in its own direction; hold everything else as a constant.

```matlab
N = 3;
x = Jet.seed(2.0, 1, N);   % direction 1
y = Jet.seed(0.5, 2, N);   % direction 2
z = Jet.seed(-1.0, 3, N);  % direction 3
```

`Jet.vars([2.0, 0.5, -1.0])` does the same in one call and is the usual choice. Direction indices are 1-based, unlike the 0-based C++ `seed`.

### The Gradient of a Scalar Function

```matlab
X = Jet.vars([1.3, -0.7, 2.1]);
f = X(1) .* X(2) + sin(X(3)) ./ X(1);
g = grad(f);    % 1-by-3: [df/dx1, df/dx2, df/dx3]
```

The gradient is `f.d`; `grad(f)` is the same thing with a scalar-shape check.

### Holding Parameters Fixed

Differentiate against the variables that move and leave the rest as constants. A constant Jet has zero partials, so it carries no sensitivity and contributes nothing to the gradient.

```matlab
N = 2;
x   = Jet.seed(xv, 1, N);
y   = Jet.seed(yv, 2, N);
rho = Jet.constant(rhoVal, N);     % a fixed parameter
f   = hypot(x, y) ./ rho;
g   = grad(f);                     % [df/dx, df/dy]; rho does not appear
```

This is the pattern for a turning radius, a regularization weight, or any quantity you want held during one differentiation and varied in another.

### Assembling a Jacobian for Vector Output

Build the output components as separate Jets over the same seeded inputs, collect them in an array, and read the Jacobian.

```matlab
X = Jet.vars([0.6, 1.8, -0.4]);
Y = [ sin(X(1)) .* X(2) + exp(X(3)), ...
      sqrt(X(2).^2 + 1) - atan2(X(3), X(1)) ];
[f, J] = Jet.jacobian(Y);   % f is 2-by-1, J is 2-by-3
```

Row `i` of `J` is the gradient of output `i`. Every output must carry the same `N`, which `Jet.jacobian` checks.

### Feeding a Gradient-Based Solver

MATLAB's solvers accept user-supplied derivatives, and a Jet evaluation supplies them exactly in one pass. For an unconstrained or constrained scalar objective, return `[f, g]` from a function that seeds the decision vector:

```matlab
function [f, g] = costAndGrad(z)
    Z = Jet.vars(z);          % seed all decision variables
    y = myCost(Z);            % cost written over the scalar type
    f = value(y);
    g = grad(y).';            % column vector for the solver
end

opts  = optimoptions('fminunc', 'SpecifyObjectiveGradient', true);
zStar = fminunc(@costAndGrad, z0, opts);
```

For a least-squares residual, return the residual vector and its Jacobian:

```matlab
function [r, J] = residualAndJac(z)
    Z = Jet.vars(z);
    R = myResiduals(Z);       % a 1-by-M Jet array
    [r, J] = Jet.jacobian(R); % r is M-by-1, J is M-by-N
end

opts  = optimoptions('lsqnonlin', 'SpecifyObjectiveGradient', true);
zStar = lsqnonlin(@residualAndJac, z0, [], [], opts);
```

There is no finite-difference callback and no separate derivative routine: the same code that returns the value returns the gradient or Jacobian, to rounding.

### Validating Against Finite Differences

Trust comes from a cross-check. `test_Jet` does this for the class itself; for your own function, compare its Jet gradient to a central difference. A cost written over the scalar type runs unchanged on a plain `double` vector — indexing `w(k)` yields a double, and the operations stay double — so the same `myCost` serves both paths:

```matlab
z0 = [0.6, 1.8];
g  = grad(myCost(Jet.vars(z0)));          % AD gradient, 1-by-N

h   = 1e-6;
gFd = zeros(1, numel(z0));
for j = 1:numel(z0)
    zp = z0; zm = z0;
    zp(j) = zp(j) + h;  zm(j) = zm(j) - h;
    gFd(j) = (myCost(zp) - myCost(zm)) / (2 * h);   % plain-double evaluations
end

fprintf('max |AD - FD| = %.2e\n', max(abs(g - gFd)));
```

Agreement to roughly `1e-8`–`1e-10` is the expected result away from domain edges and non-smooth points.

---

## Error Handling Model

The class separates two kinds of trouble.

Misuse raises an error: a direction index outside `1..N` (`Jet:badDirection`), combining Jets whose partial counts differ (`Jet:dimMismatch`), combining a Jet with a non-scalar (`Jet:nonScalar`), or calling the constructor with one argument (`Jet:ctor`).

A domain excursion returns NaN rather than throwing. `sqrt` and `log` of a negative value, and `asin` and `acos` of an argument outside `[-1, 1]`, return a Jet whose value and partials are both NaN. This keeps the result on the real line and makes the excursion visible: check the value before trusting the gradient. At the singular boundaries two cases differ. Where the value itself is unbounded — `log` at zero — the value is `-Inf` and the partial is `+Inf`. Where the value is finite but the slope is unbounded — `asin(1)`, `acos(±1)` — the value is ordinary (`pi/2`, `0`) while the derivative coefficient is infinite, so a seeded direction carries `±Inf` and any direction the variable does not enter carries NaN through `Inf * 0`; read the value, confirm you are sitting on the edge, and treat that gradient as unusable. By contrast `sqrt` at zero, and `x.^p` for `0 < p < 1` at zero, return a derivative of `0` by a non-poisoning convention — a single singular point does not poison an otherwise finite gradient — so `sqrt(x)` and `x.^0.5` agree everywhere. Power has two contracts. For `x.^p` with a constant exponent the value and base derivative follow real power behavior wherever the real result is defined — a negative base with an integer exponent stays real and differentiates normally (`(-2).^2` gives value `4`, derivative `-4`), while a negative base with a non-integer constant exponent is off the real line and returns NaN value and NaN partials. The exponent-differentiated forms — `x.^y` and `s.^y` with a Jet exponent `y` — require a positive base for the gradient: on a non-positive base they preserve the real value where it is defined but return NaN partials under that positive-base contract. The contract `x.^0 = 1` is honored with zero partials, so a zero exponent does not produce `0 * Inf`.

The overflow guards are inherited from the C++ version: `atan` of a very large argument and `atan2` at very large magnitudes compute the derivative through a reciprocal so the intermediate `x*x` or `x*x + y*y` cannot overflow and round a representable small derivative down to zero, and `hypot` uses MATLAB's overflow-safe built-in for both the value and the denominator. Division mirrors the C++ quotient policy: a structurally-zero direction is guarded under value overflow, but if the quotient value itself overflows while an active denominator partial is nonzero, a finite true derivative may still be returned as infinite.

---

## Performance Rules of Thumb

Each operation is O(N) in the direction count: it touches the whole partial vector. Set `N` to the number of variables you actually differentiate against and no larger — an unused direction is a column of zeros carried through every operation.

Unlike the C++ version, every operation here constructs a new value object and copies its `1-by-N` partials, so a tight inner loop over many evaluations pays an interpreted-language and allocation overhead. For a one-shot gradient or a solver that evaluates a modest number of times, this is immaterial. For a scalar objective in high dimension, forward mode is the wrong asymptotics regardless of language — its cost grows with the number of inputs — and reverse mode (`dlgradient`) is the better tool. For repeated evaluation of the same modest-`N` function at many points, the vectorized variant noted below, or dropping to the C++ `Jet`, removes the per-call object overhead.

---

## When to Use / When Not To

Use this class for a first-order gradient or Jacobian of a function with a modest number of inputs, for prototyping a differentiation before committing it to C++, and for cross-checking the C++ `Jet` from MATLAB. It needs no toolbox and no symbolic setup, and it returns exact derivatives.

Look elsewhere for a scalar objective in very high dimension (reverse mode is asymptotically cheaper), for higher-order derivatives or Hessians (this is first order), for a hot loop where the per-operation object cost dominates (vectorize, or use C++), and for code that must stay complex-valued through `sqrt` or `log` of negatives (the real-NaN contract is the opposite of what you want there).

---

## Migration Guide

### From Finite Differences

Delete the step and the perturbation loop. Where you re-evaluated the function at `z ± h` and divided, seed the inputs once and read `grad` or `Jet.jacobian`. The result is exact and single-pass, and the step-size question disappears. Keep a finite-difference check in your tests, not in your computation.

### From Hand-Written Derivatives

Remove the second routine. The function written over the scalar type is now the single source of truth; the gradient is whatever the function does, with no separate formula to keep in agreement.

### From the C++ Jet

The semantics match, with three adjustments to remember. Seeding is 1-based here (`Jet.seed(value, k, N)` with `k` from `1` to `N`), the partial count `N` is a run-time argument rather than a template parameter, and out-of-domain `sqrt`/`log` return real NaN where C++ also returns NaN but MATLAB's bare functions would return complex; negative-base powers follow the same contract as C++ — a non-integer constant exponent gives real NaN, while an exponent-differentiated form (a Jet exponent) preserves the real value where defined but returns NaN partials. To confirm parity, run the same computation both ways; for example the single-segment Dubins gradient gives `dL/dx1 = 0.7071…` in both.

### From MATLAB's Other Options

The Symbolic Math Toolbox `jacobian` differentiates symbolically, which is exact but builds and simplifies expressions and is heavy inside a solver loop. `dlarray` with `dlgradient` is reverse-mode AD and is the right choice for high-dimensional scalar objectives, but needs the Deep Learning Toolbox and a `dlfeval` wrapper. Complex-step differentiation gives one directional derivative per evaluation at machine precision but breaks on `abs`, `min`, `max`, and any operation that is not analytic. `Jet` sits between these: dependency-free, exact, multi-direction in one pass, at O(N) cost in the input count.

---

## Troubleshooting

**The gradient is all zeros.** The inputs were not seeded, or they were built with `Jet.constant`. Use `Jet.vars` or `Jet.seed` for variables.

**"partial-count mismatch".** Two Jets built with different `N` met in one expression. Seed every variable of one computation with the same `N`, usually via a single `Jet.vars` call.

**"a Jet may only combine with a scalar".** An operation paired a Jet with a non-scalar array. This class is scalar-valued; combine Jets with scalars, and assemble arrays of results with `[ ... ]` and `Jet.jacobian`.

**The result is complex.** A function that is not overloaded was reached, or a value left the class as a plain double and went complex through a bare `sqrt`/`log`. Keep the computation in overloaded operations; out-of-domain points return real NaN by design.

**The gradient is NaN.** A domain edge, or a power off the positive-base contract. Check `value` at the point: `sqrt`/`log` of a negative and `asin`/`acos` outside `[-1, 1]` make both value and gradient NaN; a non-integer constant power of a negative base does the same; a Jet exponent on a non-positive base keeps the real value but makes the gradient NaN.

**"Undefined function 'foo' for input arguments of type 'Jet'".** That function is not overloaded. The supported set is listed in the API reference; for anything outside it, express the computation in terms of the supported functions or add the overload following the same pattern.

---

## API Reference

**Construction**
- `Jet(value, partials)` — value is a real scalar, partials a `1-by-N` real row vector.
- `Jet.constant(value, N)` — a constant: the value with `N` zero partials.
- `Jet.seed(value, k, N)` — a variable: the value with a unit derivative in direction `k` (1-based).
- `Jet.vars(values)` — a `1-by-N` Jet array from `N` input values, each seeded in its own direction.

**Readout**
- `value(j)` or `j.v` — the value (an array of values for a Jet array).
- `grad(j)` or `j.d` — the `1-by-N` gradient of a scalar Jet.
- `[f, J] = Jet.jacobian(Y)` — values `f` (`M-by-1`) and Jacobian `J` (`M-by-N`) of a Jet array `Y`.

**Operators** — `+`, `-` (binary and unary), `*` and `.*`, `/` and `./`, `^` and `.^`. Comparisons `<`, `<=`, `>`, `>=`, `==`, `~=` compare values only.

**Elementary functions** — `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `exp`, `log`, `sqrt`, `abs`, `hypot`, `atan2`. The power `.^` covers Jet base with constant exponent, constant base with Jet exponent, and Jet base with Jet exponent.

**Errors** — `Jet:badDirection`, `Jet:dimMismatch`, `Jet:nonScalar`, `Jet:ctor`.

**Real-domain NaN** — value and partials are both NaN for `sqrt`/`log` of a negative, `asin`/`acos` outside `[-1, 1]`, and a non-integer constant power of a negative base. The exponent-differentiated forms (`x.^y`, `s.^y` with a Jet exponent) keep the real value where defined but return NaN partials on a non-positive base. The contract `x.^0 = 1` carries zero partials.

---

## FAQ

**Why is seeding 1-based when the C++ version is 0-based?** MATLAB indexes from 1, and `Jet.seed`/`Jet.vars` follow the host language so a direction index lines up with a column of the gradient and with `X(k)`.

**Why real NaN instead of MATLAB's complex result?** A Jet is a real automatic-differentiation scalar. Returning NaN at a domain excursion keeps the computation real and makes the excursion visible; a silent switch to complex would propagate into every later operation and into the gradient.

**Can the value be a vector or matrix?** Not in this class — the value is a scalar and the partials are a `1-by-N` row. Differentiate vector outputs by assembling an array of result Jets and calling `Jet.jacobian`. A variant whose value is an array, with partials along a trailing dimension, is a possible extension.

**How do I differentiate against only some inputs?** Seed those inputs with `Jet.seed` (or `Jet.vars`) and build the rest with `Jet.constant`. Constants carry zero partials and drop out of the gradient.

**Which functions can I call on a Jet?** The elementary set in the API reference, plus the arithmetic and comparison operators. Anything outside it must be written in terms of the supported functions, or added as an overload.

**How does this relate to the C++ Jet?** It is a port of the same design and uses the same derivative formulas and guards. See `OV-JET-001` / `UM-JET-001` / `CG-JET-001` for the C++ component, and `EX-JET-001` / `EX-JET-002` for the Dubins examples whose gradients this class reproduces.
