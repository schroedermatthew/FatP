# When to Use Jet

## Purpose

`Jet` is a fixed-size forward-mode automatic differentiation scalar. It is meant for code that already looks like ordinary scalar numeric code, but where you also need first derivatives.

A `Jet<N>` carries two things at the same time:

```text
value      the ordinary scalar value of the expression
partials   N first derivatives of that value with respect to N chosen inputs
```

So if a normal calculation produces:

```text
y = f(x0, x1, x2)
```

then evaluating the same calculation with `Jet<3>` can produce:

```text
y.value
∂y/∂x0
∂y/∂x1
∂y/∂x2
```

Use `Jet` when you want the derivative of a smooth numeric formula without hand-writing derivative code and without relying on finite-difference step sizes.

---

## The short rule

Use `Jet` when the problem is:

```text
small or moderate input dimension
first derivatives only
mostly smooth scalar numeric formulas
deterministic
branch behavior is understood
```

Avoid `Jet` when the problem is:

```text
huge input dimension with one scalar loss
discrete, combinatorial, or graph-like
mostly sorting, indexing, hashing, or lookup logic
nondifferentiable at the point of interest
stochastic or nondeterministic
primarily second derivatives or higher derivatives
```

The most important practical rule is:

```text
Jet is for differentiating numeric kernels, not whole programs.
```

A good target is a formula, residual, transform, geometry calculation, or local model equation. A bad target is an entire planner, solver, simulator, state machine, parser, or application.

---

# Core definitions

## Scalar

A scalar is a single numeric value, such as a `double`.

Examples:

```cpp
3.0
x
sin(theta)
sqrt(x * x + y * y)
```

A scalar is not an array, vector, matrix, string, object graph, or container. A `Jet<N>` behaves like a scalar in formulas, but it carries derivative information along with the scalar value.

---

## Function

In this document, a function means a mathematical mapping from input numbers to output numbers.

A scalar-output function looks like:

```text
f : R^N -> R
```

That means `N` real-valued inputs and one real-valued output.

Example:

```text
f(x, y) = sqrt(x^2 + y^2)
```

A vector-output function looks like:

```text
F : R^N -> R^M
```

That means `N` real-valued inputs and `M` real-valued outputs.

Example:

```text
F(x, y) = [
  sqrt(x^2 + y^2),
  atan2(y, x)
]
```

`Jet` can be used for either shape.

---

## Independent variable

An independent variable is an input you want derivatives with respect to.

For:

```text
f(x, y, z)
```

if you care about all three derivatives, then `x`, `y`, and `z` are independent variables.

In C++, that usually means using:

```cpp
using J = Jet<3>;

J x = J::seed<0>(x_value);
J y = J::seed<1>(y_value);
J z = J::seed<2>(z_value);
```

The seed direction says which derivative slot belongs to which input.

---

## Seed

A seed creates an independent variable.

For example:

```cpp
using J = Jet<2>;

J x = J::seed<0>(3.0);
J y = J::seed<1>(4.0);
```

Conceptually this means:

```text
x.value = 3.0
x.partials = [1, 0]

y.value = 4.0
y.partials = [0, 1]
```

The first partial slot tracks sensitivity to `x`. The second partial slot tracks sensitivity to `y`.

If a value is constant, construct it as a constant Jet instead of seeding it:

```cpp
J c{10.0};
```

Conceptually:

```text
c.value = 10.0
c.partials = [0, 0]
```

A constant affects the value of the expression, but it does not introduce a new derivative direction.

---

## Derivative

A derivative measures local sensitivity.

For a one-variable function:

```text
f(x) = x^2
```

The derivative is:

```text
df/dx = 2x
```

At `x = 3`:

```text
f(3) = 9
df/dx = 6
```

The derivative says that near `x = 3`, a small change in `x` changes `f` about six times as much.

---

## Partial derivative

A partial derivative measures sensitivity with respect to one input while treating the other inputs as fixed.

For:

```text
f(x, y) = x^2 + 3y
```

The partial derivatives are:

```text
∂f/∂x = 2x
∂f/∂y = 3
```

At `x = 2`, `y = 10`:

```text
f = 34
∂f/∂x = 4
∂f/∂y = 3
```

A `Jet<2>` result would carry:

```text
value = 34
partials = [4, 3]
```

---

## Gradient

A gradient is the vector of all partial derivatives for a scalar-output function.

For:

```text
f : R^N -> R
```

The gradient is:

```text
∇f = [∂f/∂x0, ∂f/∂x1, ..., ∂f/∂xN-1]
```

A single `Jet<N>` scalar output gives you the gradient directly in its partials.

Example:

```text
f(x, y) = x^2 + sin(y)
```

At `x = 3`, `y = 0`:

```text
f = 9
∂f/∂x = 6
∂f/∂y = cos(0) = 1

gradient = [6, 1]
```

---

## Jacobian

A Jacobian is the matrix of first derivatives for a vector-output function.

For:

```text
F : R^N -> R^M
```

The Jacobian has `M` rows and `N` columns:

```text
J[i, j] = ∂F_i / ∂x_j
```

Example:

```text
F(x, y) = [
  x + y,
  x * y,
  atan2(y, x)
]
```

The Jacobian is:

```text
[ ∂F0/∂x  ∂F0/∂y ]
[ ∂F1/∂x  ∂F1/∂y ]
[ ∂F2/∂x  ∂F2/∂y ]
```

With `Jet<2>`, each output is a `Jet<2>`. Each output's partial array becomes one row of the Jacobian.

---

## Forward-mode automatic differentiation

Forward-mode AD propagates derivatives in the same direction as the computation.

If the program computes:

```text
a = x * y
b = sin(a)
c = b + x
```

then forward mode carries both values and derivatives through each step:

```text
x -> a -> b -> c
```

Every intermediate value carries:

```text
ordinary value
partial derivatives with respect to the seeded inputs
```

This is what `Jet` does.

Forward mode is best when the number of independent inputs is small or moderate.

---

## Reverse-mode automatic differentiation

Reverse-mode AD runs the computation forward first, then walks backward from the output to compute sensitivities of that output with respect to earlier values.

It is the general form of backpropagation.

Reverse mode is best for functions shaped like:

```text
f : R^N -> R
```

where `N` is very large and the output is one scalar loss.

Examples:

```text
neural-network training
large scalar objective with millions of parameters
machine-learning loss functions
```

Forward mode and reverse mode are both automatic differentiation. They differ in cost shape.

Rule of thumb:

```text
few inputs, many outputs:       forward mode
many inputs, one/few outputs:   reverse mode
small fixed parameter blocks:   Jet is a good fit
huge parameter vectors:         reverse mode is usually better
```

---

## Smooth scalar numeric formula

A smooth scalar numeric formula is a formula made mostly from ordinary arithmetic and standard math functions, without discontinuous decisions at the point being differentiated.

Examples of operations that fit well:

```text
+  -  *  /
sin  cos  tan
asin  acos  atan  atan2
exp  log
sqrt  abs  pow
hypot
```

Examples:

```cpp
sqrt(x * x + y * y)
```

```cpp
atan2(y, x) + r * cos(theta)
```

```cpp
exp(-0.5 * e * e) / sigma
```

This does not mean the entire program must be simple. It means the part you differentiate should be a smooth numeric formula.

Bad targets include:

```text
sorting
hash table lookup
graph search
integer indexing
random sampling
file I/O
string processing
state machines
discrete mode selection
```

Those may exist around the formula, but `Jet` should be applied to the differentiable numeric kernel.

---

## Active branch

If code branches, `Jet` follows the branch selected by the ordinary value.

Example:

```cpp
if (x > 0.0) {
    return x * x;
} else {
    return -x;
}
```

For `x = 2`, the active branch is:

```cpp
return x * x;
```

For `x = -2`, the active branch is:

```cpp
return -x;
```

`Jet` differentiates the branch that actually runs.

This is usually fine away from the branch boundary. At the boundary, the function may be nondifferentiable.

For the example above, at `x = 0`, the derivative from the left and the derivative from the right do not agree. `Jet` will still return the derivative of whichever branch executes, but that does not make the mathematical function smooth at zero.

---

## Domain

A domain is the set of inputs where a function has real-valued meaning.

Examples:

```text
sqrt(x)    real-valued for x >= 0
log(x)     real-valued for x > 0
asin(x)    real-valued for -1 <= x <= 1
acos(x)    real-valued for -1 <= x <= 1
```

If you evaluate outside the real-valued domain, the value and derivative may become `NaN` or infinite.

This is not a Jet-specific problem. It is a property of the mathematical function.

---

# What Jet is good for

## 1. Gradients of scalar objective functions

Use `Jet` when you have:

```text
cost(parameters) -> scalar
```

and you need:

```text
cost value
gradient of cost with respect to parameters
```

Examples:

```text
least-squares cost
trajectory cost
control effort
energy function
penalty function
calibration loss
geometry error metric
```

### Example: two-parameter objective

Suppose:

```text
cost(x, y) = (x - 3)^2 + 10(y + 1)^2
```

C++ shape:

```cpp
#include "fat_p/Jet.h"

using fat_p::autodiff::Jet;

template <class Scalar>
Scalar cost(Scalar x, Scalar y)
{
    const Scalar dx = x - 3.0;
    const Scalar dy = y + 1.0;
    return dx * dx + 10.0 * dy * dy;
}

void example()
{
    using J = Jet<2>;

    J x = J::seed<0>(5.0);
    J y = J::seed<1>(2.0);

    J c = cost(x, y);

    // c.mValue       = cost value
    // c.mPartials[0] = ∂cost/∂x
    // c.mPartials[1] = ∂cost/∂y
}
```

At `x = 5`, `y = 2`:

```text
cost = (5 - 3)^2 + 10(2 + 1)^2
     = 4 + 90
     = 94

∂cost/∂x = 2(x - 3) = 4
∂cost/∂y = 20(y + 1) = 60
```

The Jet result should carry:

```text
value = 94
partials = [4, 60]
```

---

## 2. Jacobians of residual functions

This is one of the strongest uses for `Jet`.

Many numerical algorithms need:

```text
residual vector r(parameters)
Jacobian J = dr/dparameters
```

Examples:

```text
Gauss-Newton
Levenberg-Marquardt
nonlinear least squares
calibration
curve fitting
sensor model fitting
constraint solving
bundle-adjustment-style residuals
```

### Example: residual vector

Suppose a model predicts a point `(x, y)` and you have a target point `(tx, ty)`.

A residual vector could be:

```text
r0 = x - tx
r1 = y - ty
r2 = sqrt(x^2 + y^2) - target_radius
```

C++ shape:

```cpp
#include "fat_p/Jet.h"
#include <array>
#include <cmath>

using fat_p::autodiff::Jet;

template <class Scalar>
std::array<Scalar, 3> residuals(Scalar x, Scalar y,
                                double tx, double ty,
                                double target_radius)
{
    using std::hypot;

    return {
        x - tx,
        y - ty,
        hypot(x, y) - target_radius
    };
}

void example()
{
    using J = Jet<2>;

    J x = J::seed<0>(3.0);
    J y = J::seed<1>(4.0);

    auto r = residuals(x, y, 1.0, 2.0, 5.0);

    // residual values:
    // r[0].mValue, r[1].mValue, r[2].mValue

    // Jacobian rows:
    // r[0].mPartials
    // r[1].mPartials
    // r[2].mPartials
}
```

At `(x, y) = (3, 4)`:

```text
r0 = 2
r1 = 2
r2 = hypot(3, 4) - 5 = 0
```

The Jacobian is:

```text
r0 = x - tx
  ∂r0/∂x = 1
  ∂r0/∂y = 0

r1 = y - ty
  ∂r1/∂x = 0
  ∂r1/∂y = 1

r2 = sqrt(x^2 + y^2) - target_radius
  ∂r2/∂x = x / sqrt(x^2 + y^2) = 3/5
  ∂r2/∂y = y / sqrt(x^2 + y^2) = 4/5
```

So the Jacobian is:

```text
[ 1    0   ]
[ 0    1   ]
[ 0.6  0.8 ]
```

`Jet<2>` gives those rows directly from the partial arrays.

---

## 3. Geometry and coordinate transforms

Geometry code often fits `Jet` well because it is usually made from smooth scalar formulas.

Good examples:

```text
2D transforms
3D transforms
pose residuals
range and bearing
camera projection
angle normalization pieces
distance to point/line/plane
signed distance formulas
curvature formulas
```

### Example: range and bearing

Given a point `(x, y)`, compute:

```text
range   = sqrt(x^2 + y^2)
bearing = atan2(y, x)
```

C++ shape:

```cpp
#include "fat_p/Jet.h"
#include <cmath>

using fat_p::autodiff::Jet;

template <class Scalar>
Scalar range_to_origin(Scalar x, Scalar y)
{
    using std::hypot;
    return hypot(x, y);
}

template <class Scalar>
Scalar bearing_to_origin(Scalar x, Scalar y)
{
    using std::atan2;
    return atan2(y, x);
}

void example()
{
    using J = Jet<2>;

    J x = J::seed<0>(3.0);
    J y = J::seed<1>(4.0);

    J r = range_to_origin(x, y);
    J b = bearing_to_origin(x, y);

    // r.mPartials = [∂range/∂x, ∂range/∂y]
    // b.mPartials = [∂bearing/∂x, ∂bearing/∂y]
}
```

At `(3, 4)`:

```text
range = 5
∂range/∂x = 3/5
∂range/∂y = 4/5

bearing = atan2(4, 3)
∂bearing/∂x = -y / (x^2 + y^2) = -4/25
∂bearing/∂y =  x / (x^2 + y^2) =  3/25
```

So:

```text
range partials   = [0.6, 0.8]
bearing partials = [-0.16, 0.12]
```

---

## 4. Kinematics

Kinematics formulas are often excellent `Jet` targets.

Examples:

```text
robot arm forward kinematics
wheel odometry local updates
vehicle bicycle model equations
pose composition
local frame transforms
end-effector position formulas
```

### Example: planar arm endpoint

A simple two-link planar arm has joint angles `a` and `b`, with link lengths `L1` and `L2`.

Endpoint:

```text
x = L1 cos(a) + L2 cos(a + b)
y = L1 sin(a) + L2 sin(a + b)
```

The Jacobian tells how the endpoint moves when the joint angles change.

C++ shape:

```cpp
#include "fat_p/Jet.h"
#include <array>
#include <cmath>

using fat_p::autodiff::Jet;

template <class Scalar>
std::array<Scalar, 2> arm_endpoint(Scalar a, Scalar b,
                                   double L1, double L2)
{
    using std::sin;
    using std::cos;

    return {
        L1 * cos(a) + L2 * cos(a + b),
        L1 * sin(a) + L2 * sin(a + b)
    };
}

void example()
{
    using J = Jet<2>;

    J a = J::seed<0>(0.5);
    J b = J::seed<1>(0.25);

    auto p = arm_endpoint(a, b, 2.0, 1.0);

    // p[0].mValue = endpoint x
    // p[1].mValue = endpoint y

    // Jacobian:
    // row 0 = p[0].mPartials = derivatives of x wrt [a, b]
    // row 1 = p[1].mPartials = derivatives of y wrt [a, b]
}
```

This is exactly the type of small fixed-parameter derivative problem where forward mode works well.

---

## 5. Path and trajectory formulas

`Jet` is useful for path and trajectory formulas when the active formula is smooth.

Good examples:

```text
arc length formulas
line segment formulas
curvature penalties
heading residuals
local steering formulas
Dubins segment formulas
vehicle model residuals
time-scaling formulas
control effort terms
```

### Important branch warning

Many path planners choose among modes:

```text
left-straight-left
right-straight-right
left-straight-right
right-straight-left
other path families
```

The selection of the shortest path is a discrete decision. `Jet` can differentiate the active path formula, but it does not make the mode-selection boundary smooth.

Good target:

```text
differentiate the selected LSL formula away from switching boundaries
```

Bad target:

```text
differentiate the entire shortest-path selection as if min/path switching were smooth
```

If the minimum switches from one path family to another, the derivative may jump.

---

## 6. Calibration and model identification

Calibration often has small or moderate parameter blocks and residual formulas. That is a natural fit for `Jet`.

Examples:

```text
sensor bias calibration
scale factor calibration
camera intrinsics residuals
range sensor model residuals
robot kinematic parameter fitting
vehicle model parameter fitting
IMU correction formulas
```

### Example: affine sensor calibration

Suppose a sensor model is:

```text
prediction = scale * raw + bias
residual = prediction - measured
```

You want derivatives with respect to `scale` and `bias`.

C++ shape:

```cpp
template <class Scalar>
Scalar sensor_residual(Scalar scale, Scalar bias,
                       double raw, double measured)
{
    return scale * raw + bias - measured;
}

void example()
{
    using J = Jet<2>;

    J scale = J::seed<0>(1.05);
    J bias  = J::seed<1>(0.10);

    J r = sensor_residual(scale, bias, 12.0, 12.7);

    // r.mPartials[0] = ∂residual/∂scale = raw
    // r.mPartials[1] = ∂residual/∂bias  = 1
}
```

This scales directly to more complicated residuals.

---

## 7. Sensitivity analysis

Sensitivity analysis asks:

```text
If I change this input slightly, how much does the output change?
```

Examples:

```text
How does range change with x/y position?
How does path length change with turning radius?
How does cost change with penalty weight?
How does endpoint position change with joint angle?
How does residual change with calibration parameter?
```

`Jet` gives local first-order sensitivities.

### Example

If:

```text
range = hypot(x, y)
```

and at `(x, y) = (3, 4)`:

```text
range partials = [0.6, 0.8]
```

then a small perturbation:

```text
Δx = 0.01
Δy = -0.02
```

changes range approximately by:

```text
Δrange ≈ 0.6 * 0.01 + 0.8 * (-0.02)
       ≈ 0.006 - 0.016
       ≈ -0.010
```

This is local linear approximation.

---

## 8. First-order uncertainty propagation

If an output is computed from uncertain inputs, the Jacobian can propagate covariance approximately.

For:

```text
y = f(x)
```

first-order covariance propagation is:

```text
Cov_y ≈ J Cov_x J^T
```

where `J` is the Jacobian of `f` at the current point.

Good fits:

```text
sensor models
range/bearing transforms
coordinate transforms
small uncertainty propagation
local linearized models
state-estimation measurement functions
```

Caveat:

```text
This is a local first-order approximation.
It is not a full nonlinear uncertainty method.
```

If uncertainty is large or the function is strongly nonlinear, first-order propagation may be misleading.

---

## 9. Replacing fragile finite differences

Finite differences estimate derivatives by evaluating the function at nearby points.

Example:

```text
df/dx ≈ (f(x + h) - f(x - h)) / (2h)
```

This requires choosing `h`.

If `h` is too large:

```text
truncation error
```

If `h` is too small:

```text
rounding and cancellation error
```

Finite differences can also break when:

```text
branches change between x - h and x + h
scales are very different
inputs are near domain boundaries
function evaluations are noisy
```

`Jet` avoids finite-difference step-size error because it propagates derivatives through the arithmetic directly.

Finite differences are still useful for tests, but they are often a poor primary derivative implementation.

---

## 10. Testing hand-written derivatives

If you have manually derived or optimized derivative code, `Jet` can be used as a correctness oracle.

Use it to compare:

```text
manual gradient vs Jet gradient
manual Jacobian vs Jet Jacobian
symbolic derivative vs Jet derivative
finite-difference estimate vs Jet derivative
```

This is useful for catching:

```text
sign errors
missing product-rule terms
wrong atan2 argument order
missing chain-rule factors
branch-specific derivative mistakes
copy/paste mistakes
```

A good pattern is:

```text
randomly sample valid inputs
compute manual derivative
compute Jet derivative
compare within tolerance
also test edge cases near domain boundaries
```

---

# What Jet is not good for

## 1. Huge parameter vectors

Forward mode stores and propagates one partial derivative per independent variable.

If `N` is huge, every scalar operation becomes huge.

Bad fits:

```text
millions of neural-network weights
large dense optimization vectors
huge PDE parameter fields
large per-cell derivative states
very high-dimensional design spaces
```

For a scalar loss with many parameters, reverse-mode AD is usually better.

Rule:

```text
Small N: Jet is often good.
Huge N and one scalar output: use reverse mode.
```

---

## 2. Discrete algorithms

`Jet` does not differentiate discrete decisions.

Bad fits:

```text
sorting
hashing
graph search
A*
Dijkstra
integer programming
combinatorial optimization
state machines
parsers
string processing
map/set lookup as the main computation
```

You may still use `Jet` inside smooth numeric formulas that appear within those algorithms.

Example:

```text
Bad:  differentiate the whole A* planner
Good: differentiate the smooth edge-cost formula used by the planner
```

---

## 3. Branch boundaries and mode switches

Piecewise code can be fine away from boundaries. It is dangerous at the boundary.

Examples:

```text
min
max
clamp
abs at zero
if threshold
contact/no-contact switch
collision active/inactive switch
path family switch
saturation
lookup interval boundary
```

Example:

```cpp
template <class Scalar>
Scalar hinge(Scalar x)
{
    return x > 0.0 ? x : Scalar{0.0};
}
```

For `x > 0`, derivative is `1`.

For `x < 0`, derivative is `0`.

At `x = 0`, the function is not differentiable in the ordinary sense.

`Jet` will return the derivative of the executed branch, not a magical universal derivative.

---

## 4. Random or noisy functions

Automatic differentiation assumes deterministic arithmetic.

Bad fits:

```text
random sampling inside the function
Monte Carlo estimators as written
nondeterministic simulation
wall-clock dependent code
I/O dependent code
live sensor noise inside the differentiated function
```

Differentiate the deterministic model, not the random process around it.

For stochastic problems, you may need a different estimator or a reparameterized deterministic form.

---

## 5. Full Hessians as the primary result

`Jet` is a first-derivative tool.

It directly gives:

```text
gradients
Jacobians
first-order sensitivities
```

It does not directly give:

```text
full Hessians
third derivatives
higher-order Taylor expansions
```

If the primary goal is a full Hessian, use a higher-order AD design, symbolic method, finite-difference-of-gradient method, or a specialized second-order tool.

---

## 6. Heavy linear algebra as the differentiated scalar type

`Jet` is a scalar type. It works best when used inside scalar formulas.

It is not a replacement for a matrix AD framework.

Good:

```text
Use Jet as the scalar inside a small formula.
```

Be careful:

```text
Large matrices whose entries are Jet<N>
Huge dense factorizations over Jet scalars
Large dynamic linear algebra with many derivative directions
```

This may be correct but expensive.

---

# Detailed usage pattern

## Step 1: Write code generic over scalar type

Do not hard-code `double` inside the formula unless the value is truly constant.

Good:

```cpp
template <class Scalar>
Scalar formula(Scalar x, Scalar y)
{
    using std::sin;
    using std::hypot;
    return hypot(x, y) + sin(x);
}
```

Bad:

```cpp
double formula(double x, double y)
{
    return std::hypot(x, y) + std::sin(x);
}
```

The bad version only works for `double`. The good version works for `double` and for `Jet<N>`.

---

## Step 2: Use unqualified math calls with `using std::...`

Inside generic functions, prefer this pattern:

```cpp
using std::sin;
using std::cos;
using std::sqrt;
using std::atan2;
using std::hypot;

return hypot(x, y) + sin(theta);
```

This lets ordinary `double` code call `std::sin`, while `Jet` arguments can find the Jet overloads by argument-dependent lookup.

Avoid forcing `std::` on Jet arguments:

```cpp
return std::sin(x); // bad for generic Jet code
```

`std::sin` knows about built-in floating-point types. The Jet overload lives with the Jet type.

---

## Step 3: Seed inputs

Choose `N` as the number of independent variables.

For two independent inputs:

```cpp
using J = Jet<2>;

J x = J::seed<0>(3.0);
J y = J::seed<1>(4.0);
```

This means:

```text
partial slot 0 tracks x
partial slot 1 tracks y
```

---

## Step 4: Evaluate once

```cpp
J result = formula(x, y);
```

The ordinary value is:

```cpp
result.mValue
```

The derivatives are:

```cpp
result.mPartials[0]
result.mPartials[1]
```

---

## Step 5: For vector outputs, collect rows

If a function returns multiple `Jet<N>` values, each output's partials are one Jacobian row.

```cpp
auto outputs = residuals(x, y);

for (std::size_t row = 0; row < outputs.size(); ++row) {
    double value = outputs[row].mValue;
    auto gradient_row = outputs[row].mPartials;
}
```

---

# Choosing N

`N` is the number of independent variables, not the number of outputs.

Examples:

```text
f(x) -> scalar
  use Jet<1>

f(x, y) -> scalar
  use Jet<2>

F(x, y) -> 10 residuals
  use Jet<2>, not Jet<10>

pose residual with 6 pose parameters
  use Jet<6>

calibration block with 4 parameters
  use Jet<4>
```

If you only care about derivatives with respect to some inputs, seed only those inputs.

Example:

```text
f(x, y, fixed_constant)
```

If `fixed_constant` is not an optimization variable, use it as a plain `double` or constant Jet. Do not give it a seed direction.

---

# How to think about cost

Each `Jet<N>` operation propagates `N` partials.

Approximate cost model:

```text
double operation:  one value operation
Jet<N> operation:  one value operation plus N derivative updates
```

So if `N = 3`, overhead is usually reasonable.

If `N = 3000`, overhead may be enormous.

This is why `Jet` is best for small fixed parameter blocks.

Good sizes depend on the workload, but conceptually:

```text
N = 1..16       very natural
N = 16..64      often still reasonable for local kernels
N = hundreds    use carefully
N = thousands+  probably the wrong tool unless the computation is tiny
```

These are not hard limits. They are design guidance.

---

# Examples of good problem types

## Nonlinear least squares

Problem:

```text
minimize sum_i residual_i(parameters)^2
```

Use `Jet` to compute each residual and its Jacobian row.

Good fit because:

```text
parameter blocks are often small
residual formulas are usually smooth
Jacobian is needed by the solver
manual derivatives are error-prone
```

---

## Curve fitting

Example model:

```text
y = a exp(bx) + c
```

Parameters:

```text
a, b, c
```

Use `Jet<3>` to compute derivatives of prediction or residual with respect to `[a, b, c]`.

---

## Robot calibration

Example parameters:

```text
joint offset
link length correction
sensor bias
scale factor
```

Residuals are usually smooth functions of a small number of parameters.

Good fit.

---

## Camera projection residuals

Example:

```text
3D point -> projected pixel
residual = projected pixel - observed pixel
```

Differentiate with respect to:

```text
pose parameters
intrinsic parameters
point coordinates
small parameter block slices
```

Use caution around visibility, clipping, invalid depth, and robust loss branch boundaries.

---

## Vehicle model residuals

Example:

```text
next_state = dynamics(state, control, dt)
residual = predicted_next_state - measured_next_state
```

Good fit when the dynamics are smooth at the evaluated state.

Use caution around tire saturation, contact switches, clamps, and mode changes.

---

## Dubins or path-segment formulas

Good:

```text
derivative of a fixed path-family formula
arc length sensitivity
turning radius sensitivity
heading residual sensitivity
```

Careful:

```text
derivative of shortest path after min over path families
```

The formula for one path family may be smooth. The minimum over families is piecewise and can be nondifferentiable at switching points.

---

# Decision checklist

Before using `Jet`, ask:

```text
1. What are the independent variables?
2. How many are there?
3. Do I need first derivatives only?
4. Is the differentiated code mostly smooth scalar numeric math?
5. Are branches away from switching boundaries?
6. Are all inputs inside valid domains?
7. Is the function deterministic?
8. Is forward mode the right cost shape?
9. Do I need a gradient, Jacobian, or local sensitivity?
10. Can I write the formula generically over scalar type?
```

If the answers are mostly yes, `Jet` is probably a good fit.

---

# Decision table

| Problem | Use Jet? | Why |
|---|---:|---|
| Small scalar objective gradient | Yes | Direct gradient from one Jet output |
| Residual vector Jacobian | Yes | Each output partial array is one Jacobian row |
| Geometry formula | Yes | Smooth scalar math |
| Kinematics | Yes | Small parameter blocks and smooth formulas |
| Calibration residuals | Yes | Natural least-squares/Jacobian use case |
| Sensitivity analysis | Yes | Partials are local sensitivities |
| First-order covariance propagation | Yes | Needs Jacobian |
| Testing manual derivatives | Yes | Good oracle |
| Huge neural network training | No | Reverse mode is better |
| Millions of parameters and one loss | No | Forward mode cost too high |
| Sorting/search/graph algorithms | No | Discrete algorithm |
| Branch boundary derivative | Be careful | Active branch only |
| Random/noisy simulation | Usually no | Not deterministic arithmetic |
| Full Hessian as primary output | Not directly | Jet is first-order |
| Large dense linear algebra over Jet scalars | Be careful | May be correct but expensive |

---

# Common mistakes

## Mistake 1: Setting N to the number of outputs

Wrong:

```text
I have 10 residuals, so I need Jet<10>.
```

Correct:

```text
I have 3 parameters, so I need Jet<3>.
Each of the 10 residuals is a Jet<3>.
```

---

## Mistake 2: Differentiating through mode selection without thinking

Wrong:

```text
The planner selected this path, so the derivative of the whole planner is smooth.
```

Correct:

```text
Jet differentiated the selected formula. The selection itself may be discontinuous.
```

---

## Mistake 3: Using std:: math calls directly in generic code

Wrong:

```cpp
template <class Scalar>
Scalar f(Scalar x)
{
    return std::sin(x);
}
```

Correct:

```cpp
template <class Scalar>
Scalar f(Scalar x)
{
    using std::sin;
    return sin(x);
}
```

---

## Mistake 4: Seeding constants

Wrong:

```text
Seed every numeric value.
```

Correct:

```text
Only seed independent variables.
Constants should have zero partials.
```

---

## Mistake 5: Trusting derivatives at nondifferentiable points

Wrong:

```text
Jet returned a number, so the derivative is mathematically valid.
```

Correct:

```text
At branch boundaries or nonsmooth points, the returned derivative is only the derivative of the executed convention or branch.
```

---

# Summary

Use `Jet` for first derivatives of smooth numeric kernels with small or moderate input dimension.

The best uses are:

```text
gradients
Jacobians
residuals
geometry
kinematics
calibration
path formulas
sensitivity analysis
first-order uncertainty propagation
derivative testing
finite-difference replacement
```

Avoid using `Jet` as a blanket derivative wrapper around large, discrete, stochastic, or high-dimensional systems.

The right mental model is:

```text
Write the math once.
Seed the inputs you care about.
Run the same formula.
Read the value and derivatives from the result.
```

