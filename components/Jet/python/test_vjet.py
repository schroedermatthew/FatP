"""Validate vjet.VJet against the scalar jet.Jet (per point) and finite differences."""

# FATP_META:
#   meta_version: 1
#   component: Jet
#   file_role: test
#   path: components/Jet/python/test_vjet.py
#   language: python
#   layer: Foundation
#   summary: Per-point and broadcasting tests for the vectorized VJet port.
#   api_stability: in_work
#   related:
#     cpp_source: include/fat_p/Jet.h
#     tests_search: "test_vjet"

import math

import numpy as np

import jet            # scalar reference (already FD-validated)
import vjet
from vjet import VJet

rng = np.random.default_rng(0)
P = 7                                   # batch of points
xb = rng.uniform(0.2, 1.5, P)           # positive domain (safe for log/sqrt)
yb = rng.uniform(0.2, 1.5, P)
fails = []


def check(name, a, b, tol=1e-12):
    e = float(np.max(np.abs(np.asarray(a) - np.asarray(b))))
    ok = e < tol
    if not ok:
        fails.append(name)
    print(f"  {name:34s} max|diff| = {e:.2e}  {'OK' if ok else 'FAIL'}")


# function written once; runs on VJet (batch) or scalar Jet (one point)
def f(x, y, lib):
    return lib.atan2(lib.sin(x) * y + lib.sqrt(y), lib.exp(x) - y) + x ** 2


print("=== vectorized VJet vs scalar jet, per point ===")
X, Y = VJet.variables([xb, yb])
F = f(X, Y, vjet)                       # value (P,), partials (P, 2)

val_scalar = np.empty(P)
grad_scalar = np.empty((P, 2))
for i in range(P):
    xi, yi = jet.Jet.variables([float(xb[i]), float(yb[i])])
    fi = f(xi, yi, jet)
    val_scalar[i] = fi.value
    grad_scalar[i] = fi.partials

check("value (batch vs per-point)", F.value, val_scalar)
check("partials (batch vs per-point)", F.partials, grad_scalar)

print("=== VJet vs central finite differences ===")
h = 1e-6
fd = np.empty((P, 2))


def f_plain(x, y):                      # plain-float version for FD
    return math.atan2(math.sin(x) * y + math.sqrt(y), math.exp(x) - y) + x ** 2


for i in range(P):
    fd[i, 0] = (f_plain(xb[i] + h, yb[i]) - f_plain(xb[i] - h, yb[i])) / (2 * h)
    fd[i, 1] = (f_plain(xb[i], yb[i] + h) - f_plain(xb[i], yb[i] - h)) / (2 * h)
check("partials vs finite difference", F.partials, fd, tol=1e-6)

print("=== elementary functions, batch vs numpy ===")
xv = VJet.seed(xb, 0, 1)
for nm, vf, nf in [("sin", vjet.sin, np.sin), ("cos", vjet.cos, np.cos),
                   ("exp", vjet.exp, np.exp), ("log", vjet.log, np.log),
                   ("sqrt", vjet.sqrt, np.sqrt), ("atan", vjet.atan, np.arctan)]:
    check(f"{nm} value", vf(xv).value, nf(xb))

print("=== batched Jacobian R^2 -> R^2 (shape S+(M,N)) ===")
G0 = vjet.sin(X) * Y
G1 = vjet.sqrt(Y) - vjet.atan2(X, Y)
values, J = VJet.jacobian([G0, G1])
print(f"  values shape {values.shape}  J shape {J.shape}  (expect ({P}, 2) and ({P}, 2, 2))")
if values.shape != (P, 2) or J.shape != (P, 2, 2):
    fails.append("jacobian shape")
# spot-check row 0 of the per-point Jacobian against the scalar jet at point 0
xi, yi = jet.Jet.variables([float(xb[0]), float(yb[0])])
g0 = (jet.sin(xi) * yi).partials
g1 = (jet.sqrt(yi) - jet.atan2(xi, yi)).partials
check("Jacobian point 0 row 0", J[0, 0], g0)
check("Jacobian point 0 row 1", J[0, 1], g1)
empty_values, empty_J = VJet.jacobian([])
empty_ok = empty_values.shape == (0,) and empty_J.shape == (0, 0)
if not empty_ok:
    fails.append("empty jacobian shape")
print(f"  empty output shape values {empty_values.shape}  J {empty_J.shape}  {'OK' if empty_ok else 'FAIL'}")

print("=== one-pass gradients over a grid (50 points) ===")
t = np.linspace(0.1, 2.0, 50)
xg, yg = VJet.variables([t, t ** 0.5])
fg = vjet.exp(xg) * vjet.cos(yg)
print(f"  evaluated f and df/d(x,y) at {fg.value.shape[0]} points in one call; "
      f"partials shape {fg.partials.shape}")
if fg.partials.shape != (50, 2):
    fails.append("grid shape")

print("=== scalar-overload parity: VJet / scalar-0 and VJet(inf) * 0 (mirror C++) ===")
def _eqv(a, b):
    return (np.isnan(a) and np.isnan(b)) or a == b
for nm, r, ev, ed in [
    ("VJet(1)/0.0",   VJet.seed(np.array(1.0), 0, 1) / 0.0,       np.inf,  np.inf),
    ("VJet(1)/-0.0",  VJet.seed(np.array(1.0), 0, 1) / -0.0,     -np.inf, -np.inf),
    ("VJet(-1)/0.0",  VJet.seed(np.array(-1.0), 0, 1) / 0.0,     -np.inf,  np.inf),
    ("VJet(inf)*0.0", VJet.seed(np.array(np.inf), 0, 1) * 0.0,    np.nan,  0.0),
]:
    ok = _eqv(float(r.value), ev) and _eqv(float(r.partials[0]), ed)
    if not ok:
        fails.append(nm)
    print(f"  {nm:16s} value={float(r.value)}  d={float(r.partials[0])}  (want {ev} / {ed})  {'OK' if ok else 'FAIL'}")

print("=== non-positive-base pow parity: value kept, gradient NaN (mirror C++) ===")
for nm, r, ev, ed in [
    ("(-2)**V(3)",   (-2.0) ** VJet.seed(np.array(3.0), 0, 1),  -8.0,    np.nan),
    ("(-2)**V(0.5)", (-2.0) ** VJet.seed(np.array(0.5), 0, 1),   np.nan, np.nan),
    ("0**V(2)",      (0.0) ** VJet.seed(np.array(2.0), 0, 1),    0.0,    np.nan),
    ("0**V(-1)",     (0.0) ** VJet.seed(np.array(-1.0), 0, 1),   np.inf, np.nan),
    ("V(-2)**V(3)",  VJet.seed(np.array(-2.0), 0, 1) ** VJet.constant(np.array(3.0), 1), -8.0, np.nan),
]:
    ok = _eqv(float(r.value), ev) and _eqv(float(r.partials[0]), ed)
    if not ok:
        fails.append(nm)
    print(f"  {nm:14s} value={float(r.value)}  d={float(r.partials[0])}  (want {ev} / {ed})  {'OK' if ok else 'FAIL'}")

print("=== NumPy broadcasting and array-valued powers ===")
xb = VJet.seed(np.arange(3.0).reshape(3, 1), 0, 1)
yb = np.arange(4.0).reshape(1, 4) + 1.0
for tag, r in [("x*y", xb * yb), ("x/y", xb / yb), ("y*x", yb * xb), ("y/x", yb / xb)]:
    ok = isinstance(r, VJet) and r.value.shape == (3, 4) and r.partials.shape == (3, 4, 1)
    if not ok:
        fails.append("broadcast " + tag)
    shp = r.value.shape if isinstance(r, VJet) else "not-VJet"
    print(f"  {tag:5s} -> {type(r).__name__:5s} value {shp}  {'OK' if ok else 'FAIL'}")
xe = VJet.seed(np.array([2.0, 3.0, 4.0]), 0, 1)
pe = np.array([2.0, 3.0, 0.5])
rp = xe ** pe
okp = np.allclose(rp.value, xe.value ** pe) and np.allclose(rp.partials[:, 0], pe * xe.value ** (pe - 1.0))
rb = np.array([2.0, 3.0, 10.0]) ** xe
okb = np.allclose(rb.value, np.array([2.0, 3.0, 10.0]) ** xe.value)
if not (okp and okb):
    fails.append("array power")
print(f"  x ** [2,3,0.5] / [2,3,10] ** x   {'OK' if okp and okb else 'FAIL'}")

print("=== signed-zero pow parity: VJet(-0.0) ** p vs scalar/std::pow ===")
def _exactv(a, b):
    if math.isnan(a) and math.isnan(b):
        return True
    if a == 0.0 and b == 0.0:
        return math.copysign(1.0, a) == math.copysign(1.0, b)
    return a == b
for p, ev, ed in [
    (-3.0, -math.inf, -math.inf), (-2.0, math.inf, math.inf), (-1.0, -math.inf, -math.inf),
    # Finding B: 0 < p < 1 at zero has slope 0 (non-poisoning convention),
    # matching sqrt; previously this row expected +inf.
    (0.0, 1.0, 0.0), (0.5, 0.0, 0.0), (1.0, -0.0, 1.0), (1.5, 0.0, 0.0),
    (2.0, 0.0, -0.0), (3.0, -0.0, 0.0),
]:
    r = VJet.seed(np.array(-0.0), 0, 1) ** p
    rv, rd = float(r.value), float(r.partials[0])
    ok = _exactv(rv, ev) and _exactv(rd, ed)
    if not ok:
        fails.append(f"vjet signed-zero pow {p}")
    print(f"  (-0.0)**{p:+} value={rv!r} d={rd!r}  (want {ev!r} / {ed!r})  {'OK' if ok else 'FAIL'}")

print("=== signed-zero pow parity: exponent-differentiated forms (+0 value, NaN grad) ===")
for nm, r in [
    ("(-0.0) ** V(0.5)",  (-0.0) ** VJet.seed(np.array(0.5), 0, 1)),
    ("V(-0.0) ** V(0.5)", VJet.seed(np.array(-0.0), 0, 1) ** VJet.constant(np.array(0.5), 1)),
]:
    rv, rd = float(r.value), float(r.partials[0])
    ok = rv == 0.0 and math.copysign(1.0, rv) > 0.0 and math.isnan(rd)
    if not ok:
        fails.append("expdiff signed-zero " + nm)
    print(f"  {nm:18s} value={rv!r} (sign {'+' if math.copysign(1.0, rv) > 0 else '-'}) grad={rd!r}  {'OK' if ok else 'FAIL'}")

print()
print("=== division extreme-magnitude regressions (Class X), with warnings-as-errors ===")
import warnings as _warnings
with _warnings.catch_warnings():
    _warnings.simplefilter("error")  # the structural-zero guard must not leak a numpy warning
    _cases = [
        ("seed0 / const",
         VJet.seed(np.array([1.0]), 0, 2) / VJet.constant(np.array([1e-310]), 2),
         (math.inf, 0.0)),
        ("seed0 / seed1",
         VJet.seed(np.array([1.0]), 0, 2) / VJet.seed(np.array([1e-310]), 1, 2),
         (math.inf, -math.inf)),
        ("1.0 / seed0",
         1.0 / VJet.seed(np.array([1e-310]), 0, 2),
         (-math.inf, 0.0)),
    ]
    for _nm, _r, (_e0, _e1) in _cases:
        _p = [float(_r.partials[0, 0]), float(_r.partials[0, 1])]
        _ok = (_p[0] == _e0) and (_p[1] == _e1)
        if not _ok:
            fails.append("vjet Class X " + _nm)
        print(f"  {_nm:14s} partials={_p}  (want [{_e0}, {_e1}])  {'OK' if _ok else 'FAIL'}")

print()
if fails:
    raise SystemExit(f"FAILED: {fails}")
print("ALL TESTS PASSED")
