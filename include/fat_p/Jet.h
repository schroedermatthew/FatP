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
     * @return A Jet with value `value` and mPartials[k] == 1.
     */
    [[nodiscard]] static constexpr Jet seed(double value, std::size_t k) noexcept
    {
        Jet result(value);
        result.mPartials[k] = 1.0;
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
    const double inv = 1.0 / y.mValue;
    return detail::lift(x.mValue * inv, inv, -x.mValue * inv * inv, x, y);
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
    const double inv = 1.0 / s;
    return detail::lift(x.mValue * inv, inv, x);
}
/// Scalar divided by a Jet.
template <std::size_t N>
[[nodiscard]] constexpr Jet<N> operator/(double s, const Jet<N>& x) noexcept
{
    const double inv = 1.0 / x.mValue;
    return detail::lift(s * inv, -s * inv * inv, x);
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
/// Arcsine of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> asin(const Jet<N>& x)
{
    return detail::lift(std::asin(x.mValue), 1.0 / std::sqrt(1.0 - x.mValue * x.mValue), x);
}
/// Arccosine of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> acos(const Jet<N>& x)
{
    return detail::lift(std::acos(x.mValue), -1.0 / std::sqrt(1.0 - x.mValue * x.mValue), x);
}
/// Arctangent of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> atan(const Jet<N>& x)
{
    return detail::lift(std::atan(x.mValue), 1.0 / (1.0 + x.mValue * x.mValue), x);
}
/// Exponential of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> exp(const Jet<N>& x)
{
    const double e = std::exp(x.mValue);
    return detail::lift(e, e, x);
}
/// Natural logarithm of a Jet.
template <std::size_t N>
[[nodiscard]] Jet<N> log(const Jet<N>& x)
{
    return detail::lift(std::log(x.mValue), 1.0 / x.mValue, x);
}
/// Square root of a Jet. The derivative is unbounded as the value approaches 0.
template <std::size_t N>
[[nodiscard]] Jet<N> sqrt(const Jet<N>& x)
{
    const double s = std::sqrt(x.mValue);
    return detail::lift(s, (s > 0.0) ? 0.5 / s : 0.0, x);
}
/// Absolute value of a Jet. The derivative is taken as 0 at the value 0.
template <std::size_t N>
[[nodiscard]] Jet<N> abs(const Jet<N>& x)
{
    const double sign = static_cast<double>((x.mValue > 0.0) - (x.mValue < 0.0));
    return detail::lift(std::fabs(x.mValue), sign, x);
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
    return detail::lift(std::pow(x.mValue, p), p * std::pow(x.mValue, p - 1.0), x);
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
    return detail::lift(ab, y.mValue * std::pow(x.mValue, y.mValue - 1.0),
                        ab * std::log(x.mValue), x, y);
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
    const double hx = (h > 0.0) ? x.mValue / h : 0.0;
    const double hy = (h > 0.0) ? y.mValue / h : 0.0;
    return detail::lift(h, hx, hy, x, y);
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
    const double denom = x.mValue * x.mValue + y.mValue * y.mValue;
    const double dy = (denom > 0.0) ? x.mValue / denom : 0.0;
    const double dx = (denom > 0.0) ? -y.mValue / denom : 0.0;
    return detail::lift(std::atan2(y.mValue, x.mValue), dy, dx, y, x);
}

} // namespace fat_p::autodiff
