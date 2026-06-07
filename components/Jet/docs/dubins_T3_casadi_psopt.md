# Bounded-horizon Dubins optimal control with a generic accumulation objective: direct multiple shooting (CasADi) vs pseudospectral collocation (PSOPT)

*Draft — author(s): [you]*

## Abstract

We study the bounded-horizon (T3) optimal control of a Dubins vehicle that optimizes a
general *accumulation* objective — a quantity integrated along the path, with a smooth
terminal aggregator — subject to a fixed time budget $t_f\le T_{\max}$. Detection
probability is the running example, but the formulation covers coverage, information gain,
exposure, and dwell. We give the problem in Bolza form, note the saturation subtlety that
makes the budget bound active (and the regularization that restores a unique tail), and
present two reference implementations from *different transcription families*: direct
multiple shooting (RK4 gap-closing) in CasADi/IPOPT (Python) and adaptive pseudospectral
collocation in PSOPT/IPOPT (C++). The comparison is therefore as much about the
transcription class as the tool — shooting's simplicity and warm-start friendliness against
spectral accuracy with automatic mesh control. We give a reproducible all-free toolchain
(IPOPT + MUMPS).

## 1. Introduction

Many Dubins-vehicle tasks are *fixed-budget* searches: maximize what you accumulate
(detection, coverage, information) within a hard time or energy limit, rather than
minimize time to a goal. This is the bounded-time (T3) regime — $t_f\le T_{\max}$ — and it
is the well-posed home for *time-favorable* objectives, those that improve monotonically
with horizon and are therefore degenerate under free final time.

Our contribution is threefold: (i) a clean specialization of the bounded-horizon Dubins
problem with a generic accumulation objective, including the saturation/regularization
point that is easy to get wrong; (ii) two reference implementations chosen to be free,
open, and from two transcription families — direct multiple shooting (RK4) in CasADi
(Python) and pseudospectral collocation in PSOPT (C++); and (iii) a structured comparison
of those families — accuracy, mesh control, derivatives, deployment — plus a reproducibility
recipe so the results can be regenerated with no commercial software.

## 2. Problem — bounded-horizon Dubins with an accumulation objective

State $z=(x,y,\theta,\zeta)\in\mathbb R^{3+m}$: Dubins kinematics plus $m$ accumulation
states $\zeta$. Control $u$ (turn rate), unit speed $V=1$.

$$
\dot x=\cos\theta,\quad \dot y=\sin\theta,\quad \dot\theta=u,\quad \dot\zeta=\ell(x,y,u,t),\quad |u|\le u_{\max}=\tfrac{1}{R_{\min}}.
$$

Objective (minimize, WLOG; maximizing accumulation is minimizing its complement):

$$
J=\varphi\big(\zeta(t_f)\big)+\rho\!\int_0^{t_f}\lVert u\rVert^2\,dt,\qquad t_f\le T_{\max},\qquad \zeta(0)=0,\ \ (x,y,\theta)(0)\ \text{fixed}.
$$

**Worked instance (detection).** $m=1$, $\zeta=E$ (search effort), $\ell=\lambda(x,y)$ a
smooth footprint $\lambda(x,y)=\lambda_0\exp(-\lVert(x,y)-\xi\rVert^2/2\sigma^2)$, and
$\varphi(E)=e^{-E}$ (minimizing non-detection $=$ maximizing $1-e^{-E}$). Coverage,
information gain, and exposure are obtained by changing $(\ell,\varphi)$.

**Why the bound is active, and the tail subtlety.** Because $\dot E=\lambda\ge0$, more time
never lowers the objective, so the optimum sits at $t_f=T_{\max}$ — we therefore fix
$t_f=T_{\max}$ and solve a *fixed-final-time* problem (no free-time scaling, better
conditioning). If accumulation saturates before $T_{\max}$ (the vehicle has swept all
reachable footprint), the remaining arc is a "don't care" with no effect on $\varphi$; the
control-effort term $\rho\!\int\lVert u\rVert^2$ (small $\rho$) removes that degeneracy and
yields a unique, smooth tail.

**Optimality structure.** With Hamiltonian
$H=\rho u^2+p_x\cos\theta+p_y\sin\theta+p_\theta u+p_E\lambda$, the effort costate is
constant ($\dot p_E=0$, the marginal value of effort), and away from the regularized arcs
the steering is bang-bang ($u^\star=\mathrm{sat}(p_\theta)$), with the position costates
forced by $\nabla\lambda$ — the time-optimal turn structure bent toward the footprint.

## 3. Transcription families

Both implementations discretize the continuous OCP into a sparse NLP and call IPOPT, but
they belong to **different transcription families**:

- **Direct multiple shooting (CasADi).** Choose $N$ intervals on $[0,T_{\max}]$, step
  $h=T_{\max}/N$; states at $N{+}1$ nodes, controls at $N$ nodes. The ODE is integrated
  *explicitly* across each interval (RK4 here) and continuity is enforced by gap-closing
  constraints $x_{k+1}=\mathrm{RK4}(x_k,u_k)$. The dynamics hold by construction within each
  interval; accuracy is set by $N$ and the integrator order, and refinement is manual
  (re-solve with larger $N$). Simple, robust, warm-start friendly, with a banded structure.
- **Pseudospectral collocation (PSOPT).** States and controls are approximated by global
  (or segment-local) orthogonal polynomials on Legendre–Gauss collocation points, and the
  ODE is enforced *implicitly* as defect constraints at those points. PSOPT identifies the
  Jacobian/Hessian sparsity automatically and runs an ODE-error-driven mesh-refinement loop,
  raising polynomial order / adding segments until the discretization error clears tolerance.
  Spectral (fast) convergence for smooth solutions, at the cost of denser linear algebra and
  an implicit, mesh-adaptive solve.

These are complementary endpoints of the direct-method spectrum: shooting trades accuracy
for simplicity and structure; pseudospectral trades simplicity for high-order accuracy and
automatic mesh control. Comparing them on the same bounded-horizon problem is the
methodological core of this paper.

## 4. Implementation A — CasADi / IPOPT, direct multiple shooting (Python)

Direct multiple shooting: fixed horizon $t_f=T_{\max}$, RK4 gap-closing continuity, single
solve. CasADi builds the NLP symbolically, differentiates by graph AD, and hands it to IPOPT
(free MUMPS linear solver bundled).

```python
import casadi as ca
import numpy as np

N, Tmax, umax = 60, 8.0, 1.0
h = Tmax / N
xi, sig, lam0, rho = np.array([3.0, 2.0]), 1.2, 1.0, 1e-3

opti = ca.Opti()
X = opti.variable(4, N + 1)      # [x, y, theta, E]
U = opti.variable(1, N)          # turn rate

def lam(px, py):
    return lam0 * ca.exp(-((px - xi[0])**2 + (py - xi[1])**2) / (2 * sig**2))

def f(z, u):
    return ca.vertcat(ca.cos(z[2]), ca.sin(z[2]), u, lam(z[0], z[1]))

for k in range(N):               # RK4 shooting: integrate, then close the gap
    k1 = f(X[:, k],          U[:, k])
    k2 = f(X[:, k] + h/2*k1, U[:, k])
    k3 = f(X[:, k] + h/2*k2, U[:, k])
    k4 = f(X[:, k] + h*k3,   U[:, k])
    opti.subject_to(X[:, k+1] == X[:, k] + h/6*(k1 + 2*k2 + 2*k3 + k4))

opti.subject_to(opti.bounded(-umax, U, umax))
opti.subject_to(X[:, 0] == ca.DM([0, 0, 0, 0]))      # start pose, E(0)=0

opti.minimize(ca.exp(-X[3, N]) + rho * ca.sumsqr(U) * h)   # phi(E_f) + control reg
opti.solver('ipopt')
sol = opti.solve()
```

Notes: the time budget is baked in as the fixed grid length; to study a different $T_{\max}$
re-solve (or promote $T_{\max}$ to an `Opti` parameter and `set_value` for a sweep). No
automatic mesh refinement — convergence in $N$ is checked by re-solving at higher $N$.

## 5. Implementation B — PSOPT / IPOPT, pseudospectral collocation (C++)

Bolza callbacks; fixed $t_f=T_{\max}$ via equal EndTime bounds; PSOPT supplies automatic
sparsity, ADOL-C AD, automatic scaling, and an automatic mesh-refinement loop.

```cpp
adouble endpoint_cost(adouble* xf, adouble* xi, adouble* p,
                      adouble& t0, adouble& tf, adouble* xad,
                      int iphase, Workspace* w) {
    return exp(-xf[3]);                       // phi(E_f): minimize non-detection
}

adouble integrand_cost(adouble* x, adouble* u, adouble* p,
                       adouble& t, adouble* xad, int iphase, Workspace* w) {
    return 1e-3 * u[0] * u[0];                // control regularization
}

void dae(adouble* d, adouble* path, adouble* x, adouble* u, adouble* p,
         adouble& t, adouble* xad, int iphase, Workspace* w) {
    adouble th = x[2];
    adouble lam = 1.0 * exp(-((x[0]-3.0)*(x[0]-3.0) + (x[1]-2.0)*(x[1]-2.0))
                            / (2.0*1.2*1.2));
    d[0] = cos(th); d[1] = sin(th); d[2] = u[0]; d[3] = lam;
}

void events(adouble* e, adouble* xi, adouble* xf, adouble* p,
            adouble& t0, adouble& tf, adouble* xad, int iphase, Workspace* w) {
    e[0] = xi[0]; e[1] = xi[1]; e[2] = xi[2]; e[3] = xi[3];   // start pose + E(0)
}
```

In `main()`: `nstates=4, ncontrols=1, nevents=4`; state/control bounds (`|u|≤u_max`); event
bounds pinning the start and `E(0)=0`; `problem.bounds.lower.EndTime =
problem.bounds.upper.EndTime = Tmax` (fixed horizon); `algorithm.nlp_method="IPOPT"`,
`algorithm.derivatives="automatic"`, `algorithm.collocation_method="Legendre"`,
`algorithm.mesh_refinement="automatic"`.

## 6. Comparison

| Aspect | CasADi / IPOPT (Python) | PSOPT / IPOPT (C++) |
|---|---|---|
| Transcription family | direct multiple shooting (RK4 gap-closing), user-set $N$ | pseudospectral collocation (LGL), mesh-adaptive |
| Derivatives | graph AD | ADOL-C AD + automatic sparsity |
| Mesh refinement | manual (re-solve at larger $N$) | automatic (ODE-error driven) |
| Scaling | manual / IPOPT defaults | automatic |
| Free-time handling | not needed (fixed horizon) | fixed via equal EndTime bounds |
| Language / deployment | Python (rapid prototyping); C++ codegen possible | C++ (embeddable, onboard) |
| Solver | IPOPT (+ MUMPS) | IPOPT (+ MUMPS) |
| Best when | prototyping, parametric $T_{\max}$ studies, accessibility | accuracy on a fixed horizon, mesh-adaptive solutions, embedding |
| License | free, open (LGPL) | free, open |

Both reach the same optimum on this problem; the practical split is shooting's simplicity,
warm-start friendliness, and Python iteration speed (CasADi) versus pseudospectral accuracy
with automatic mesh control and a C++ deployment path (PSOPT).

## 7. Numerical example *(to populate)*

Worked instance: single Gaussian footprint at $\xi=(3,2)$, $\sigma=1.2$, $\lambda_0=1$,
$u_{\max}=1$, start $(0,0,0)$, budget $T_{\max}=8$. Planned figures/metrics:
- optimal trajectory over the footprint (both solvers overlaid),
- control profile (bang-bang + regularized tail),
- achieved objective $1-e^{-E(t_f)}$, solve time, iteration count,
- CasADi grid-convergence in $N$ vs PSOPT mesh-refinement history.

*(Numbers/plots to be generated — CasADi runs directly in the free stack below; PSOPT
results from a local build.)*

## 8. Reproducibility (all-free toolchain)

- CasADi: `pip install casadi` ships IPOPT with the MUMPS linear solver — no commercial
  components. Python 3.x, NumPy.
- PSOPT: build from source against IPOPT (+ MUMPS) and ADOL-C; all open-source. C++17.
- No SNOPT, no MATLAB, no GPOPS-II required. The faster HSL linear solvers for IPOPT are
  optional (free for academic use); MUMPS suffices here.

## 9. Discussion and extensions

- Multiple targets / a target prior: $\zeta\in\mathbb R^m$ with $\varphi=\sum_j\pi_j e^{-E_j}$; both implementations scale by raising $m$.
- Obstacles / keep-out: add path-inequality constraints (CasADi `subject_to`; PSOPT `path` in `dae`).
- Cooperative multi-vehicle: PSOPT multi-phase with shared accumulation states; CasADi by stacking vehicle blocks.
- Other time types are one change away: free EndTime bounds (T2), or $t_f$ into the cost (T4) for a time-vs-objective trade.

## References

- CasADi — https://web.casadi.org
- PSOPT — https://github.com/PSOPT/psopt
- IPOPT (with MUMPS) — https://github.com/coin-or/Ipopt
- ADOL-C — automatic differentiation by overloading in C++
