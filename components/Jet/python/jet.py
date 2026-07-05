"""Fixed-size forward-mode automatic differentiation scalar.

A :class:`Jet` carries a scalar value together with its partial derivatives
with respect to ``N`` independent variables, propagated through arithmetic and
the elementary functions by the chain rule. A function written generically over
its scalar type, evaluated on seeded Jets, returns both its value and its
gradient (or, stacked, a Jacobian) at the seed point.

This is a Python port of the Fat-P C++ ``Jet<N>``; it uses the same derivative
formulas and the same overflow/domain guards. It depends only on the standard
library (``math``).

Differences from the C++ version are noted where they matter: ``N`` is a
run-time property rather than a template parameter, and out-of-domain ``sqrt``/
``log``/``asin``/``acos`` return real NaN where the bare ``math`` functions would
raise ``ValueError``. For a non-positive base with a Jet exponent the real value
is preserved where the host ``pow`` defines it (``**`` would otherwise go
complex), but the partials are NaN because the log-based exponent derivative
needs a positive base -- so a domain excursion stays on the real line and is
visible.

Seeding is 0-based, matching the C++ version (MATLAB's port is 1-based).

Example
-------
>>> from jet import Jet, sin, exp
>>> x1, x2 = Jet.variables([0.7, 1.2])
>>> y = sin(x1) * x2 + exp(x2)
>>> round(y.value, 6)
4.093178
>>> [round(g, 6) for g in y.partials]   # [df/dx1, df/dx2]
[0.917811, 3.964335]
"""

# FATP_META:
#   meta_version: 1
#   component: Jet
#   file_role: port_module
#   path: components/Jet/python/jet.py
#   language: python
#   layer: Foundation
#   summary: Forward-mode AD scalar; Python port of the C++ Jet<N>.
#   api_stability: in_work
#   related:
#     cpp_source: include/fat_p/Jet.h
#     docs_search: "Jet"
#     tests_search: "test_jet"
#   hygiene:
#     stdlib_only: true

from __future__ import annotations

import math
from typing import List, Sequence, Tuple, Union

__all__ = [
    "Jet",
    "sin", "cos", "tan", "asin", "acos", "atan",
    "exp", "log", "sqrt", "hypot", "atan2",
]

Number = Union[int, float]
Scalar = Union["Jet", Number]


class Jet:
    """A value travelling with its ``N`` partial derivatives."""

    __slots__ = ("value", "partials")

    def __init__(self, value: Number, partials: Sequence[Number]) -> None:
        self.value: float = float(value)
        self.partials: Tuple[float, ...] = tuple(float(p) for p in partials)

    # -- construction --------------------------------------------------------
    @classmethod
    def constant(cls, value: Number, n: int) -> "Jet":
        """A constant: the value with ``n`` zero partials."""
        return cls(value, (0.0,) * n)

    @classmethod
    def seed(cls, value: Number, k: int, n: int) -> "Jet":
        """A variable: the value with a unit derivative in direction ``k`` (0-based)."""
        if not 0 <= k < n:
            raise ValueError(f"direction k={k} out of range 0..{n - 1}")
        parts = [0.0] * n
        parts[k] = 1.0
        return cls(value, parts)

    @classmethod
    def variables(cls, values: Sequence[Number]) -> Tuple["Jet", ...]:
        """A tuple of seeded Jets, one per value, each in its own direction."""
        vals = list(values)
        n = len(vals)
        return tuple(cls.seed(v, i, n) for i, v in enumerate(vals))

    # -- readout -------------------------------------------------------------
    @property
    def gradient(self) -> Tuple[float, ...]:
        """The partial derivatives (an alias for :attr:`partials`)."""
        return self.partials

    @staticmethod
    def jacobian(outputs: Sequence["Jet"]) -> Tuple[List[float], List[List[float]]]:
        """Values and Jacobian of a sequence of output Jets.

        Returns ``(values, J)`` where ``values`` is length ``M`` and ``J`` is an
        ``M``-by-``N`` list of lists, row ``i`` being the gradient of output ``i``.
        """
        outs = list(outputs)
        if not outs:
            return [], []
        n = len(outs[0].partials)
        values: List[float] = []
        jac: List[List[float]] = []
        for o in outs:
            if len(o.partials) != n:
                raise ValueError("inconsistent partial counts across outputs")
            values.append(o.value)
            jac.append(list(o.partials))
        return values, jac

    # -- arithmetic ----------------------------------------------------------
    def __add__(self, other: Scalar) -> "Jet":
        av, ad, bv, bd, _ = _binary_parts(self, other)
        return Jet(av + bv, tuple(x + y for x, y in zip(ad, bd)))

    __radd__ = __add__  # addition commutes

    def __sub__(self, other: Scalar) -> "Jet":
        av, ad, bv, bd, _ = _binary_parts(self, other)
        return Jet(av - bv, tuple(x - y for x, y in zip(ad, bd)))

    def __rsub__(self, other: Scalar) -> "Jet":
        av, ad, bv, bd, _ = _binary_parts(other, self)
        return Jet(av - bv, tuple(x - y for x, y in zip(ad, bd)))

    def __mul__(self, other: Scalar) -> "Jet":
        if not isinstance(other, Jet):  # Jet * scalar: dedicated overload, mirrors C++ Jet*double
            s = float(other)
            return Jet(self.value * s, tuple(g * s for g in self.partials))
        av, ad, bv, bd, _ = _binary_parts(self, other)
        return Jet(av * bv, tuple(av * y + bv * x for x, y in zip(ad, bd)))

    __rmul__ = __mul__  # multiplication commutes

    def __truediv__(self, other: Scalar) -> "Jet":
        if not isinstance(other, Jet):  # Jet / scalar: dedicated overload, mirrors C++ Jet/double
            s = float(other)
            return Jet(_div(self.value, s), tuple(_div(g, s) for g in self.partials))
        av, ad, bv, bd, _ = _binary_parts(self, other)
        value = _div(av, bv)
        return Jet(value, tuple(
            # structural-zero guard (Class X): when a denominator partial y is 0,
            # the cross-term value*y is structurally zero but becomes inf*0 = NaN
            # once value overflows; x/bv bypasses the overflowed value.
            _div(x, bv) if y == 0.0 else _div(x - value * y, bv)
            for x, y in zip(ad, bd)))

    def __rtruediv__(self, other: Scalar) -> "Jet":
        av, ad, bv, bd, _ = _binary_parts(other, self)
        value = _div(av, bv)
        return Jet(value, tuple(
            # structural-zero guard (Class X): when a denominator partial y is 0,
            # the cross-term value*y is structurally zero but becomes inf*0 = NaN
            # once value overflows; x/bv bypasses the overflowed value.
            _div(x, bv) if y == 0.0 else _div(x - value * y, bv)
            for x, y in zip(ad, bd)))

    def __neg__(self) -> "Jet":
        return Jet(-self.value, tuple(-g for g in self.partials))

    def __pos__(self) -> "Jet":
        return self

    def __abs__(self) -> "Jet":
        if math.isnan(self.value):
            return _nan(len(self.partials))
        s = 0.0 if self.value == 0.0 else math.copysign(1.0, self.value)
        return Jet(abs(self.value), tuple(s * g for g in self.partials))

    def __pow__(self, other: Scalar) -> "Jet":
        if isinstance(other, Jet):
            return _pow_jj(self, other)
        return _pow_js(self, float(other))

    def __rpow__(self, base: Number) -> "Jet":
        return _pow_sj(float(base), self)

    # -- comparisons (value only, matching the C++ ordering) -----------------
    def __lt__(self, other: Scalar) -> bool:
        return self.value < _value_of(other)

    def __le__(self, other: Scalar) -> bool:
        return self.value <= _value_of(other)

    def __gt__(self, other: Scalar) -> bool:
        return self.value > _value_of(other)

    def __ge__(self, other: Scalar) -> bool:
        return self.value >= _value_of(other)

    def __eq__(self, other: object) -> bool:
        if isinstance(other, (Jet, int, float)):
            return self.value == _value_of(other)  # type: ignore[arg-type]
        return NotImplemented

    def __ne__(self, other: object) -> bool:
        if isinstance(other, (Jet, int, float)):
            return self.value != _value_of(other)  # type: ignore[arg-type]
        return NotImplemented

    __hash__ = None  # value-comparison object: not hashable

    # -- display -------------------------------------------------------------
    def __repr__(self) -> str:
        return f"Jet(value={self.value!r}, partials={self.partials!r})"


# ---- internal helpers ------------------------------------------------------
def _value_of(x: Scalar) -> float:
    return x.value if isinstance(x, Jet) else float(x)


def _binary_parts(a: Scalar, b: Scalar):
    """Promote a scalar operand to a constant Jet; return value/partial pairs and N."""
    if isinstance(a, Jet) and isinstance(b, Jet):
        n = len(a.partials)
        if len(b.partials) != n:
            raise ValueError(f"partial-count mismatch: {n} vs {len(b.partials)}")
        return a.value, a.partials, b.value, b.partials, n
    if isinstance(a, Jet):
        n = len(a.partials)
        return a.value, a.partials, float(b), (0.0,) * n, n
    n = len(b.partials)  # type: ignore[union-attr]
    return float(a), (0.0,) * n, b.value, b.partials, n  # type: ignore[union-attr]


def _nan(n: int) -> Jet:
    return Jet(math.nan, (math.nan,) * n)


def _div(a: float, b: float) -> float:
    """Real division returning IEEE infinities/NaN on a zero denominator
    (matching C++/IEEE) rather than raising ``ZeroDivisionError``."""
    try:
        return a / b
    except ZeroDivisionError:
        if a == 0.0 or math.isnan(a):
            return math.nan                                  # 0/0 -> NaN
        return math.copysign(math.inf, a) * math.copysign(1.0, b)


def _pow_real(base: float, exponent: float) -> float:
    """Real power returning an IEEE infinity on overflow, matching C++/IEEE
    ``std::pow`` -- including the sign for a negative base and odd integer power."""
    try:
        return math.pow(base, exponent)
    except OverflowError:
        if base < 0.0 and float(exponent).is_integer() and int(exponent) % 2 != 0:
            return -math.inf
        return math.inf


def _stdpow_value(base: float, exponent: float) -> float:
    """Value of ``std::pow(base, exponent)`` including non-positive-base cases
    where the real result is defined: negative base with integer exponent, and
    ``0 ** k`` (``0`` for ``k>0``, ``1`` for ``k==0``, ``inf`` for ``k<0``, with the
    sign of ``-0.0`` carried for odd integer ``k``). Negative base with a
    non-integer exponent is NaN."""
    try:
        return _pow_real(base, exponent)
    except ValueError:                       # math.pow domain error
        if base == 0.0 and exponent < 0.0:   # pole at zero; sign follows std::pow
            if (math.copysign(1.0, base) < 0.0 and float(exponent).is_integer()
                    and int(exponent) % 2 != 0):
                return -math.inf             # (-0.0) ** negative odd integer
            return math.inf
        return math.nan                      # negative base, non-integer exponent


def _pow_js(x: Jet, p: float) -> Jet:
    """``x ** p`` with a constant exponent. Value and base-derivative follow
    ``std::pow`` (signed zero included); a negative base with a non-integer
    exponent is off the real line (NaN)."""
    n = len(x.partials)
    if p == 0.0:
        return Jet(1.0, (0.0,) * n)  # x ** 0 is the constant 1, zero partials
    v = x.value
    if v < 0.0 and not float(p).is_integer():
        return _nan(n)               # negative base, non-integer power -> off the real line
    value = _stdpow_value(v, p)
    # Finding B: for the root-like family 0 < p < 1 at v == 0 the true slope is
    # +inf; return 0 by the same non-poisoning convention sqrt() uses, so
    # x ** 0.5 and sqrt(x) agree everywhere.
    coeff = 0.0 if (v == 0.0 and 0.0 < p < 1.0) else p * _stdpow_value(v, p - 1.0)
    return Jet(value, tuple(coeff * g for g in x.partials))


def _pow_sj(s: float, y: Jet) -> Jet:
    """``s ** y`` with a constant base. For a non-positive base the value is kept
    where the real power is defined, but the gradient is NaN -- the log-based
    derivative is not real. Mirrors C++ ``pow(double, Jet)``."""
    n = len(y.partials)
    if not s > 0.0:
        return Jet(_stdpow_value(s, y.value), (math.nan,) * n)
    value = _pow_real(s, y.value)
    coeff = value * math.log(s)
    return Jet(value, tuple(coeff * g for g in y.partials))


def _pow_jj(x: Jet, y: Jet) -> Jet:
    """``x ** y`` with both operands Jets. For a non-positive base the value is
    kept where the real power is defined, but the gradient is NaN under the
    positive-base derivative contract. Mirrors C++ ``pow(Jet, Jet)``."""
    n = len(x.partials)
    if len(y.partials) != n:
        raise ValueError(f"partial-count mismatch: {n} vs {len(y.partials)}")
    if not x.value > 0.0:
        return Jet(_stdpow_value(x.value, y.value), (math.nan,) * n)
    value = _pow_real(x.value, y.value)
    dx = y.value * _pow_real(x.value, y.value - 1.0)  # d/dx
    dy = value * math.log(x.value)                   # d/dy
    return Jet(value, tuple(dx * gx + dy * gy for gx, gy in zip(x.partials, y.partials)))


# ---- elementary functions (dispatch over Jet / float) ----------------------
def sin(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        return Jet(math.sin(x.value), tuple(math.cos(x.value) * g for g in x.partials))
    return math.sin(x)


def cos(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        return Jet(math.cos(x.value), tuple(-math.sin(x.value) * g for g in x.partials))
    return math.cos(x)


def tan(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        c = math.cos(x.value)
        return Jet(math.tan(x.value), tuple(g / (c * c) for g in x.partials))
    return math.tan(x)


def exp(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        try:
            e = math.exp(x.value)
        except OverflowError:
            e = math.inf  # match C++/IEEE: std::exp overflow -> +inf, not an exception
        return Jet(e, tuple(e * g for g in x.partials))
    try:
        return math.exp(x)
    except OverflowError:
        return math.inf


def log(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        v = x.value
        if v < 0.0:
            return _nan(len(x.partials))
        if v == 0.0:
            # 1/v is +inf for +0.0 and -inf for -0.0; matches C++ signed-zero log.
            d = math.copysign(math.inf, v)
            return Jet(-math.inf, tuple(g * d for g in x.partials))
        return Jet(math.log(v), tuple(g / v for g in x.partials))
    return math.log(x)


def sqrt(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        v = x.value
        if v < 0.0:
            return _nan(len(x.partials))
        sv = math.sqrt(v)
        if sv == 0.0:  # derivative 0 at the boundary, by convention (matches C++)
            return Jet(0.0, (0.0,) * len(x.partials))
        return Jet(sv, tuple((0.5 / sv) * g for g in x.partials))
    return math.sqrt(x)


def asin(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        v = x.value
        if abs(v) > 1.0:
            return _nan(len(x.partials))
        r = math.sqrt((1.0 - v) * (1.0 + v))
        if r == 0.0:  # |v| == 1
            return Jet(math.asin(v), tuple((math.copysign(math.inf, g) if g != 0.0 else math.nan)
                                           for g in x.partials))
        return Jet(math.asin(v), tuple(g / r for g in x.partials))
    return math.asin(x)


def acos(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        v = x.value
        if abs(v) > 1.0:
            return _nan(len(x.partials))
        r = math.sqrt((1.0 - v) * (1.0 + v))
        if r == 0.0:  # |v| == 1
            return Jet(math.acos(v), tuple((math.copysign(math.inf, -g) if g != 0.0 else math.nan)
                                           for g in x.partials))
        return Jet(math.acos(v), tuple(-g / r for g in x.partials))
    return math.acos(x)


def atan(x: Scalar) -> Scalar:
    if isinstance(x, Jet):
        v = x.value
        if abs(v) > 1.0:  # large-argument guard: 1/v keeps v*v from overflowing
            iv = 1.0 / v
            deriv = (iv * iv) / (iv * iv + 1.0)
        else:
            deriv = 1.0 / (1.0 + v * v)
        return Jet(math.atan(v), tuple(deriv * g for g in x.partials))
    return math.atan(x)


def hypot(a: Scalar, b: Scalar) -> Scalar:
    if isinstance(a, Jet) or isinstance(b, Jet):
        av, ad, bv, bd, n = _binary_parts(a, b)
        if math.isnan(av) or math.isnan(bv):
            return _nan(n)
        h = math.hypot(av, bv)
        # Coefficients av/h and bv/h, computed through a scaled norm so they stay
        # finite when h overflows for a large finite pair (matches C++).
        scale = max(abs(av), abs(bv))
        if scale == 0.0:  # origin: gradient undefined, taken as 0
            return Jet(0.0, (0.0,) * n)
        xs, ys = av / scale, bv / scale
        hs = math.sqrt(xs * xs + ys * ys)
        hx, hy = xs / hs, ys / hs
        return Jet(h, tuple(hx * x + hy * y for x, y in zip(ad, bd)))
    return math.hypot(a, b)


def atan2(a: Scalar, b: Scalar) -> Scalar:
    """``atan2(y, x)`` for Jets, with a scaled denominator so ``x*x + y*y`` cannot overflow."""
    if isinstance(a, Jet) or isinstance(b, Jet):
        yv, yd, xv, xd, n = _binary_parts(a, b)
        if math.isnan(yv) or math.isnan(xv):
            return _nan(n)
        s = max(abs(xv), abs(yv))
        value = math.atan2(yv, xv)
        if s == 0.0:
            return Jet(value, (0.0,) * n)
        xs, ys = xv / s, yv / s
        den = xs * xs + ys * ys  # in [1, 2]
        dy = (xs / den) / s      # d/dy
        dx = -(ys / den) / s     # d/dx
        return Jet(value, tuple(dy * gy + dx * gx for gy, gx in zip(yd, xd)))
    return math.atan2(a, b)
