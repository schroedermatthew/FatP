"""Single-segment Dubins shortest path over the Python Jet, as a cross-check.

The geometry is the same closed form as the C++ dubins_geometry.h, written once
over a scalar so it runs with plain floats (the path) and with Jets (length plus
exact gradient). This script reproduces the recorded C++ result for the scenario
start (0,0,0) -> goal (3,3,pi/2), rho = 1: word LSL, length pi/2 + 2*sqrt(2),
and gradient dL/d(x1,y1,th1,rho) = (0.70710678, 0.70710678, 0.29289322,
0.15658276), confirming the Python Jet agrees with the C++ Jet on a real problem.

Run: python3 dubins_jet.py
"""

import math

from jet import Jet, sin, cos, sqrt, hypot, atan2, acos

TWO_PI = 2.0 * math.pi


def value_of(x):
    return x.value if isinstance(x, Jet) else float(x)


def mod2pi(x):
    # x - 2*pi*floor(x/2*pi); for a Jet this subtracts a constant, so the
    # derivative passes through unchanged.
    k = math.floor(value_of(x) / TWO_PI)
    return x - TWO_PI * k


def word_lsl(a, b, d):
    psq = 2.0 + d * d - 2.0 * cos(a - b) + 2.0 * d * (sin(a) - sin(b))
    if value_of(psq) < 0.0:
        return None
    u = atan2(cos(b) - cos(a), d + sin(a) - sin(b))
    return mod2pi(-a + u), sqrt(psq), mod2pi(b - u)


def word_rsr(a, b, d):
    psq = 2.0 + d * d - 2.0 * cos(a - b) + 2.0 * d * (sin(b) - sin(a))
    if value_of(psq) < 0.0:
        return None
    u = atan2(cos(a) - cos(b), d - sin(a) + sin(b))
    return mod2pi(a - u), sqrt(psq), mod2pi(-b + u)


def word_lsr(a, b, d):
    psq = -2.0 + d * d + 2.0 * cos(a - b) + 2.0 * d * (sin(a) + sin(b))
    if value_of(psq) < 0.0:
        return None
    p = sqrt(psq)
    u = atan2(-cos(a) - cos(b), d + sin(a) + sin(b)) - atan2(-2.0, p)
    return mod2pi(-a + u), p, mod2pi(-b + u)


def word_rsl(a, b, d):
    psq = -2.0 + d * d + 2.0 * cos(a - b) - 2.0 * d * (sin(a) + sin(b))
    if value_of(psq) < 0.0:
        return None
    p = sqrt(psq)
    u = atan2(cos(a) + cos(b), d - sin(a) - sin(b)) - atan2(2.0, p)
    return mod2pi(a - u), p, mod2pi(b - u)


def word_rlr(a, b, d):
    tmp = (6.0 - d * d + 2.0 * cos(a - b) + 2.0 * d * (sin(a) - sin(b))) / 8.0
    if abs(value_of(tmp)) > 1.0:
        return None
    p = mod2pi(TWO_PI - acos(tmp))
    t = mod2pi(a - atan2(cos(a) - cos(b), d - sin(a) + sin(b)) + mod2pi(p / 2.0))
    q = mod2pi(a - b - t + mod2pi(p))
    return t, p, q


def word_lrl(a, b, d):
    tmp = (6.0 - d * d + 2.0 * cos(a - b) + 2.0 * d * (-sin(a) + sin(b))) / 8.0
    if abs(value_of(tmp)) > 1.0:
        return None
    p = mod2pi(TWO_PI - acos(tmp))
    t = mod2pi(-a - atan2(cos(a) - cos(b), d + sin(a) - sin(b)) + p / 2.0)
    q = mod2pi(mod2pi(b) - a - t + mod2pi(p))
    return t, p, q


WORDS = [("LSL", word_lsl), ("LSR", word_lsr), ("RSL", word_rsl),
         ("RSR", word_rsr), ("RLR", word_rlr), ("LRL", word_lrl)]


def dubins_shortest(x0, y0, th0, x1, y1, th1, rho):
    """Return (word_name, t, p, q, length) for the shortest valid word."""
    dx = x1 - x0
    dy = y1 - y0
    d = hypot(dx, dy) / rho
    theta = mod2pi(atan2(dy, dx))
    alpha = mod2pi(th0 - theta)
    beta = mod2pi(th1 - theta)

    best = math.inf
    chosen = None
    for name, fn in WORDS:
        seg = fn(alpha, beta, d)
        if seg is None:
            continue
        t, p, q = seg
        total = t + p + q
        lv = value_of(total)
        if lv < best:
            best = lv
            chosen = (name, t, p, q, total * rho)
    return chosen


def main():
    x0, y0, th0 = 0.0, 0.0, 0.0
    x1, y1, th1 = 3.0, 3.0, math.pi / 2.0
    rho = 1.0

    # --- path with plain floats -----------------------------------------
    name, t, p, q, length = dubins_shortest(x0, y0, th0, x1, y1, th1, rho)
    print(f"word = {name}   (t,p,q) = ({t:.4f}, {p:.4f}, {q:.4f})   length = {length:.6f}")

    # --- length + gradient with Jets (seed x1,y1,th1,rho; start fixed) ---
    jx1 = Jet.seed(x1, 0, 4)
    jy1 = Jet.seed(y1, 1, 4)
    jth1 = Jet.seed(th1, 2, 4)
    jrho = Jet.seed(rho, 3, 4)
    jx0 = Jet.constant(x0, 4)
    jy0 = Jet.constant(y0, 4)
    jth0 = Jet.constant(th0, 4)
    name_j, _, _, _, jlen = dubins_shortest(jx0, jy0, jth0, jx1, jy1, jth1, jrho)
    grad = jlen.partials

    labels = ["dL/dx1", "dL/dy1", "dL/dth1", "dL/drho"]
    print(f"\nactive word matches float path: {'yes' if name_j == name else 'NO'}")
    for lab, g in zip(labels, grad):
        print(f"  {lab:8s} = {g:+.8f}")

    # --- compare to the recorded C++ result -----------------------------
    cpp = {"length": math.pi / 2 + 2 * math.sqrt(2),
           "grad": (0.70710678, 0.70710678, 0.29289322, 0.15658276)}
    print("\n=== vs recorded C++ gradient ===")
    dmax = abs(jlen.value - cpp["length"])
    for lab, g, gc in zip(labels, grad, cpp["grad"]):
        dmax = max(dmax, abs(g - gc))
        print(f"  {lab:8s} python={g:+.8f}  C++={gc:+.8f}  |d|={abs(g-gc):.1e}")
    print(f"  length    python={jlen.value:.6f}  C++={cpp['length']:.6f}")
    print(f"max |python - C++| = {dmax:.2e}")

    # --- finite-difference cross-check (float path) ---------------------
    def L(a, b, c, e):
        return dubins_shortest(x0, y0, th0, a, b, c, e)[4]

    h = 1e-6
    fd = [
        (L(x1 + h, y1, th1, rho) - L(x1 - h, y1, th1, rho)) / (2 * h),
        (L(x1, y1 + h, th1, rho) - L(x1, y1 - h, th1, rho)) / (2 * h),
        (L(x1, y1, th1 + h, rho) - L(x1, y1, th1 - h, rho)) / (2 * h),
        (L(x1, y1, th1, rho + h) - L(x1, y1, th1, rho - h)) / (2 * h),
    ]
    emax = max(abs(g - f) for g, f in zip(grad, fd))
    print(f"\nmax |Jet - finite difference| = {emax:.2e}")

    assert dmax < 1e-6, "Python gradient disagrees with the C++ result"
    assert emax < 1e-6, "Python gradient disagrees with finite differences"
    print("\nCROSS-CHECK PASSED: the Python Jet reproduces the C++ Dubins gradient.")


if __name__ == "__main__":
    main()
