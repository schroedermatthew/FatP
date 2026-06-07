---
doc_id: EX-JET-002
doc_type: "Example"
title: "Dubins Tour Gradient Assembly via Jet"
fatp_components: ["Jet"]
topics: ["Dubins tour", "multi-segment path", "trajectory optimization", "gradient assembly", "sparse Jacobian", "block-bidiagonal structure", "automatic differentiation", "exact gradient", "SNOPT user function"]
constraints: ["per-waypoint locality", "non-smoothness at word switches", "constant-parameter differentiation", "finite-difference cross-check"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-06-06"
audience: ["C++ developers", "motion planning", "optimization", "AI assistants"]
status: "draft"
example_source: "components/Jet/examples/dubins_chain_jet.cpp"
---

# Dubins Tour Gradient Assembly via Jet

*Fat-P Library — June 2026*

---

## What this example is

This example chains the single-segment Dubins path of `EX-JET-001` into a **tour** through several waypoints, and assembles the exact gradient of the tour's total length in the form a gradient-based solver consumes. The single-segment file gives one length and its gradient; this file shows how those pieces compose when the waypoints become decision variables, and how the gradient is built from local per-segment blocks rather than one monolithic evaluation.

The tour visits a fixed start, a sequence of interior waypoints, and a fixed goal. Its cost is the sum of single-segment Dubins lengths over consecutive poses, and the interior waypoint coordinates are the variables a solver would move. Each segment's length is differentiated with respect to its own six endpoint coordinates by one `Jet<6>` evaluation, and the resulting block is scattered into the global gradient. The structure that emerges — a block-bidiagonal segment-length Jacobian whose column sum is the objective gradient — is exactly what a sparse nonlinear solver such as SNOPT expects from a user function. The geometry lives in `dubins_geometry.h`, shared with `EX-JET-001`; the AD scalar is documented in `OV-JET-001` / `UM-JET-001` / `CG-JET-001`. The companion source is [`dubins_chain_jet.cpp`](components/Jet/examples/dubins_chain_jet.cpp).

---

## The tour and its objective

Let the tour visit poses $q_0, q_1, \dots, q_M$, with $q_0$ the fixed start and $q_M$ the fixed goal, and the interior poses $q_1, \dots, q_{M-1}$ free. Each pose is

$$
q_i = (x_i,\ y_i,\ \theta_i).
$$

The decision vector stacks the interior poses:

$$
z = (x_1, y_1, \theta_1,\ \dots,\ x_{M-1}, y_{M-1}, \theta_{M-1}) \in \mathbb{R}^{n}, \qquad n = 3(M-1),
$$

so interior waypoint $k$ occupies columns $3(k-1),\ 3(k-1)+1,\ 3(k-1)+2$. Writing $\ell(q_a, q_b;\rho)$ for the single-segment Dubins shortest-path length of `EX-JET-001`, the objective is the summed length

$$
L(z) = \sum_{i=0}^{M-1} \ell\big(q_i,\ q_{i+1};\ \rho\big).
$$

The turning radius $\rho$ is a fixed parameter of the example, not a decision variable.

---

## Locality: each waypoint touches two segments

Segment $i$ joins $q_i$ to $q_{i+1}$, so its length $\ell_i := \ell(q_i, q_{i+1})$ depends on $z$ only through the coordinates of those two poses — at most six numbers. An interior pose $q_i$ (for $1 \le i \le M-1$) appears in exactly two segments: as the second endpoint of $\ell_{i-1}$ and the first endpoint of $\ell_i$. The sum rule and this locality give the gradient block at that waypoint:

$$
\frac{\partial L}{\partial q_i} = \frac{\partial \ell_{i-1}}{\partial q_i} + \frac{\partial \ell_i}{\partial q_i},
\qquad
\frac{\partial}{\partial q_i} \equiv \left( \frac{\partial}{\partial x_i},\ \frac{\partial}{\partial y_i},\ \frac{\partial}{\partial \theta_i} \right).
$$

Every interior waypoint accumulates the contributions of its two incident segments, and nothing else. This is the structure the assembly reproduces.

---

## Per-segment differentiation with Jet

Because the geometry is written over a scalar type, a segment length is differentiated by evaluating it on seeded jets. For segment $i$, seed its six endpoint coordinates as a `Jet<6>` in directions $0$ through $5$,

$$
x_i \mapsto 0,\quad y_i \mapsto 1,\quad \theta_i \mapsto 2,\quad x_{i+1} \mapsto 3,\quad y_{i+1} \mapsto 4,\quad \theta_{i+1} \mapsto 5,
$$

and one evaluation returns the length together with its local gradient

$$
\nabla_{\!\mathrm{loc}}\,\ell_i = \left(
\frac{\partial \ell_i}{\partial x_i},\
\frac{\partial \ell_i}{\partial y_i},\
\frac{\partial \ell_i}{\partial \theta_i},\
\frac{\partial \ell_i}{\partial x_{i+1}},\
\frac{\partial \ell_i}{\partial y_{i+1}},\
\frac{\partial \ell_i}{\partial \theta_{i+1}}
\right) \in \mathbb{R}^{6}.
$$

Holding $\rho$ as a constant jet keeps it out of the partials. The first three components are $\partial \ell_i / \partial q_i$, the last three are $\partial \ell_i / \partial q_{i+1}$ — the two halves that the locality formula adds together at shared waypoints.

---

## Assembling the global Jacobian and gradient

Collect the segment lengths into the map $\ell : \mathbb{R}^n \to \mathbb{R}^M$, $\ell(z) = (\ell_0, \dots, \ell_{M-1})$, and let

$$
J \in \mathbb{R}^{M \times n}, \qquad J_{ij} = \frac{\partial \ell_i}{\partial z_j}
$$

be its Jacobian. Scatter places each segment's local block into this matrix: the first three entries of $\nabla_{\!\mathrm{loc}}\,\ell_i$ land in the columns of $q_i$ and the last three in the columns of $q_{i+1}$, but only where those poses are interior — entries that would fall on the fixed $q_0$ or $q_M$ are dropped. Row $i$ therefore touches at most the two consecutive three-column waypoint blocks it borders, so $J$ is **block-bidiagonal**. For a tour with three interior waypoints (four segments), grouping the columns by waypoint $(q_1 \mid q_2 \mid q_3)$,

$$
J =
\begin{pmatrix}
\partial_2 \ell_0 & 0 & 0 \\
\partial_1 \ell_1 & \partial_2 \ell_1 & 0 \\
0 & \partial_1 \ell_2 & \partial_2 \ell_2 \\
0 & 0 & \partial_1 \ell_3
\end{pmatrix},
$$

where $\partial_1 \ell_i$ and $\partial_2 \ell_i$ are the $1 \times 3$ gradients of $\ell_i$ with respect to its first and second endpoint. The objective gradient is the column sum over the segment rows,

$$
g_j = \frac{\partial L}{\partial z_j} = \sum_{i=0}^{M-1} J_{ij}, \qquad g = J^{\!\top} \mathbf{1}_M,
$$

which, block by block, reproduces the locality formula:

$$
g = \big(\ \partial_2 \ell_0 + \partial_1 \ell_1,\ \ \ \partial_2 \ell_1 + \partial_1 \ell_2,\ \ \ \partial_2 \ell_2 + \partial_1 \ell_3\ \big).
$$

---

## Why the assembled gradient is the exact gradient

There is no approximation in the scatter. Since $L = \sum_i \ell_i$ and each $\ell_i$ depends on $z$ only through its two endpoint slices, the chain rule reads

$$
\frac{\partial L}{\partial z_j} = \sum_{i=0}^{M-1} \frac{\partial \ell_i}{\partial z_j},
$$

and the right-hand side is precisely what scatter-and-accumulate computes — each `Jet<6>` block supplies the nonzero $\partial \ell_i / \partial z_j$, the zeros are the structural zeros of $J$. A single `Jet<n>` instantiated over the whole chain forms the same sum in the same order of floating-point additions, so the two routes agree to the last bit; the example reports a maximum difference of $0$ between them. The per-segment route is the one a solver uses, because it computes only the nonzero blocks and never forms an $n$-wide jet.

---

## Mapping to a sparse solver

A sparse nonlinear solver such as SNOPT calls a user function that returns a vector $F$ and the nonzero derivative entries $G$ in coordinate form, with the sparsity pattern declared once at setup. The assembly produces both framings directly.

As an objective alone,

$$
F = L(z), \qquad G = g = J^{\!\top}\mathbf{1},
$$

a length-$n$ gradient (dense, or sparse if some waypoints are pinned). Exposing the segments as rows — for instance as per-segment length constraints $\ell_i \le \ell_{\max}$, or to let the solver see the path structure — instead returns

$$
F = (\ell_0, \dots, \ell_{M-1}), \qquad G = \{\, J_{ij} \ne 0 \,\},
$$

the nonzeros of the block-bidiagonal $J$. The pattern carries at most six nonzeros per row: $2 \times 3$ for a row whose segment lies between two interior waypoints, and $3$ for the two boundary rows that meet a fixed endpoint. Jet supplies the $G$ values to floating-point rounding, with no finite-difference step to choose and no second derivative routine to maintain alongside the length.

---

## Non-smoothness

Each segment length $\ell_i$ is continuous and piecewise smooth, in the sense of `EX-JET-001`: non-differentiable only where that segment's optimal word switches or one of its wrapped angles crosses a multiple of $2\pi$. The summed length $L$ is non-differentiable on the union of those segment sets, which is still measure-zero. Off that set the assembled gradient is the true gradient, correct to rounding. The non-smoothness is the geometry of the Dubins tour, not an artifact of the differentiation; a sequential quadratic programming method works with a piecewise-smooth objective, but the corners are real and a solver may stall against one.

---

## What the example computes

The demonstration runs a four-segment tour: fixed start $(0, 0, 0)$, fixed goal $(10, 0, 0)$, three interior waypoints, $\rho = 1$, giving $n = 9$ decision variables. At the initial waypoints the active words are LSL, RSL, LSL, LRL and the total length is $18.324835$.

The segment-length Jacobian shows the block-bidiagonal pattern, segment $i$ touching only the waypoints it borders:

```
        q1      q2      q3
seg 0  X X X . . . . . .
seg 1  X X X X X X . . .
seg 2  . . . X X X X X X
seg 3  . . . . . . X X X
```

Three independent computations of the gradient then agree:

- the assembled gradient from the per-segment `Jet<6>` blocks matches a single monolithic `Jet<9>` over the whole chain to a maximum difference of $0$ — the assembly is exact, not approximate;
- it matches a central finite difference at step $h = 10^{-6}$ to a maximum difference of $2.2 \times 10^{-9}$;
- a backtracking gradient descent on the assembled gradient drives the length from $18.324835$ down to $10.000000$.

The last figure is the lower bound for this tour. Any path from start to goal has arc length at least the straight-line distance between them,

$$
L(z) \ \ge\ \big\| (x_M, y_M) - (x_0, y_0) \big\| = 10,
$$

and because the start and goal headings are both zero and collinear, the bound is attained when every waypoint lies on the connecting line with heading zero, making each segment a straight run. The descent recovers that configuration to six places, which confirms the assembled gradient is a usable descent direction and not merely numerically plausible. The example compiles under `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Werror` and reports no findings under AddressSanitizer or UndefinedBehaviorSanitizer.

---

## Building

```sh
g++ -std=c++20 -I<dir containing fat_p/Jet.h> dubins_chain_jet.cpp -o dubins_chain_jet
```

The tour size is a runtime quantity, while a jet's direction count is fixed at compile time. The per-segment route needs only `Jet<6>` regardless of how many waypoints the tour has, so it scales to any tour length without recompilation; the monolithic cross-check is instantiated at the example's specific $n = 9$ and is present only to confirm the assembly. A production assembly keeps the `Jet<6>` blocks and the scatter, and never forms the wide jet.

---

## References

- L. E. Dubins, "On curves of minimal length with a constraint on average curvature, and with prescribed initial and terminal positions and tangents," *American Journal of Mathematics*, 79(3):497–516, 1957.
- A. M. Shkel and V. Lumelsky, "Classification of the Dubins set," *Robotics and Autonomous Systems*, 34(4):179–202, 2001.
- P. E. Gill, W. Murray, and M. A. Saunders, "SNOPT: An SQP algorithm for large-scale constrained optimization," *SIAM Review*, 47(1):99–131, 2005.
- Fat-P: `EX-JET-001` (single-segment Dubins via Jet); `OV-JET-001`, `UM-JET-001`, `CG-JET-001` (the `Jet` forward-mode AD scalar).
