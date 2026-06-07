---
doc_id: EX-JET-001
doc_type: "Example"
title: "Dubins Shortest Path via Jet"
fatp_components: ["Jet"]
topics: ["Dubins path", "shortest path", "bounded-curvature motion", "automatic differentiation", "exact gradient", "path-length sensitivity", "trajectory optimization", "dual numbers"]
constraints: ["non-smoothness at word switches", "angle-wrap differentiation", "argmin gradient", "out-of-domain segment formulas", "finite-difference cross-check"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-06-06"
audience: ["C++ developers", "motion planning", "optimization", "AI assistants"]
status: "draft"
example_source: "components/Jet/examples/dubins_jet.cpp"
---

# Dubins Shortest Path via Jet

*Fat-P Library — June 2026*

---

## What this example is

This example computes the shortest **Dubins path** — the minimum-length route between two oriented poses for a vehicle that cannot turn tighter than a fixed radius — and, from the same code, the **exact gradient** of that path length with respect to the goal pose and the turning radius.

The point of the example is the second part. The path geometry is written once, templated on the scalar type `T`. Instantiating it two ways gives two different results from one body of code:

- with `T = double`, the geometry returns the path: which of the six Dubins words is optimal, its segment magnitudes, the total length, and a sampled sequence of waypoints;
- with `T = Jet<4>`, the same geometry returns the length **together with** its partial derivatives with respect to the four seeded inputs $(x_1, y_1, \theta_1, \rho)$, computed in a single evaluation by the chain rule rather than by a finite-difference loop.

Dubins-path length is built from `sin`, `cos`, `atan2`, `sqrt`, and `acos`, with branch selection over the six word types and angle wrapping throughout. That makes it a representative test of forward-mode automatic differentiation: every elementary function in `Jet` is exercised, the `atan2`/`sqrt`/`acos` domain edges matter, and the non-smooth pieces (the `min` over words and the angle wrap) require explicit handling. The companion source is [`dubins_jet.cpp`](components/Jet/examples/dubins_jet.cpp); the AD scalar it uses is documented in `OV-JET-001` / `UM-JET-001` / `CG-JET-001`.

---

## The Dubins problem

Model the vehicle as a unit-speed planar kinematic car with bounded turning rate. Its state is a pose $q = (x, y, \theta)$, and its motion obeys

$$
\dot{x} = \cos\theta, \qquad \dot{y} = \sin\theta, \qquad \dot{\theta} = u, \qquad |u| \le \frac{1}{\rho}.
$$

Because the speed is one, arc length equals elapsed parameter, so minimizing path length is minimizing the total parameter to steer from a start pose $q_0 = (x_0, y_0, \theta_0)$ to a goal pose $q_1 = (x_1, y_1, \theta_1)$. The bound $|u| \le 1/\rho$ caps curvature: the tightest turn the vehicle can make is a circle of radius $\rho$.

**Dubins' theorem (1957).** The shortest such path is always one of six *words*, each a concatenation of three segments drawn from $\{L, R, S\}$, where $L$ and $R$ are circular arcs of radius $\rho$ traversed counter-clockwise and clockwise respectively, and $S$ is a straight segment:

$$
\mathcal{W} = \{\, \mathrm{LSL},\ \mathrm{RSR},\ \mathrm{LSR},\ \mathrm{RSL} \,\} \cup \{\, \mathrm{RLR},\ \mathrm{LRL} \,\}.
$$

The first four are *CSC* words (arc — straight — arc); the last two are *CCC* words (three arcs). The shortest path is the shortest **valid** word, where a word is valid when its closed-form construction has a real solution.

---

## Reduction to a canonical frame

Each word has a closed form once the problem is placed in a canonical frame: start at the origin, start heading along $+x$, and turning radius scaled to one. The transformation is a translation, a rotation, and a uniform scale. Define

$$
\Delta x = x_1 - x_0, \qquad \Delta y = y_1 - y_0,
$$

$$
D = \sqrt{\Delta x^2 + \Delta y^2}, \qquad d = \frac{D}{\rho}, \qquad \theta = \operatorname{atan2}(\Delta y, \Delta x),
$$

$$
\alpha = \operatorname{mod}_{2\pi}(\theta_0 - \theta), \qquad \beta = \operatorname{mod}_{2\pi}(\theta_1 - \theta).
$$

Here $D$ is the straight-line distance between the poses, $d$ is that distance in units of the turning radius, $\theta$ is the bearing from start to goal, and $(\alpha, \beta)$ are the start and goal headings relative to that bearing. The wrap operator

$$
\operatorname{mod}_{2\pi}(x) = x - 2\pi \left\lfloor \frac{x}{2\pi} \right\rfloor \ \in\ [0, 2\pi)
$$

keeps every angle in a single revolution. After this reduction the canonical problem runs from $(0, 0, \alpha)$ to $(d, 0, \beta)$ with unit radius, and the segment magnitudes returned below are in that unit-radius frame.

---

## Closed-form segment magnitudes

For each word the construction returns a triple $(t, p, q)$ of normalized segment magnitudes. For an arc segment the magnitude is the **turn angle** in radians; for the straight segment of a CSC word the magnitude is a **length** in the unit-radius frame. A word is valid only when the indicated square-root or arc-cosine argument is in range. The formulas below match the example line for line.

**LSL.**

$$
\begin{aligned}
p^2 &= 2 + d^2 - 2\cos(\alpha - \beta) + 2d\,(\sin\alpha - \sin\beta), \\
u   &= \operatorname{atan2}\!\big(\cos\beta - \cos\alpha,\ \ d + \sin\alpha - \sin\beta\big), \\
t &= \operatorname{mod}_{2\pi}(-\alpha + u), \qquad p = \sqrt{p^2}, \qquad q = \operatorname{mod}_{2\pi}(\beta - u),
\end{aligned}
$$

valid when $p^2 \ge 0$.

**RSR.**

$$
\begin{aligned}
p^2 &= 2 + d^2 - 2\cos(\alpha - \beta) + 2d\,(\sin\beta - \sin\alpha), \\
u   &= \operatorname{atan2}\!\big(\cos\alpha - \cos\beta,\ \ d - \sin\alpha + \sin\beta\big), \\
t &= \operatorname{mod}_{2\pi}(\alpha - u), \qquad p = \sqrt{p^2}, \qquad q = \operatorname{mod}_{2\pi}(-\beta + u),
\end{aligned}
$$

valid when $p^2 \ge 0$.

**LSR.**

$$
\begin{aligned}
p^2 &= -2 + d^2 + 2\cos(\alpha - \beta) + 2d\,(\sin\alpha + \sin\beta), \qquad p = \sqrt{p^2}, \\
u   &= \operatorname{atan2}\!\big(-\cos\alpha - \cos\beta,\ \ d + \sin\alpha + \sin\beta\big) - \operatorname{atan2}(-2,\ p), \\
t &= \operatorname{mod}_{2\pi}(-\alpha + u), \qquad q = \operatorname{mod}_{2\pi}(-\beta + u),
\end{aligned}
$$

valid when $p^2 \ge 0$.

**RSL.**

$$
\begin{aligned}
p^2 &= -2 + d^2 + 2\cos(\alpha - \beta) - 2d\,(\sin\alpha + \sin\beta), \qquad p = \sqrt{p^2}, \\
u   &= \operatorname{atan2}\!\big(\cos\alpha + \cos\beta,\ \ d - \sin\alpha - \sin\beta\big) - \operatorname{atan2}(2,\ p), \\
t &= \operatorname{mod}_{2\pi}(\alpha - u), \qquad q = \operatorname{mod}_{2\pi}(\beta - u),
\end{aligned}
$$

valid when $p^2 \ge 0$.

**RLR.**

$$
\xi = \tfrac{1}{8}\big(6 - d^2 + 2\cos(\alpha - \beta) + 2d\,(\sin\alpha - \sin\beta)\big),
$$

$$
\begin{aligned}
p &= \operatorname{mod}_{2\pi}\!\big(2\pi - \arccos\xi\big), \\
t &= \operatorname{mod}_{2\pi}\!\Big(\alpha - \operatorname{atan2}\!\big(\cos\alpha - \cos\beta,\ d - \sin\alpha + \sin\beta\big) + \operatorname{mod}_{2\pi}\!\big(\tfrac{p}{2}\big)\Big), \\
q &= \operatorname{mod}_{2\pi}\!\big(\alpha - \beta - t + \operatorname{mod}_{2\pi}(p)\big),
\end{aligned}
$$

valid when $|\xi| \le 1$.

**LRL.**

$$
\xi = \tfrac{1}{8}\big(6 - d^2 + 2\cos(\alpha - \beta) + 2d\,(-\sin\alpha + \sin\beta)\big),
$$

$$
\begin{aligned}
p &= \operatorname{mod}_{2\pi}\!\big(2\pi - \arccos\xi\big), \\
t &= \operatorname{mod}_{2\pi}\!\Big(-\alpha - \operatorname{atan2}\!\big(\cos\alpha - \cos\beta,\ d + \sin\alpha - \sin\beta\big) + \tfrac{p}{2}\Big), \\
q &= \operatorname{mod}_{2\pi}\!\big(\operatorname{mod}_{2\pi}(\beta) - \alpha - t + \operatorname{mod}_{2\pi}(p)\big),
\end{aligned}
$$

valid when $|\xi| \le 1$.

These are the forms in A. Walker's `dubins.c`, following Shkel & Lumelsky's classification of the Dubins set.

---

## Selecting the shortest path

A word's normalized length is $t + p + q$. Because the canonical frame scaled the radius to one, the **actual** length restores the factor $\rho$:

$$
L_w = \rho\,(t_w + p_w + q_w).
$$

The Dubins length is the minimum over the valid words,

$$
L^\star = \rho \min_{w \in \mathcal{W}_{\text{valid}}} (t_w + p_w + q_w),
$$

and the optimal word is the minimizer $w^\star = \arg\min_{w} L_w$. The example evaluates all six words, discards the invalid ones, and keeps the one of least length.

---

## Reconstructing the path

Given the optimal word and its magnitudes, the pose is propagated segment by segment by integrating the kinematics in closed form. From a pose $(x, y, \theta)$:

a **straight** segment of length $\ell$ gives

$$
x' = x + \ell\cos\theta, \qquad y' = y + \ell\sin\theta, \qquad \theta' = \theta;
$$

a **left** arc through angle $\phi$ (radius $\rho$) gives

$$
x' = x + \rho\big(\sin(\theta + \phi) - \sin\theta\big), \quad
y' = y + \rho\big(\cos\theta - \cos(\theta + \phi)\big), \quad
\theta' = \theta + \phi;
$$

a **right** arc through angle $\phi$ (radius $\rho$) gives

$$
x' = x + \rho\big(\sin\theta - \sin(\theta - \phi)\big), \quad
y' = y + \rho\big(\cos(\theta - \phi) - \cos\theta\big), \quad
\theta' = \theta - \phi.
$$

One normalization detail bridges the magnitudes and the reconstruction. A turn angle is scale-invariant, so for an arc segment the magnitude $t$, $q$ (and $p$ for a CCC word) is used directly as $\phi$. A length is not scale-invariant, so for the straight segment of a CSC word the normalized magnitude $p$ becomes the actual length $\ell = \rho\,p$. Sampling each segment at a fixed number of intermediate fractions yields the waypoint sequence, and the final waypoint reproduces the goal pose — the end-to-end check that the word formulas and the integration agree.

---

## Differentiation with Jet

The reason to write the geometry over a scalar type is to obtain $\nabla L^\star$ without writing a second derivative routine and without finite-difference error.

**The scalar.** A `Jet<N>` carries a value and $N$ partial derivatives,

$$
\hat{v} = v + \sum_{k=0}^{N-1} g_k\, \varepsilon_k, \qquad \varepsilon_j \varepsilon_k = 0,
$$

a first-order truncated dual number. For an analytic $f$,

$$
f\!\left(v + \sum_k g_k \varepsilon_k\right) = f(v) + f'(v) \sum_k g_k\, \varepsilon_k,
$$

so each elementary operation maps the value by $f$ and the partials by $f'(v)$ — the chain rule, carried in the partial slots. **Seeding** input $i$ means giving it the partial vector $e_i$ (a one in slot $i$). After a function runs on seeded inputs, the partial in slot $k$ of any output holds that output's derivative with respect to the input seeded in direction $k$.

**The gradient computed here.** The example seeds the four inputs of interest,

$$
x_1 \mapsto \text{slot } 0, \quad y_1 \mapsto \text{slot } 1, \quad \theta_1 \mapsto \text{slot } 2, \quad \rho \mapsto \text{slot } 3,
$$

with the start pose held as a constant Jet, and reads the gradient out of the resulting length:

$$
\nabla L^\star = \left( \frac{\partial L^\star}{\partial x_1},\ \frac{\partial L^\star}{\partial y_1},\ \frac{\partial L^\star}{\partial \theta_1},\ \frac{\partial L^\star}{\partial \rho} \right)
= \big(L^\star.\texttt{partials}[0..3]\big).
$$

These four numbers are the first-order change in optimal path length per unit change in each goal coordinate and in the turning radius. Three constructs in the geometry need comment, because each is a place where naive differentiation would be wrong.

**Angle wrap.** Differentiating $\operatorname{mod}_{2\pi}$ directly is the trap. Since

$$
\frac{d}{dx}\operatorname{mod}_{2\pi}(x) = 1 - 2\pi\,\frac{d}{dx}\!\left\lfloor \frac{x}{2\pi} \right\rfloor = 1 \quad \text{(away from jumps),}
$$

the wrap contributes a unit derivative: the floor term is locally constant, so its derivative is zero. The example computes the integer multiple $2\pi\lfloor v / 2\pi\rfloor$ from the **value** $v$ and subtracts it as a constant, which leaves the partials untouched. Subtracting a quantity the differentiator believes is constant is exactly what reproduces the derivative of one.

**The minimum over words.** The optimal length is a pointwise minimum. Away from a tie,

$$
\nabla \min_{w} L_w = \nabla L_{w^\star}, \qquad w^\star = \arg\min_w L_w,
$$

so the gradient of the minimum is the gradient of the active word. The example obtains this by evaluating all six words as Jets and selecting the one of least **value**; the selected Jet already carries the active word's partials, so the gradient is consistent with the length by construction.

**Segment-formula domains.** A word is valid only where its $\sqrt{\cdot}$ or $\arccos$ argument is in range. The validity test reads the **value**, so out-of-domain words are discarded before any square root or arc cosine is taken on them. On a `Jet`, an out-of-domain $\sqrt{\cdot}$ or $\arccos$ would yield a NaN value with NaN partials, but the value-level test removes those words first, so only in-domain words enter the comparison.

---

## Where the gradient is valid

$L^\star$ is a continuous, piecewise-smooth function of the goal pose and the radius. It fails to be differentiable only on a measure-zero set: the boundaries where the optimal word $w^\star$ switches from one type to another (the length is continuous across the switch but has a corner), and the configurations where a wrapped angle crosses a multiple of $2\pi$. Off that set the `Jet` gradient is the true gradient, correct to floating-point rounding. A gradient-based solver therefore sees a piecewise-smooth objective, with the non-smoothness intrinsic to the Dubins problem rather than introduced by the differentiation.

---

## What the example computes

Two checks anchor correctness, and neither is an assertion that the code works.

A sanity case sends the vehicle straight ahead with aligned headings — start $(0, 0, 0)$, goal $(4, 0, 0)$, $\rho = 1$ — and recovers the straight-line answer, word LSL with length $4.000000$.

The main scenario steers from $(0, 0, 0)$ to $(3, 3, \tfrac{\pi}{2})$ with $\rho = 1$. The optimal word is LSL with

$$
(t, p, q) = \left(\tfrac{\pi}{4},\ 2\sqrt{2},\ \tfrac{\pi}{4}\right) \approx (0.7854,\ 2.8284,\ 0.7854), \qquad
L^\star = \tfrac{\pi}{2} + 2\sqrt{2} \approx 4.399223.
$$

The two gates then fire:

- **Reconstruction.** The sampled path lands on the goal pose to $9.9 \times 10^{-16}$ in position and $0$ in heading, confirming that the closed-form magnitudes and the segment integration agree.
- **Differentiation.** The `Jet<4>` gradient is

$$
\nabla L^\star \approx (0.70710678,\ 0.70710678,\ 0.29289322,\ 0.15658276),
$$

  and it agrees with a central finite difference at step $h = 10^{-6}$ to a maximum absolute discrepancy of $5.0 \times 10^{-10}$ across all four components, with the active word identical on the value path and the Jet path. The AD gradient is exact to rounding and computed in one pass; the finite difference is the independent cross-check, carrying the truncation-plus-roundoff error that the AD result does not.

The example also compiles under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror` and reports no findings under AddressSanitizer or UndefinedBehaviorSanitizer.

---

## Building and adapting

```sh
g++ -std=c++20 -I<dir containing fat_p/Jet.h> dubins_jet.cpp -o dubins_jet
```

The geometry is templated on the scalar, so adapting the differentiation does not touch it. Seeding a different set of inputs changes only which partials come back; differentiating with respect to the start pose as well as the goal raises the partial count to `Jet<7>` and seeds seven directions, with the geometry unchanged. The same property is what lets a chain of Dubins segments through several waypoints hand a gradient-based path optimizer an exact per-waypoint Jacobian: evaluate the chained length over `Jet<N>`, seed the waypoint coordinates, and read the Jacobian from the partials.

---

## References

- L. E. Dubins, "On curves of minimal length with a constraint on average curvature, and with prescribed initial and terminal positions and tangents," *American Journal of Mathematics*, 79(3):497–516, 1957.
- A. M. Shkel and V. Lumelsky, "Classification of the Dubins set," *Robotics and Autonomous Systems*, 34(4):179–202, 2001.
- A. Walker, `dubins.c` (Dubins-Curves), BSD license — the closed forms transcribed here.
- Fat-P: `OV-JET-001`, `UM-JET-001`, `CG-JET-001` — the `Jet` forward-mode AD scalar.
