"""Vectorized forward-mode automatic differentiation over NumPy arrays.

Where :class:`jet.Jet` carries one scalar value with ``N`` partials, a
:class:`VJet` carries an array of values of shape ``S`` with partials of shape
``S + (N,)`` -- the partials along a trailing axis. One evaluation then yields
the value and the gradient at every point of a batch at once: seed ``N`` input
arrays of shape ``S`` and read a partials array of shape ``S + (N,)`` whose
``[..., k]`` slice is the derivative with respect to input ``k``.

This module requires NumPy. The derivative formulas and the overflow/domain
guards match the scalar :mod:`jet` (and the C++/MATLAB ports); out-of-domain
points carry NaN, and the ``atan``/``atan2`` reciprocal-scaling guards are kept.
Seeding is 0-based, as in the scalar version.

Example
-------
>>> import numpy as np
>>> from vjet import VJet, sin
>>> x = VJet.seed(np.array([0.0, 0.5, 1.0]), 0, 1)   # 3 points, 1 variable
>>> y = sin(x)
>>> np.allclose(y.value, np.sin([0.0, 0.5, 1.0]))
True
>>> np.allclose(y.partials[:, 0], np.cos([0.0, 0.5, 1.0]))
True
"""

# FATP_META:
#   meta_version: 1
#   component: Jet
#   file_role: port_module
#   path: Jet/python/vjet.py
#   language: python
#   layer: Foundation
#   summary: Vectorized forward-mode AD over NumPy arrays; companion to the Python Jet.
#   api_stability: in_work
#   related:
#     cpp_source: include/fat_p/Jet.h
#     docs_search: "Jet"
#     tests_search: "test_vjet"
#   hygiene:
#     requires: numpy

from __future__ import annotations

from typing import Sequence, Tuple, Union

import numpy as np

__all__ = [
    "VJet",
    "sin", "cos", "tan", "asin", "acos", "atan",
    "exp", "log", "sqrt", "hypot", "atan2",
]

ArrayLike = Union["VJet", np.ndarray, float, int]
_ERR = dict(invalid="ignore", divide="ignore", over="ignore")


class VJet:
    """An array of values, each travelling with its ``N`` partial derivatives."""

    __slots__ = ("value", "partials")
    __array_priority__ = 1000.0  # win dispatch over ndarray in mixed array-VJet ops

    def __init__(self, value, partials) -> None:
        p = np.asarray(partials, dtype=float)
        v = np.asarray(value, dtype=float)
        if v.shape != p.shape[:-1]:
            v = np.broadcast_to(v, p.shape[:-1]).copy()
        self.value: np.ndarray = v
        self.partials: np.ndarray = p

    @property
    def n(self) -> int:
        """Number of partial-derivative directions."""
        return self.partials.shape[-1]

    # -- construction --------------------------------------------------------
    @classmethod
    def constant(cls, value, n: int) -> "VJet":
        """A constant array (zero partials)."""
        v = np.asarray(value, dtype=float)
        return cls(v, np.zeros(v.shape + (n,)))

    @classmethod
    def seed(cls, value, k: int, n: int) -> "VJet":
        """A variable array: unit derivative in direction ``k`` (0-based) at every point."""
        if not 0 <= k < n:
            raise ValueError(f"direction k={k} out of range 0..{n - 1}")
        v = np.asarray(value, dtype=float)
        p = np.zeros(v.shape + (n,))
        p[..., k] = 1.0
        return cls(v, p)

    @classmethod
    def variables(cls, values: Sequence) -> Tuple["VJet", ...]:
        """A tuple of seeded VJets from ``N`` input arrays, broadcast to a common shape."""
        arrs = np.broadcast_arrays(*[np.asarray(a, dtype=float) for a in values])
        n = len(arrs)
        return tuple(cls.seed(arrs[i], i, n) for i in range(n))

    # -- readout -------------------------------------------------------------
    @property
    def gradient(self) -> np.ndarray:
        """The partials (shape ``S + (N,)``)."""
        return self.partials

    @staticmethod
    def jacobian(outputs: Sequence["VJet"]) -> Tuple[np.ndarray, np.ndarray]:
        """Values (shape ``S + (M,)``) and per-point Jacobian (shape ``S + (M, N)``)."""
        outs = list(outputs)
        values = np.stack([o.value for o in outs], axis=-1)
        jac = np.stack([o.partials for o in outs], axis=-2)
        return values, jac

    # -- arithmetic ----------------------------------------------------------
    def __add__(self, other: ArrayLike) -> "VJet":
        av, ap, bv, bp = _parts(self, other)
        return VJet(av + bv, ap + bp)

    __radd__ = __add__

    def __sub__(self, other: ArrayLike) -> "VJet":
        av, ap, bv, bp = _parts(self, other)
        return VJet(av - bv, ap - bp)

    def __rsub__(self, other: ArrayLike) -> "VJet":
        av, ap, bv, bp = _parts(other, self)
        return VJet(av - bv, ap - bp)

    def __mul__(self, other: ArrayLike) -> "VJet":
        if not isinstance(other, VJet):  # VJet * array-or-scalar: mirrors C++ Jet*double
            with np.errstate(**_ERR):
                fac = np.asarray(other, dtype=float)
                value = self.value * fac                        # broadcast to the result shape
                fac_b = np.broadcast_to(fac, value.shape)
                part_b = np.broadcast_to(self.partials, value.shape + (self.n,))
                return VJet(value, part_b * fac_b[..., None])
        av, ap, bv, bp = _parts(self, other)
        return VJet(av * bv, av[..., None] * bp + bv[..., None] * ap)

    __rmul__ = __mul__

    def __truediv__(self, other: ArrayLike) -> "VJet":
        if not isinstance(other, VJet):  # VJet / array-or-scalar: mirrors C++ Jet/double
            with np.errstate(**_ERR):
                den = np.asarray(other, dtype=float)
                value = self.value / den                        # broadcast to the result shape
                den_b = np.broadcast_to(den, value.shape)
                part_b = np.broadcast_to(self.partials, value.shape + (self.n,))
                return VJet(value, part_b / den_b[..., None])
        av, ap, bv, bp = _parts(self, other)
        with np.errstate(**_ERR):
            value = av / bv
            denom = bv[..., None]
            # structural-zero guard (Class X): where a denominator partial is 0,
            # the cross-term value*bp is structurally zero but becomes inf*0 = NaN
            # under value overflow; ap/denom bypasses the overflowed value.
            # np.where broadcasts correctly; the eager inf*0 in the unused branch
            # is masked out and its warning suppressed by np.errstate(**_ERR).
            part = np.where(bp != 0.0,
                            (ap - value[..., None] * bp) / denom,
                            ap / denom)
        return VJet(value, part)

    def __rtruediv__(self, other: ArrayLike) -> "VJet":
        av, ap, bv, bp = _parts(other, self)
        with np.errstate(**_ERR):
            value = av / bv
            denom = bv[..., None]
            # structural-zero guard (Class X): where a denominator partial is 0,
            # the cross-term value*bp is structurally zero but becomes inf*0 = NaN
            # under value overflow; ap/denom bypasses the overflowed value.
            # np.where broadcasts correctly; the eager inf*0 in the unused branch
            # is masked out and its warning suppressed by np.errstate(**_ERR).
            part = np.where(bp != 0.0,
                            (ap - value[..., None] * bp) / denom,
                            ap / denom)
        return VJet(value, part)

    def __neg__(self) -> "VJet":
        return VJet(-self.value, -self.partials)

    def __pos__(self) -> "VJet":
        return self

    def __abs__(self) -> "VJet":
        s = np.sign(self.value)
        return VJet(np.abs(self.value), s[..., None] * self.partials)

    def __pow__(self, other: ArrayLike) -> "VJet":
        if isinstance(other, VJet):
            return _pow_jj(self, other)
        return _pow_js(self, np.asarray(other, dtype=float))

    def __rpow__(self, base) -> "VJet":
        return _pow_sj(np.asarray(base, dtype=float), self)

    # -- comparisons (value only, elementwise) -------------------------------
    def __lt__(self, other): return self.value < _val(other)
    def __le__(self, other): return self.value <= _val(other)
    def __gt__(self, other): return self.value > _val(other)
    def __ge__(self, other): return self.value >= _val(other)
    def __eq__(self, other): return self.value == _val(other)
    def __ne__(self, other): return self.value != _val(other)
    __hash__ = None

    def __repr__(self) -> str:
        return f"VJet(shape={self.value.shape}, n={self.n})"


# ---- internal helpers ------------------------------------------------------
def _val(x: ArrayLike) -> np.ndarray:
    return x.value if isinstance(x, VJet) else np.asarray(x, dtype=float)


def _parts(a: ArrayLike, b: ArrayLike):
    """Return value/partial pairs, promoting a plain array/scalar to zero partials."""
    if isinstance(a, VJet) and isinstance(b, VJet):
        if a.n != b.n:
            raise ValueError(f"partial-count mismatch: {a.n} vs {b.n}")
        return a.value, a.partials, b.value, b.partials
    if isinstance(a, VJet):
        bv = np.asarray(b, dtype=float)
        return a.value, a.partials, bv, np.zeros(bv.shape + (a.n,))
    av = np.asarray(a, dtype=float)
    return av, np.zeros(av.shape + (b.n,)), b.value, b.partials  # type: ignore[union-attr]


def _scale(deriv: np.ndarray, partials: np.ndarray) -> np.ndarray:
    """Multiply each partial by a per-point derivative (broadcast over the N axis)."""
    return deriv[..., None] * partials


def _pow_js(x: VJet, p) -> VJet:
    """``x ** p`` with a constant scalar-or-array exponent. Differentiates with
    respect to the base, so an integer exponent stays real for a negative base."""
    p = np.asarray(p, dtype=float)
    v = x.value
    with np.errstate(**_ERR):
        is_int = p == np.floor(p)
        base = np.where((v < 0.0) & ~is_int, np.nan, v)  # negative base, non-integer exp -> nan
        val = np.power(base, p)
        coeff = p * np.power(base, p - 1.0)
        zero = p == 0.0
        val = np.where(zero, 1.0, val)                   # x ** 0 == 1, derivative 0
        coeff = np.where(zero, 0.0, coeff)
        # Finding B: for 0 < p < 1 at v == 0 the true slope is +inf; return 0 by
        # the non-poisoning convention sqrt() uses, so x ** 0.5 and sqrt(x) agree.
        root_zero = (v == 0.0) & (p > 0.0) & (p < 1.0)
        coeff = np.where(root_zero, 0.0, coeff)
        # std::pow(+-0, non-integer y > 0) == +0; numpy can return -0 for a -0.0
        # base, so force the +0 sign on the zero value and (finite) derivative.
        frac_pos_zero = (v == 0.0) & (p > 0.0) & ~is_int
        val = np.where(frac_pos_zero, np.abs(val), val)
        coeff = np.where(frac_pos_zero & (p > 1.0) & np.isfinite(coeff), np.abs(coeff), coeff)
    parts = np.broadcast_to(x.partials, val.shape + (x.n,))
    return VJet(val, _scale(coeff, parts))


def _pow_sj(s, y: VJet) -> VJet:
    """``s ** y`` with a constant scalar-or-array base. For a non-positive base the
    value is kept where defined, but the gradient is NaN (matches C++)."""
    s = np.asarray(s, dtype=float)
    with np.errstate(**_ERR):
        val = np.power(s, y.value)          # std::pow value, incl. defined non-positive cases
        is_int_exp = y.value == np.floor(y.value)
        val = np.where((s == 0.0) & (y.value > 0.0) & ~is_int_exp, np.abs(val), val)  # +0 sign
        pos = s > 0.0
        log_s = np.where(pos, np.log(np.where(pos, s, 1.0)), np.nan)
        coeff = val * log_s                 # NaN where base <= 0
    parts = np.broadcast_to(y.partials, val.shape + (y.n,))
    return VJet(val, _scale(coeff, parts))


def _pow_jj(x: VJet, y: VJet) -> VJet:
    if x.n != y.n:
        raise ValueError(f"partial-count mismatch: {x.n} vs {y.n}")
    with np.errstate(**_ERR):
        val = np.power(x.value, y.value)              # std::pow value (incl. negative**integer)
        is_int_exp = y.value == np.floor(y.value)
        val = np.where((x.value == 0.0) & (y.value > 0.0) & ~is_int_exp, np.abs(val), val)  # +0
        base = np.where(x.value > 0.0, x.value, np.nan)  # gradient only where base > 0
        dx = y.value * np.power(base, y.value - 1.0)
        dy = val * np.log(base)
    return VJet(val, _scale(dx, x.partials) + _scale(dy, y.partials))


# ---- elementary functions (dispatch over VJet / array) ---------------------
def sin(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        return VJet(np.sin(x.value), _scale(np.cos(x.value), x.partials))
    return np.sin(x)


def cos(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        return VJet(np.cos(x.value), _scale(-np.sin(x.value), x.partials))
    return np.cos(x)


def tan(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        with np.errstate(**_ERR):
            c = np.cos(x.value)
            deriv = 1.0 / (c * c)
        return VJet(np.tan(x.value), _scale(deriv, x.partials))
    return np.tan(x)


def exp(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        e = np.exp(x.value)
        return VJet(e, _scale(e, x.partials))
    return np.exp(x)


def log(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        v = x.value
        with np.errstate(**_ERR):
            val = np.log(v)            # v < 0 -> nan, v == 0 -> -inf
            deriv = 1.0 / v
        neg = v < 0
        val = np.where(neg, np.nan, val)
        deriv = np.where(neg, np.nan, deriv)
        return VJet(val, _scale(deriv, x.partials))
    return np.log(x)


def sqrt(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        v = x.value
        with np.errstate(**_ERR):
            sv = np.sqrt(np.where(v < 0, np.nan, v))   # negatives -> nan
            # 0.5/sv, but 0 at sv == 0 by convention (matches C++); NaN out of domain.
            deriv = np.where(sv > 0.0, 0.5 / sv, 0.0)
            deriv = np.where(np.isnan(sv), np.nan, deriv)
        return VJet(sv, _scale(deriv, x.partials))
    return np.sqrt(x)


def asin(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        v = x.value
        with np.errstate(**_ERR):
            val = np.arcsin(v)                 # |v| > 1 -> nan
            deriv = 1.0 / np.sqrt((1.0 - v) * (1.0 + v)) # |v| > 1 -> nan, |v| == 1 -> inf
        return VJet(val, _scale(deriv, x.partials))
    return np.arcsin(x)


def acos(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        v = x.value
        with np.errstate(**_ERR):
            val = np.arccos(v)
            deriv = -1.0 / np.sqrt((1.0 - v) * (1.0 + v))
        return VJet(val, _scale(deriv, x.partials))
    return np.arccos(x)


def atan(x: ArrayLike) -> ArrayLike:
    if isinstance(x, VJet):
        v = x.value
        with np.errstate(**_ERR):
            iv = 1.0 / v
            deriv = np.where(np.abs(v) > 1.0, (iv * iv) / (iv * iv + 1.0), 1.0 / (1.0 + v * v))
        return VJet(np.arctan(v), _scale(deriv, x.partials))
    return np.arctan(x)


def hypot(a: ArrayLike, b: ArrayLike) -> ArrayLike:
    if isinstance(a, VJet) or isinstance(b, VJet):
        av, ap, bv, bp = _parts(a, b)
        with np.errstate(**_ERR):
            h = np.hypot(av, bv)
            # Coefficients through a scaled norm so they stay finite when h
            # overflows for a large finite pair (matches C++).
            scale = np.maximum(np.abs(av), np.abs(bv))
            safe = scale != 0
            xs = np.where(safe, av / scale, 0.0)
            ys = np.where(safe, bv / scale, 0.0)
            hs = np.sqrt(xs * xs + ys * ys)
            da = np.where(safe, xs / hs, 0.0)
            db = np.where(safe, ys / hs, 0.0)
        bad = np.isnan(av) | np.isnan(bv)
        h = np.where(bad, np.nan, h)
        da = np.where(bad, np.nan, da)
        db = np.where(bad, np.nan, db)
        return VJet(h, _scale(da, ap) + _scale(db, bp))
    return np.hypot(a, b)


def atan2(a: ArrayLike, b: ArrayLike) -> ArrayLike:
    """``atan2(y, x)`` for VJets, scaled denominator so ``x*x + y*y`` cannot overflow."""
    if isinstance(a, VJet) or isinstance(b, VJet):
        yv, yd, xv, xd = _parts(a, b)
        with np.errstate(**_ERR):
            s = np.maximum(np.abs(xv), np.abs(yv))
            safe = s != 0
            xs = np.where(safe, xv / s, 0.0)
            ys = np.where(safe, yv / s, 0.0)
            den = xs * xs + ys * ys
            dy = np.where(safe, (xs / den) / s, 0.0)
            dx = np.where(safe, -(ys / den) / s, 0.0)
            val = np.arctan2(yv, xv)
        bad = np.isnan(yv) | np.isnan(xv)
        val = np.where(bad, np.nan, val)
        dy = np.where(bad, np.nan, dy)
        dx = np.where(bad, np.nan, dx)
        return VJet(val, _scale(dy, yd) + _scale(dx, xd))
    return np.arctan2(a, b)
