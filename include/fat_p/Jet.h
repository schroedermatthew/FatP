#pragma once

/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: public_header
  path: include/fat_p/Jet.h
  namespace: fat_p::autodiff
  layer: Foundation
  summary: Fixed-size forward-mode automatic differentiation scalar.
  api_stability: in_work
  related:
    docs_search: "Jet"
    tests_search: "test_Jet"
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file Jet.h
 * @brief Fixed-size forward-mode automatic differentiation scalar.
 *
 * A Jet carries a value together with its partial derivatives with respect to
 * N independent variables, propagated through arithmetic and the elementary
 * functions by the chain rule. A function written generically over its scalar
 * type, evaluated on seeded Jets, returns both its result and the corresponding
 * gradient (or, for vector outputs, the full Jacobian) at the seed point.
 *
 * @see test_Jet.cpp for the finite-difference validation of every derivative.
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <compare>
#include <cstddef>

namespace fat_p::autodiff
{

/**
 * @brief Forward-mode AD scalar: a value plus N partial derivatives.
 *
 * The value lives in mValue and the partials in mPartials. Seeding the k-th
 * independent variable with seed() sets a unit derivative in direction k;
 * propagating the Jet through a computation accumulates the gradient by the
 * chain rule.
 *
 * @tparam N Number of independent variables (partial-derivative directions).
 *
 * @note Complexity: each elementary operation is O(N) in the partial count.
 * @note Thread-safety: a Jet is a value type with no shared state; concurrent
 *       use mirrors double (independent instances are independent).
 * @note Arithmetic, comparison, and seed() are constexpr. The elementary
 *       functions are not: the underlying <cmath> functions are not constexpr
 *       in C++20.
 */
template <std::size_t N>
    requires(N >= 1)
struct Jet
{
    double mValue{0.0};
    std::array<double, N> mPartials{};

    /// Constructs a zero Jet (value 0, all partials 0).
    constexpr Jet() = default;

    /// Constructs a constant Jet (value `value`, all partials 0).
    constexpr explicit Jet(double value) noexcept : mValue(value) {}

    /**
     * @brief Creates an independent variable seeded in direction k.
     * @param value The value of the variable.
     * @param k     The partial-derivative direction set to unit (0-based).
     *              Precondition: k < N. A constant expression with k >= N is
     *              ill-formed; at runtime k >= N is a debug-asserted precondition
     *              (and otherwise out-of-bounds, as with std::array::operator[]).
     * @return A Jet with value `value` and mPartials[k] == 1.
     */
    [[nodiscard]] static constexpr Jet seed(double value, std::size_t k) noexcept
    {
        assert(k < N && "Jet::seed: direction index out of range");
        Jet result(value);
        result.mPartials[k] = 1.0;
        return result;
    }

    /**
     * @brief Creates an independent variable seeded in a compile-time direction K.
     * @tparam K The partial-derivative direction set to unit (0-based). K < N is
     *           enforced at compile time, so an out-of-range direction is ill-formed.
     * @param value The value of the variable.
     * @return A Jet with value `value` and mPartials[K] == 1.
     */
    template <std::size_t K>
        requires(K < N)
    [[nodiscard]] static constexpr Jet seed(double value) noexcept
    {
        Jet result(value);
        result.mPartials[K] = 1.0;
        return result;
    }

    /// Three-way comparison on the value only; partials carry no order. double
    /// admits NaN, so the relation is partial. <=> and == synthesize the other
    /// relational operators, including the reversed scalar-on-left forms.
    [[nodiscard]] friend constexpr std::partial_ordering operator<=>(const Jet& x,
                                                                     const Jet& y) noexcept
    {
        return x.mValue <=> y.mValue;
    }
    /// Equality on the value only.
    [[nodiscard]] friend constexpr bool operator==(const Jet& x, const Jet& y) noexcept
    {
        return x.mValue == y.mValue;
    }
    /// Three-way comparison against a scalar (value only).
    [[nodiscard]] friend constexpr std::partial_ordering operator<=>(const Jet& x,
                                                                     double s) noexcept
    {
        return x.mValue <=> s;
    }
    /// Equality against a scalar (value only).
    [[nodiscard]] friend constexpr bool operator==(const Jet& x, double s) noexcept
    {
        return x.mValue == s;
    }

    /// Compound assignment, expressed through the binary operators so the
    /// derivative rules live in one place; constexpr like that arithmetic.
    constexpr Jet& operator+=(const Jet& rhs) noexcept { return *this = *this + rhs; }
    constexpr Jet& operator-=(const Jet& rhs) noexcept { return *this = *this - rhs; }
    constexpr Jet& operator*=(const Jet& rhs) noexcept { return *this = *this * rhs; }
    constexpr Jet& operator/=(const Jet& rhs) noexcept { return *this = *this / rhs; }
    constexpr Jet& operator+=(double rhs) noexcept { return *this = *this + rhs; }
    constexpr Jet& operator-=(double rhs) noexcept { return *this = *this - rhs; }
    constexpr Jet& operator*=(double rhs) noexcept { return *this = *this * rhs; }
    constexpr Jet& operator/=(double rhs) noexcept { return *this = *this / rhs; }
};

namespace detail
{

// y = f(x): value f(a), partials f'(a) * partials.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> lift(double value, double deriv, const Jet<N>& x) noexcept
{
    Jet<N> r;
    r.mValue = value;
    std::ranges::transform(x.mPartials, r.mPartials.begin(),
                           [deriv](double dx) { return deriv * dx; });
    return r;
}

// z = g(x, y): value g, partials gx * dx + gy * dy.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> lift(double value, double gx, double gy, const Jet<N>& x,
                                    const Jet<N>& y) noexcept
{
    Jet<N> r;
    r.mValue = value;
    std::ranges::transform(x.mPartials, y.mPartials, r.mPartials.begin(),
                           [gx, gy](double dx, double dy) { return gx * dx + gy * dy; });
    return r;
}

} // namespace detail

// ===== arithmetic ==========================================================

/// Sum of two Jets.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator+(const Jet<N>& x, const Jet<N>& y) noexcept
{
    return detail::lift(x.mValue + y.mValue, 1.0, 1.0, x, y);
}
/// Difference of two Jets.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator-(const Jet<N>& x, const Jet<N>& y) noexcept
{
    return detail::lift(x.mValue - y.mValue, 1.0, -1.0, x, y);
}
/// Product of two Jets (product rule).
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator*(const Jet<N>& x, const Jet<N>& y) noexcept
{
    return detail::lift(x.mValue * y.mValue, y.mValue, x.mValue, x, y);
}
/// Quotient of two Jets (quotient rule).
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator/(const Jet<N>& x, const Jet<N>& y) noexcept
{
    // Stable quotient: value by direct division, and each partial divided by
    // the denominator once. Avoids forming 1/y or y*y, which overflow or
    // underflow for tiny y even when the true quotient is finite.
    //
    // Structural-zero guard (Class X): when a denominator partial is zero (a
    // constant operand or an inactive direction) the cross-term -value*dy is
    // identically zero, but evaluates to inf*0 = NaN once the value overflows.
    // Skipping it recovers the true partial. This is NOT the coefficient*0
    // suppression rejected in detail::lift: there the zeroed coefficient may be
    // a genuine NaN encoding non-differentiability; here dy == 0 is structural
    // and carries no information. A genuine NaN in dx still propagates.
    Jet<N> r;
    r.mValue = x.mValue / y.mValue;
    for (std::size_t i = 0; i < N; ++i)
        r.mPartials[i] = (y.mPartials[i] == 0.0)
                             ? x.mPartials[i] / y.mValue
                             : (x.mPartials[i] - r.mValue * y.mPartials[i]) / y.mValue;
    return r;
}
/// Negation.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator-(const Jet<N>& x) noexcept
{
    return detail::lift(-x.mValue, -1.0, x);
}
/// Scalar times a Jet.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator*(double s, const Jet<N>& x) noexcept
{
    return detail::lift(s * x.mValue, s, x);
}
/// Jet times a scalar.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator*(const Jet<N>& x, double s) noexcept
{
    return s * x;
}
/// Jet divided by a scalar.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator/(const Jet<N>& x, double s) noexcept
{
    Jet<N> r;
    r.mValue = x.mValue / s;
    for (std::size_t i = 0; i < N; ++i)
        r.mPartials[i] = x.mPartials[i] / s;
    return r;
}
/// Scalar divided by a Jet.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator/(double s, const Jet<N>& x) noexcept
{
    // Multiply-first ordering -(value*dx)/x (matching the language ports) avoids
    // the intermediate value/x overflowing when the value is finite but large
    // (Class Y). The structural-zero guard handles inactive directions under
    // value overflow (Class X), as in operator/(Jet,Jet).
    //
    // Known limitation (Class A2b): when the value itself overflows to inf, an
    // active direction whose true derivative is finite cannot be recovered here,
    // because the formula reuses the overflowed value. -s*dx/x^2 would need an
    // exponent-scaled helper (cf. hypot/atan2). Deferred; documented.
    Jet<N> r;
    r.mValue = s / x.mValue;
    for (std::size_t i = 0; i < N; ++i)
        r.mPartials[i] =
            (x.mPartials[i] == 0.0) ? 0.0 : -(r.mValue * x.mPartials[i]) / x.mValue;
    return r;
}
/// Adds a scalar to the value.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator+(const Jet<N>& x, double s) noexcept
{
    Jet<N> r = x;
    r.mValue += s;
    return r;
}
/// Adds a scalar to the value.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator+(double s, const Jet<N>& x) noexcept
{
    return x + s;
}
/// Subtracts a scalar from the value.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator-(const Jet<N>& x, double s) noexcept
{
    Jet<N> r = x;
    r.mValue -= s;
    return r;
}
/// Scalar minus a Jet.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator-(double s, const Jet<N>& x) noexcept
{
    return detail::lift(s - x.mValue, -1.0, x);
}

// ===== elementary functions ================================================
// Named to match <cmath> so generic code resolves them by ADL. Not constexpr:
// the <cmath> functions are not constexpr in C++20.

/// Sine of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> sin(const Jet<N>& x)
{
    return detail::lift(std::sin(x.mValue), std::cos(x.mValue), x);
}
/// Cosine of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> cos(const Jet<N>& x)
{
    return detail::lift(std::cos(x.mValue), -std::sin(x.mValue), x);
}
/// Tangent of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> tan(const Jet<N>& x)
{
    const double t = std::tan(x.mValue);
    return detail::lift(t, 1.0 + t * t, x);
}
/// Arcsine of a Jet. The derivative radicand is factored as (1-x)(1+x) rather
/// than 1 - x*x, which loses precision to cancellation as |x| -> 1.
template <std::size_t N>
[[nodiscard]] Jet<N> asin(const Jet<N>& x)
{
    return detail::lift(std::asin(x.mValue),
                        1.0 / std::sqrt((1.0 - x.mValue) * (1.0 + x.mValue)), x);
}
/// Arccosine of a Jet. Radicand factored as (1-x)(1+x) for the same reason as asin.
template <std::size_t N>
[[nodiscard]] Jet<N> acos(const Jet<N>& x)
{
    return detail::lift(std::acos(x.mValue),
                        -1.0 / std::sqrt((1.0 - x.mValue) * (1.0 + x.mValue)), x);
}
/// Arctangent of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> atan(const Jet<N>& x)
{
    // For |x| > 1 compute via 1/x so x*x cannot overflow before the derivative
    // (representable as a subnormal for large x) underflows.
    double deriv;
    if (std::fabs(x.mValue) > 1.0)
    {
        const double inv = 1.0 / x.mValue;
        const double inv2 = inv * inv;
        deriv = inv2 / (1.0 + inv2);
    }
    else
    {
        deriv = 1.0 / (1.0 + x.mValue * x.mValue);
    }
    return detail::lift(std::atan(x.mValue), deriv, x);
}
/// Exponential of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> exp(const Jet<N>& x)
{
    const double e = std::exp(x.mValue);
    return detail::lift(e, e, x);
}
/// Natural logarithm of a Jet. A negative argument is out of domain: the value
/// and the partials are NaN. At +0.0 the value is -inf and the derivative +inf;
/// at signed -0.0 the derivative follows the IEEE reciprocal and is -inf.
template <std::size_t N>
[[nodiscard]] Jet<N> log(const Jet<N>& x)
{
    const double v = std::log(x.mValue);
    const double deriv = std::isnan(v) ? v : 1.0 / x.mValue;
    return detail::lift(v, deriv, x);
}
/// Square root of a Jet. The true derivative is unbounded as the value
/// approaches 0; at exactly 0 it is returned as 0 by convention. A negative
/// argument is out of domain: the value and the partials are NaN.
template <std::size_t N>
[[nodiscard]] Jet<N> sqrt(const Jet<N>& x)
{
    const double s = std::sqrt(x.mValue);
    const double deriv = std::isnan(s) ? s : ((s > 0.0) ? 0.5 / s : 0.0);
    return detail::lift(s, deriv, x);
}
/// Absolute value of a Jet. The derivative is taken as 0 at the value 0. A NaN
/// argument propagates to the partials.
template <std::size_t N>
[[nodiscard]] Jet<N> abs(const Jet<N>& x)
{
    const double a = std::fabs(x.mValue);
    const double sign = static_cast<double>((x.mValue > 0.0) - (x.mValue < 0.0));
    const double deriv = std::isnan(a) ? a : sign;
    return detail::lift(a, deriv, x);
}

/**
 * @brief Raises a Jet to a constant power: (x^p)' = p x^(p-1).
 * @param x The base.
 * @param p The constant exponent.
 * @return A Jet holding x^p and its gradient.
 */
template <std::size_t N>
[[nodiscard]] Jet<N> pow(const Jet<N>& x, double p)
{
    if (p == 0.0)
    {
        return Jet<N>{std::pow(x.mValue, p)}; // x^0 is the constant function; zero partials
    }
    // Finding B: for the root-like family 0 < p < 1 at x == 0 the true slope is
    // +inf. Return 0 by the same non-poisoning convention sqrt() uses, so
    // pow(x, 0.5) and sqrt(x) agree everywhere. p == 1 (slope 1) and p > 1
    // (slope 0) are already finite and need no special case.
    double deriv;
    if (x.mValue == 0.0 && p > 0.0 && p < 1.0)
        deriv = 0.0;
    else
        deriv = p * std::pow(x.mValue, p - 1.0);
    return detail::lift(std::pow(x.mValue, p), deriv, x);
}

/**
 * @brief Raises a Jet to a Jet power.
 *
 * d/dx = y x^(y-1), d/dy = x^y ln x. The mixed partial via the exponent
 * requires a positive base.
 *
 * @param x The base (value must be positive).
 * @param y The exponent.
 * @return A Jet holding x^y and its gradient.
 */
template <std::size_t N>
[[nodiscard]] Jet<N> pow(const Jet<N>& x, const Jet<N>& y)
{
    const double ab = std::pow(x.mValue, y.mValue);
    if (!(x.mValue > 0.0)) // positive-base contract; non-positive base -> NaN gradient
    {
        return detail::lift(ab, std::nan(""), std::nan(""), x, y);
    }
    return detail::lift(ab, y.mValue * std::pow(x.mValue, y.mValue - 1.0),
                        ab * std::log(x.mValue), x, y);
}

/**
 * @brief Raises a scalar base to a Jet power.
 *
 * d/d(exp) base^exp = base^exp ln(base), which requires a positive base; for a
 * non-positive base the gradient is NaN (no real derivative exists there).
 *
 * @param base     The constant base (must be positive).
 * @param exponent The exponent.
 * @return A Jet holding base^exponent and its gradient.
 */
template <std::size_t N>
[[nodiscard]] Jet<N> pow(double base, const Jet<N>& exponent)
{
    const double v = std::pow(base, exponent.mValue);
    if (!(base > 0.0)) // includes 0, negative, and NaN base
    {
        return detail::lift(v, std::nan(""), exponent);
    }
    return detail::lift(v, v * std::log(base), exponent);
}

/**
 * @brief Euclidean norm hypot(x, y): d/dx = x/h, d/dy = y/h.
 * @param x First component.
 * @param y Second component.
 * @return A Jet holding sqrt(x^2 + y^2) and its gradient.
 */
template <std::size_t N>
[[nodiscard]] Jet<N> hypot(const Jet<N>& x, const Jet<N>& y)
{
    const double h = std::hypot(x.mValue, y.mValue);
    if (std::isnan(h))
    {
        return detail::lift(h, h, h, x, y); // out of domain: NaN value and partials
    }
    // Coefficients are x/h and y/h, both in [-1, 1]. Compute them through a
    // scaled norm so they survive when h overflows to inf for a large finite
    // pair; scale == 0 only at the origin, where the gradient is undefined (0).
    const double scale = std::max(std::fabs(x.mValue), std::fabs(y.mValue));
    double hx = 0.0;
    double hy = 0.0;
    if (scale > 0.0)
    {
        const double xs = x.mValue / scale;
        const double ys = y.mValue / scale;
        const double hs = std::sqrt(xs * xs + ys * ys);
        hx = xs / hs;
        hy = ys / hs;
    }
    return detail::lift(h, hx, hy, x, y);
}

/// hypot with one scalar argument; delegates so the NaN and origin conventions
/// are shared with the Jet/Jet form.
template <std::size_t N>
[[nodiscard]] Jet<N> hypot(const Jet<N>& x, double y)
{
    return hypot(x, Jet<N>{y});
}
template <std::size_t N>
[[nodiscard]] Jet<N> hypot(double x, const Jet<N>& y)
{
    return hypot(Jet<N>{x}, y);
}

/**
 * @brief Two-argument arctangent atan2(y, x). Argument order matches std.
 *
 * d/dy = x / (x^2 + y^2), d/dx = -y / (x^2 + y^2).
 *
 * @param y Numerator argument.
 * @param x Denominator argument.
 * @return A Jet holding atan2(y, x) and its gradient.
 */
template <std::size_t N>
[[nodiscard]] Jet<N> atan2(const Jet<N>& y, const Jet<N>& x)
{
    const double v = std::atan2(y.mValue, x.mValue);
    if (std::isnan(v))
    {
        return detail::lift(v, v, v, y, x); // out of domain: NaN value and partials
    }
    // Scale by the larger magnitude so the denominator stays O(1); squaring the
    // raw values would overflow or underflow for very large or very small inputs.
    const double scale = std::max(std::fabs(x.mValue), std::fabs(y.mValue));
    if (scale == 0.0)
    {
        return detail::lift(v, 0.0, 0.0, y, x); // origin: gradient undefined, 0 by convention
    }
    const double xs = x.mValue / scale;
    const double ys = y.mValue / scale;
    const double denom = xs * xs + ys * ys;
    const double dy = (xs / denom) / scale;  // d/dy =  x / (x^2 + y^2); avoid forming scale*denom
    const double dx = (-ys / denom) / scale; // d/dx = -y / (x^2 + y^2)
    return detail::lift(v, dy, dx, y, x);
}

/// atan2 with one scalar argument; delegates to the Jet/Jet form (argument
/// order matches std::atan2(y, x)).
template <std::size_t N>
[[nodiscard]] Jet<N> atan2(const Jet<N>& y, double x)
{
    return atan2(y, Jet<N>{x});
}
template <std::size_t N>
[[nodiscard]] Jet<N> atan2(double y, const Jet<N>& x)
{
    return atan2(Jet<N>{y}, x);
}

} // namespace fat_p::autodiff
