"""Validate jet.Jet against finite differences. Run: python3 test_jet.py"""

# FATP_META:
#   meta_version: 1
#   component: Jet
#   file_role: test
#   path: components/Jet/python/test_jet.py
#   language: python
#   layer: Foundation
#   summary: Finite-difference and edge-case tests for the Python Jet port.
#   api_stability: in_work
#   related:
#     cpp_source: include/fat_p/Jet.h
#     tests_search: "test_jet"

import math

from jet import Jet, sin, cos, tan, asin, acos, atan, exp, log, sqrt, hypot, atan2

H = 1e-6
TOL = 1e-6
_fails = []


def report(name, ad, fd, tol=TOL):
    err = abs(ad - fd)
    ok = err < tol
    if not ok:
        _fails.append(name)
    print(f"  {name:18s} AD={ad:+.10f}  FD={fd:+.10f}  |d|={err:.1e}  {'OK' if ok else 'FAIL'}")


def note(name, val, ok, expect):
    if not ok:
        _fails.append(name)
    print(f"  {name:26s} = {val:.3e}  ({expect})  {'OK' if ok else 'FAIL'}")


def fd1(f, x0):
    return (f(x0 + H) - f(x0 - H)) / (2 * H)


print("=== unary derivatives (same function dispatches Jet and float) ===")
for nm, f, x0 in [
    ("sin", sin, 0.7), ("cos", cos, 0.7), ("tan", tan, 0.7),
    ("exp", exp, 0.7), ("log", log, 2.3), ("sqrt", sqrt, 2.3),
    ("asin", asin, 0.4), ("acos", acos, 0.4), ("atan", atan, 0.7),
    ("abs", abs, 1.7),
]:
    y = f(Jet.seed(x0, 0, 1))
    report(nm, y.partials[0], fd1(f, x0))

print("=== operators ===")
x = Jet.seed(1.7, 0, 1)
report("x*x", (x * x).partials[0], fd1(lambda t: t * t, 1.7))
report("1/x", (1.0 / x).partials[0], fd1(lambda t: 1.0 / t, 1.7))
report("x - 2/x", (x - 2.0 / x).partials[0], fd1(lambda t: t - 2.0 / t, 1.7))
report("3*x", (3 * x).partials[0], fd1(lambda t: 3 * t, 1.7))

print("=== overflow guards (FD underflows; check finiteness/sign) ===")
ya = atan(Jet.seed(1e155, 0, 1))
note("atan(1e155)", ya.partials[0], math.isfinite(ya.partials[0]) and ya.partials[0] > 0, "~1e-310, >0")
yb = atan2(Jet.seed(1e308, 0, 2), Jet.seed(1e308, 1, 2))
note("atan2(1e308,1e308) d/dy", yb.partials[0],
     math.isfinite(yb.partials[0]) and yb.partials[0] > 0, "~5e-309, >0")

print("=== two-argument functions ===")
yv, xv = 1.3, 2.1
yj = hypot(Jet.seed(yv, 0, 2), Jet.seed(xv, 1, 2))
report("hypot d/dy", yj.partials[0], (math.hypot(yv + H, xv) - math.hypot(yv - H, xv)) / (2 * H))
report("hypot d/dx", yj.partials[1], (math.hypot(yv, xv + H) - math.hypot(yv, xv - H)) / (2 * H))
yj = atan2(Jet.seed(yv, 0, 2), Jet.seed(xv, 1, 2))
report("atan2 d/dy", yj.partials[0], (math.atan2(yv + H, xv) - math.atan2(yv - H, xv)) / (2 * H))
report("atan2 d/dx", yj.partials[1], (math.atan2(yv, xv + H) - math.atan2(yv, xv - H)) / (2 * H))

print("=== powers ===")
report("x**3", (Jet.seed(1.7, 0, 1) ** 3).partials[0], fd1(lambda t: t ** 3, 1.7))
report("(-2)**4 (real)", (Jet.seed(-2.0, 0, 1) ** 4).partials[0], fd1(lambda t: t ** 4, -2.0))
report("2**y", (2 ** Jet.seed(1.3, 0, 1)).partials[0], fd1(lambda t: 2 ** t, 1.3))
bv, ev = 1.4, 0.9
yj = Jet.seed(bv, 0, 2) ** Jet.seed(ev, 1, 2)
report("x**y d/dx", yj.partials[0], (math.pow(bv + H, ev) - math.pow(bv - H, ev)) / (2 * H))
report("x**y d/dy", yj.partials[1], (math.pow(bv, ev + H) - math.pow(bv, ev - H)) / (2 * H))

print("=== composite gradient: f = atan2(sin(x1)*x2, sqrt(x2)+x1) ===")
v1, v2 = 0.6, 1.8
x1, x2 = Jet.variables([v1, v2])
yc = atan2(sin(x1) * x2, sqrt(x2) + x1)
Fs = lambda a, b: math.atan2(math.sin(a) * b, math.sqrt(b) + a)
report("df/dx1", yc.partials[0], (Fs(v1 + H, v2) - Fs(v1 - H, v2)) / (2 * H))
report("df/dx2", yc.partials[1], (Fs(v1, v2 + H) - Fs(v1, v2 - H)) / (2 * H))

print("=== Jacobian R^3 -> R^2 ===")
xv3 = [0.6, 1.8, -0.4]
a, b, c = Jet.variables(xv3)
Y = [sin(a) * b + exp(c), sqrt(b ** 2 + 1) - atan2(c, a)]
_, J = Jet.jacobian(Y)


def Fv(z):
    return [math.sin(z[0]) * z[1] + math.exp(z[2]),
            math.sqrt(z[1] ** 2 + 1) - math.atan2(z[2], z[0])]


Jfd = [[0.0] * 3 for _ in range(2)]
for j in range(3):
    zp, zm = list(xv3), list(xv3)
    zp[j] += H
    zm[j] -= H
    fp, fm = Fv(zp), Fv(zm)
    for i in range(2):
        Jfd[i][j] = (fp[i] - fm[i]) / (2 * H)
emax = max(abs(J[i][j] - Jfd[i][j]) for i in range(2) for j in range(3))
note("Jacobian max |AD-FD|", emax, emax < TOL, "over all 6 entries")

print("=== real-domain NaN (must not raise) ===")
checks = [
    ("sqrt(-1)", sqrt(Jet.seed(-1.0, 0, 1))),
    ("log(-1)", log(Jet.seed(-1.0, 0, 1))),
    ("asin(2)", asin(Jet.seed(2.0, 0, 1))),
    ("(-2)**0.5", Jet.seed(-2.0, 0, 1) ** 0.5),
]
for nm, r in checks:
    ok = math.isnan(r.value) and all(math.isnan(g) for g in r.partials)
    if not ok:
        _fails.append(nm)
    print(f"  {nm:18s} value={r.value}  partials={r.partials}  {'OK' if ok else 'FAIL'}")

print("=== scalar-overload parity: Jet / scalar-0 and Jet(inf) * 0 (mirror C++) ===")
def _eq(a, b):  # exact match including inf sign and nan
    return (math.isnan(a) and math.isnan(b)) or a == b
for nm, r, ev, ed in [
    ("Jet(1)/0.0",   Jet.seed(1.0, 0, 1) / 0.0,       math.inf,  math.inf),
    ("Jet(1)/-0.0",  Jet.seed(1.0, 0, 1) / -0.0,     -math.inf, -math.inf),
    ("Jet(-1)/0.0",  Jet.seed(-1.0, 0, 1) / 0.0,     -math.inf,  math.inf),
    ("Jet(inf)*0.0", Jet.seed(math.inf, 0, 1) * 0.0,  math.nan,  0.0),
]:
    ok = _eq(r.value, ev) and _eq(r.partials[0], ed)
    if not ok:
        _fails.append(nm)
    print(f"  {nm:16s} value={r.value}  d={r.partials[0]}  (want {ev} / {ed})  {'OK' if ok else 'FAIL'}")

print("=== non-positive-base pow parity: value kept, gradient NaN (mirror C++) ===")
for nm, r, ev, ed in [
    ("(-2)**J(3)",   (-2.0) ** Jet.seed(3.0, 0, 1),   -8.0,      math.nan),
    ("(-2)**J(0.5)", (-2.0) ** Jet.seed(0.5, 0, 1),    math.nan, math.nan),
    ("0**J(2)",      (0.0) ** Jet.seed(2.0, 0, 1),     0.0,      math.nan),
    ("0**J(-1)",     (0.0) ** Jet.seed(-1.0, 0, 1),    math.inf, math.nan),
    ("J(-2)**J(3)",  Jet.seed(-2.0, 0, 1) ** Jet.constant(3.0, 1), -8.0, math.nan),
]:
    ok = _eq(r.value, ev) and _eq(r.partials[0], ed)
    if not ok:
        _fails.append(nm)
    print(f"  {nm:14s} value={r.value}  d={r.partials[0]}  (want {ev} / {ed})  {'OK' if ok else 'FAIL'}")

print("=== signed-zero pow parity: (-0.0) ** p (value, deriv) vs C++/std::pow ===")
def _exact(a, b):  # exact match including sign of zero and inf
    if math.isnan(a) and math.isnan(b):
        return True
    if a == 0.0 and b == 0.0:
        return math.copysign(1.0, a) == math.copysign(1.0, b)
    return a == b
for p, ev, ed in [
    (-3.0, -math.inf, -math.inf), (-2.0, math.inf, math.inf), (-1.0, -math.inf, -math.inf),
    (1.0, -0.0, 1.0), (2.0, 0.0, -0.0), (3.0, -0.0, 0.0),
]:
    r = Jet.seed(-0.0, 0, 1) ** p
    ok = _exact(r.value, ev) and _exact(r.partials[0], ed)
    if not ok:
        _fails.append(f"signed-zero pow {p}")
    print(f"  (-0.0)**{p:+} value={r.value!r} d={r.partials[0]!r}  (want {ev!r} / {ed!r})  {'OK' if ok else 'FAIL'}")

print()
print("=== division extreme-magnitude regressions (Class X) + Finding B alignment ===")
# Class X: value overflow must not poison structurally-zero directions to NaN.
_ax = Jet.seed(1.0, 0, 2) / Jet.constant(1e-310, 2)   # value +inf; want (+inf, 0)
if not (math.isinf(_ax.partials[0]) and _ax.partials[1] == 0.0):
    _fails.append("Class X jet/jet const-denom")
print(f"  X jet/jet const-denom: {_ax.partials}  (want (inf, 0.0))")
_bx = Jet.constant(1.0, 2) / Jet.seed(1e-310, 0, 2)   # value +inf; want (-inf, 0)
if not (math.isinf(_bx.partials[0]) and _bx.partials[1] == 0.0):
    _fails.append("Class X scalar/jet")
print(f"  X scalar/jet:          {_bx.partials}  (want (-inf, 0.0))")
# Finding B: sqrt(x) and x ** 0.5 agree, including slope-0 convention at 0.
_sq0 = sqrt(Jet.seed(0.0, 0, 1)).partials[0]
_pw0 = (Jet.seed(0.0, 0, 1) ** 0.5).partials[0]
if not (_sq0 == 0.0 and _pw0 == 0.0):
    _fails.append("Finding B sqrt/pow(0,0.5) alignment")
print(f"  Finding B at 0: sqrt'={_sq0}  (x**0.5)'={_pw0}  (want 0.0 / 0.0)")
_sq4 = sqrt(Jet.seed(4.0, 0, 1)).partials[0]
_pw4 = (Jet.seed(4.0, 0, 1) ** 0.5).partials[0]
if not (abs(_sq4 - 0.25) < 1e-15 and abs(_pw4 - 0.25) < 1e-15):
    _fails.append("Finding B sqrt/pow alignment at 4")
print(f"  Finding B at 4: sqrt'={_sq4}  (x**0.5)'={_pw4}  (want 0.25 / 0.25)")

print()
if _fails:
    raise SystemExit(f"FAILED: {_fails}")
print("ALL TESTS PASSED")
