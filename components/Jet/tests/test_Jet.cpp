/**
 * @file test_Jet.cpp
 * @brief Exhaustive unit tests for Jet.h (forward-mode AD scalar).
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: test
  path: components/Jet/tests/test_Jet.cpp
  namespace: fat_p::testing::jet
  layer: Testing
  summary: Exhaustive unit tests for the Jet forward-mode AD scalar.
  api_stability: in_work
  related:
    docs_search: "Jet"
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>

#include "Jet.h"

#include "FatPTest.h"

namespace fat_p::testing::jet
{

using fat_p::autodiff::Jet;

// ============================================================================
// Helpers. The FATP_ASSERT_* macros return false from the enclosing function
// on failure, so each helper returns bool and callers use `if (!h(...)) return
// false;`. Analytic checks use the default CLOSE tolerance; finite-difference
// checks use an explicit 1e-6 (central differences carry ~1e-9 error).
// ============================================================================

template <class FJet, class FVal, class FDer>
bool checkUnary(const char* name, FJet fj, FVal fval, FDer fder, double x0)
{
    const Jet<1> r = fj(Jet<1>::seed(x0, 0));
    FATP_ASSERT_CLOSE(r.mValue, fval(x0), name);
    FATP_ASSERT_CLOSE(r.mPartials[0], fder(x0), name);
    return true;
}

template <class FJet, class FVal, class FDir0, class FDir1>
bool checkBinary(const char* name, FJet fj, FVal fval, FDir0 d0, FDir1 d1, double v0, double v1)
{
    const Jet<2> r = fj(Jet<2>::seed(v0, 0), Jet<2>::seed(v1, 1));
    FATP_ASSERT_CLOSE(r.mValue, fval(v0, v1), name);
    FATP_ASSERT_CLOSE(r.mPartials[0], d0(v0, v1), name);
    FATP_ASSERT_CLOSE(r.mPartials[1], d1(v0, v1), name);
    return true;
}

template <class FJet, class Fd>
bool fdSweepUnary(const char* name, FJet fj, Fd fd, double lo, double hi, std::mt19937& rng)
{
    std::uniform_real_distribution<double> dist(lo, hi);
    const double h = 1e-6;
    for (int i = 0; i < 50; ++i)
    {
        const double x0 = dist(rng);
        const double ad = fj(Jet<1>::seed(x0, 0)).mPartials[0];
        const double fdv = (fd(x0 + h) - fd(x0 - h)) / (2.0 * h);
        FATP_ASSERT_CLOSE_EPS(ad, fdv, 1e-6, name);
    }
    return true;
}

template <class FJet, class Fd>
bool fdSweepBinary(const char* name, FJet fj, Fd fd, double xlo, double xhi, double ylo, double yhi,
                   std::mt19937& rng)
{
    std::uniform_real_distribution<double> dx(xlo, xhi);
    std::uniform_real_distribution<double> dy(ylo, yhi);
    const double h = 1e-6;
    for (int i = 0; i < 50; ++i)
    {
        const double x0 = dx(rng);
        const double y0 = dy(rng);
        const Jet<2> r = fj(Jet<2>::seed(x0, 0), Jet<2>::seed(y0, 1));
        const double fdx = (fd(x0 + h, y0) - fd(x0 - h, y0)) / (2.0 * h);
        const double fdy = (fd(x0, y0 + h) - fd(x0, y0 - h)) / (2.0 * h);
        FATP_ASSERT_CLOSE_EPS(r.mPartials[0], fdx, 1e-6, name);
        FATP_ASSERT_CLOSE_EPS(r.mPartials[1], fdy, 1e-6, name);
    }
    return true;
}

// ============================================================================
// Construction, seeding, value semantics
// ============================================================================

FATP_TEST_CASE(default_construction)
{
    Jet<3> z;
    FATP_ASSERT_CLOSE(z.mValue, 0.0, "default value is 0");
    for (double d : z.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "default partials are 0");
    }
    return true;
}

FATP_TEST_CASE(constant_construction)
{
    Jet<3> c{5.0};
    FATP_ASSERT_CLOSE(c.mValue, 5.0, "constant value");
    for (double d : c.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "constant Jet has zero partials");
    }
    return true;
}

FATP_TEST_CASE(seed_basic)
{
    Jet<4> s = Jet<4>::seed(2.5, 2);
    FATP_ASSERT_CLOSE(s.mValue, 2.5, "seed value");
    FATP_ASSERT_CLOSE(s.mPartials[2], 1.0, "seeded direction is unit");
    FATP_ASSERT_CLOSE(s.mPartials[0], 0.0, "unseeded direction 0");
    FATP_ASSERT_CLOSE(s.mPartials[1], 0.0, "unseeded direction 1");
    FATP_ASSERT_CLOSE(s.mPartials[3], 0.0, "unseeded direction 3");
    return true;
}

FATP_TEST_CASE(seed_high_dimension)
{
    // Eight independent variables: each seed touches exactly one direction,
    // and that independence survives a chain of operations.
    Jet<8> acc{0.0};
    for (std::size_t k = 0; k < 8; ++k)
    {
        acc = acc + Jet<8>::seed(static_cast<double>(k + 1), k);
    }
    FATP_ASSERT_CLOSE(acc.mValue, 36.0, "sum 1..8");
    for (std::size_t k = 0; k < 8; ++k)
    {
        FATP_ASSERT_CLOSE(acc.mPartials[k], 1.0, "each direction contributes unit slope");
    }
    return true;
}

FATP_TEST_CASE(value_semantics)
{
    Jet<3> a = Jet<3>::seed(2.0, 1);
    Jet<3> b = a; // copy construct
    FATP_ASSERT_CLOSE(b.mValue, 2.0, "copy preserves value");
    FATP_ASSERT_CLOSE(b.mPartials[1], 1.0, "copy preserves partials");

    Jet<3> c;
    c = a; // copy assign
    FATP_ASSERT_CLOSE(c.mPartials[1], 1.0, "assignment preserves partials");

    Jet<3> d = std::move(a); // move == copy for a trivial type
    FATP_ASSERT_CLOSE(d.mPartials[1], 1.0, "move preserves partials");

    static_assert(std::is_trivially_copyable_v<Jet<3>>, "Jet is a trivially copyable value type");
    static_assert(std::is_standard_layout_v<Jet<3>>, "Jet is standard layout");
    return true;
}

// ============================================================================
// constexpr contract
// ============================================================================

FATP_TEST_CASE(constexpr_arithmetic)
{
    constexpr Jet<2> x = Jet<2>::seed(3.0, 0);
    constexpr Jet<2> y = Jet<2>::seed(4.0, 1);

    constexpr Jet<2> p = x * y;
    static_assert(p.mValue == 12.0, "constexpr product value");
    static_assert(p.mPartials[0] == 4.0, "constexpr d/dx of x*y is y");
    static_assert(p.mPartials[1] == 3.0, "constexpr d/dy of x*y is x");

    constexpr Jet<2> q = x / y;
    static_assert(q.mValue == 0.75, "constexpr quotient value");

    constexpr Jet<2> s = x + 2.0 - y;
    static_assert(s.mValue == 1.0, "constexpr mixed value");
    static_assert(s.mPartials[0] == 1.0, "constexpr mixed d/dx");
    static_assert(s.mPartials[1] == -1.0, "constexpr mixed d/dy");

    FATP_ASSERT_TRUE(true, "constexpr arithmetic evaluated at compile time");
    return true;
}

FATP_TEST_CASE(constexpr_seed_and_compare)
{
    constexpr Jet<1> a = Jet<1>::seed(1.0, 0);
    constexpr Jet<1> b = Jet<1>::seed(2.0, 0);
    static_assert(a.mPartials[0] == 1.0, "constexpr seed sets unit partial");
    static_assert(a < b, "constexpr ordering");
    static_assert(a == 1.0, "constexpr scalar equality");
    FATP_ASSERT_TRUE(true, "constexpr seed and comparison evaluated at compile time");
    return true;
}

// ============================================================================
// Comparison (value-only, NaN-aware)
// ============================================================================

FATP_TEST_CASE(comparison_jet_jet)
{
    Jet<1> one = Jet<1>::seed(1.0, 0);
    Jet<1> two = Jet<1>::seed(2.0, 0);
    Jet<1> one2 = Jet<1>::seed(1.0, 0);

    FATP_ASSERT_TRUE(one < two, "1 < 2");
    FATP_ASSERT_TRUE(two > one, "2 > 1");
    FATP_ASSERT_TRUE(one <= one2, "1 <= 1");
    FATP_ASSERT_TRUE(one >= one2, "1 >= 1");
    FATP_ASSERT_TRUE(one == one2, "1 == 1");
    FATP_ASSERT_TRUE(one != two, "1 != 2");
    FATP_ASSERT_FALSE(one > two, "not 1 > 2");
    FATP_ASSERT_FALSE(one == two, "not 1 == 2");
    return true;
}

FATP_TEST_CASE(comparison_jet_scalar)
{
    Jet<1> one = Jet<1>::seed(1.0, 0);

    FATP_ASSERT_TRUE(one < 2.0, "jet < scalar");
    FATP_ASSERT_TRUE(one <= 1.0, "jet <= scalar");
    FATP_ASSERT_TRUE(one > 0.0, "jet > scalar");
    FATP_ASSERT_TRUE(one >= 1.0, "jet >= scalar");
    FATP_ASSERT_TRUE(one == 1.0, "jet == scalar");
    FATP_ASSERT_TRUE(one != 2.0, "jet != scalar");

    // Reversed (scalar on the left) forms are synthesized from <=> and ==.
    FATP_ASSERT_TRUE(2.0 > one, "scalar > jet");
    FATP_ASSERT_TRUE(0.0 < one, "scalar < jet");
    FATP_ASSERT_TRUE(1.0 == one, "scalar == jet");
    FATP_ASSERT_TRUE(2.0 != one, "scalar != jet");
    return true;
}

FATP_TEST_CASE(comparison_value_only)
{
    // Ordering and equality use the value only; partials are ignored.
    Jet<2> a = Jet<2>::seed(1.0, 0);
    Jet<2> b = Jet<2>::seed(1.0, 1);
    FATP_ASSERT_TRUE(a == b, "equal values compare equal regardless of partials");
    FATP_ASSERT_FALSE(a < b, "equal values are not ordered strictly");
    FATP_ASSERT_NE(a.mPartials[0], b.mPartials[0], "partials genuinely differ");
    return true;
}

FATP_TEST_CASE(comparison_nan_unordered)
{
    Jet<1> nan{std::numeric_limits<double>::quiet_NaN()};
    Jet<1> zero{0.0};
    FATP_ASSERT_TRUE((nan <=> zero) == std::partial_ordering::unordered, "NaN <=> finite unordered");
    FATP_ASSERT_FALSE(nan < zero, "NaN not less");
    FATP_ASSERT_FALSE(nan > zero, "NaN not greater");
    FATP_ASSERT_FALSE(nan == zero, "NaN not equal");
    FATP_ASSERT_FALSE(nan == nan, "NaN not equal to itself");
    return true;
}

// ============================================================================
// Arithmetic (value and every partial)
// ============================================================================

FATP_TEST_CASE(add)
{
    Jet<2> r = Jet<2>::seed(3.0, 0) + Jet<2>::seed(4.0, 1);
    FATP_ASSERT_CLOSE(r.mValue, 7.0, "sum value");
    FATP_ASSERT_CLOSE(r.mPartials[0], 1.0, "d(x+y)/dx");
    FATP_ASSERT_CLOSE(r.mPartials[1], 1.0, "d(x+y)/dy");
    return true;
}

FATP_TEST_CASE(subtract)
{
    Jet<2> r = Jet<2>::seed(3.0, 0) - Jet<2>::seed(4.0, 1);
    FATP_ASSERT_CLOSE(r.mValue, -1.0, "difference value");
    FATP_ASSERT_CLOSE(r.mPartials[0], 1.0, "d(x-y)/dx");
    FATP_ASSERT_CLOSE(r.mPartials[1], -1.0, "d(x-y)/dy");
    return true;
}

FATP_TEST_CASE(multiply)
{
    Jet<2> r = Jet<2>::seed(3.0, 0) * Jet<2>::seed(4.0, 1);
    FATP_ASSERT_CLOSE(r.mValue, 12.0, "product value");
    FATP_ASSERT_CLOSE(r.mPartials[0], 4.0, "d(xy)/dx = y");
    FATP_ASSERT_CLOSE(r.mPartials[1], 3.0, "d(xy)/dy = x");
    return true;
}

FATP_TEST_CASE(divide)
{
    Jet<2> r = Jet<2>::seed(3.0, 0) / Jet<2>::seed(4.0, 1);
    FATP_ASSERT_CLOSE(r.mValue, 0.75, "quotient value");
    FATP_ASSERT_CLOSE(r.mPartials[0], 0.25, "d(x/y)/dx = 1/y");
    FATP_ASSERT_CLOSE(r.mPartials[1], -3.0 / 16.0, "d(x/y)/dy = -x/y^2");
    return true;
}

FATP_TEST_CASE(unary_negate)
{
    Jet<2> r = -Jet<2>::seed(3.0, 0);
    FATP_ASSERT_CLOSE(r.mValue, -3.0, "negation value");
    FATP_ASSERT_CLOSE(r.mPartials[0], -1.0, "d(-x)/dx");
    return true;
}

FATP_TEST_CASE(scalar_arithmetic)
{
    Jet<1> x = Jet<1>::seed(3.0, 0);

    Jet<1> a = 2.0 * x;
    FATP_ASSERT_CLOSE(a.mValue, 6.0, "2*x value");
    FATP_ASSERT_CLOSE(a.mPartials[0], 2.0, "d(2x)/dx");

    Jet<1> b = x * 2.0;
    FATP_ASSERT_CLOSE(b.mPartials[0], 2.0, "d(x*2)/dx");

    Jet<1> c = x + 5.0;
    FATP_ASSERT_CLOSE(c.mValue, 8.0, "x+5 value");
    FATP_ASSERT_CLOSE(c.mPartials[0], 1.0, "d(x+5)/dx unchanged");

    Jet<1> d = 5.0 + x;
    FATP_ASSERT_CLOSE(d.mValue, 8.0, "5+x value");

    Jet<1> e = x - 5.0;
    FATP_ASSERT_CLOSE(e.mValue, -2.0, "x-5 value");
    FATP_ASSERT_CLOSE(e.mPartials[0], 1.0, "d(x-5)/dx");

    Jet<1> f = 5.0 - x;
    FATP_ASSERT_CLOSE(f.mValue, 2.0, "5-x value");
    FATP_ASSERT_CLOSE(f.mPartials[0], -1.0, "d(5-x)/dx");

    Jet<1> g = x / 2.0;
    FATP_ASSERT_CLOSE(g.mValue, 1.5, "x/2 value");
    FATP_ASSERT_CLOSE(g.mPartials[0], 0.5, "d(x/2)/dx");

    Jet<1> hh = 6.0 / x;
    FATP_ASSERT_CLOSE(hh.mValue, 2.0, "6/x value");
    FATP_ASSERT_CLOSE(hh.mPartials[0], -6.0 / 9.0, "d(6/x)/dx = -6/x^2");
    return true;
}

// ============================================================================
// Algebraic identities (value and gradient)
// ============================================================================

FATP_TEST_CASE(additive_identities)
{
    Jet<2> x = Jet<2>::seed(3.0, 0);

    Jet<2> a = x + 0.0;
    FATP_ASSERT_CLOSE(a.mValue, x.mValue, "x+0 value == x");
    FATP_ASSERT_CLOSE(a.mPartials[0], x.mPartials[0], "x+0 grad == x grad");

    Jet<2> b = x - x;
    FATP_ASSERT_CLOSE(b.mValue, 0.0, "x-x value 0");
    FATP_ASSERT_CLOSE(b.mPartials[0], 0.0, "x-x gradient 0");

    Jet<2> c = -(-x);
    FATP_ASSERT_CLOSE(c.mValue, x.mValue, "-(-x) value == x");
    FATP_ASSERT_CLOSE(c.mPartials[0], x.mPartials[0], "-(-x) grad == x grad");
    return true;
}

FATP_TEST_CASE(multiplicative_identities)
{
    Jet<2> x = Jet<2>::seed(3.0, 0);

    Jet<2> a = x * 1.0;
    FATP_ASSERT_CLOSE(a.mValue, x.mValue, "x*1 value == x");
    FATP_ASSERT_CLOSE(a.mPartials[0], x.mPartials[0], "x*1 grad == x grad");

    Jet<2> b = x * 0.0;
    FATP_ASSERT_CLOSE(b.mValue, 0.0, "x*0 value 0");
    FATP_ASSERT_CLOSE(b.mPartials[0], 0.0, "x*0 gradient 0");

    Jet<2> c = x / x;
    FATP_ASSERT_CLOSE(c.mValue, 1.0, "x/x value 1");
    FATP_ASSERT_CLOSE(c.mPartials[0], 0.0, "x/x gradient 0");
    return true;
}

FATP_TEST_CASE(commutativity)
{
    Jet<2> x = Jet<2>::seed(3.0, 0);
    Jet<2> y = Jet<2>::seed(4.0, 1);

    Jet<2> s1 = x + y;
    Jet<2> s2 = y + x;
    FATP_ASSERT_CLOSE(s1.mValue, s2.mValue, "x+y == y+x value");
    FATP_ASSERT_CLOSE(s1.mPartials[0], s2.mPartials[0], "x+y == y+x d/dx");
    FATP_ASSERT_CLOSE(s1.mPartials[1], s2.mPartials[1], "x+y == y+x d/dy");

    Jet<2> p1 = x * y;
    Jet<2> p2 = y * x;
    FATP_ASSERT_CLOSE(p1.mValue, p2.mValue, "x*y == y*x value");
    FATP_ASSERT_CLOSE(p1.mPartials[0], p2.mPartials[0], "x*y == y*x d/dx");
    FATP_ASSERT_CLOSE(p1.mPartials[1], p2.mPartials[1], "x*y == y*x d/dy");
    return true;
}

// ============================================================================
// Elementary functions: one case each (value vs std, derivative vs analytic)
// ============================================================================

FATP_TEST_CASE(fn_sin)
{
    return checkUnary(
        "sin", [](Jet<1> v) { return sin(v); }, [](double v) { return std::sin(v); },
        [](double v) { return std::cos(v); }, 0.6);
}

FATP_TEST_CASE(fn_cos)
{
    return checkUnary(
        "cos", [](Jet<1> v) { return cos(v); }, [](double v) { return std::cos(v); },
        [](double v) { return -std::sin(v); }, 0.6);
}

FATP_TEST_CASE(fn_tan)
{
    return checkUnary(
        "tan", [](Jet<1> v) { return tan(v); }, [](double v) { return std::tan(v); },
        [](double v) { return 1.0 + std::tan(v) * std::tan(v); }, 0.6);
}

FATP_TEST_CASE(fn_asin)
{
    return checkUnary(
        "asin", [](Jet<1> v) { return asin(v); }, [](double v) { return std::asin(v); },
        [](double v) { return 1.0 / std::sqrt(1.0 - v * v); }, 0.4);
}

FATP_TEST_CASE(fn_acos)
{
    return checkUnary(
        "acos", [](Jet<1> v) { return acos(v); }, [](double v) { return std::acos(v); },
        [](double v) { return -1.0 / std::sqrt(1.0 - v * v); }, 0.4);
}

FATP_TEST_CASE(fn_atan)
{
    return checkUnary(
        "atan", [](Jet<1> v) { return atan(v); }, [](double v) { return std::atan(v); },
        [](double v) { return 1.0 / (1.0 + v * v); }, 0.4);
}

FATP_TEST_CASE(fn_exp)
{
    return checkUnary(
        "exp", [](Jet<1> v) { return exp(v); }, [](double v) { return std::exp(v); },
        [](double v) { return std::exp(v); }, 0.7);
}

FATP_TEST_CASE(fn_log)
{
    return checkUnary(
        "log", [](Jet<1> v) { return log(v); }, [](double v) { return std::log(v); },
        [](double v) { return 1.0 / v; }, 2.3);
}

FATP_TEST_CASE(fn_sqrt)
{
    return checkUnary(
        "sqrt", [](Jet<1> v) { return sqrt(v); }, [](double v) { return std::sqrt(v); },
        [](double v) { return 0.5 / std::sqrt(v); }, 2.3);
}

FATP_TEST_CASE(fn_abs)
{
    if (!checkUnary(
            "abs+", [](Jet<1> v) { return abs(v); }, [](double v) { return std::fabs(v); },
            [](double) { return 1.0; }, 1.3))
        return false;
    return checkUnary(
        "abs-", [](Jet<1> v) { return abs(v); }, [](double v) { return std::fabs(v); },
        [](double) { return -1.0; }, -1.3);
}

FATP_TEST_CASE(fn_pow_const)
{
    return checkUnary(
        "pow_const", [](Jet<1> v) { return pow(v, 1.5); },
        [](double v) { return std::pow(v, 1.5); },
        [](double v) { return 1.5 * std::pow(v, 0.5); }, 2.3);
}

FATP_TEST_CASE(fn_pow_jet)
{
    return checkBinary(
        "pow", [](Jet<2> b, Jet<2> e) { return pow(b, e); },
        [](double b, double e) { return std::pow(b, e); },
        [](double b, double e) { return e * std::pow(b, e - 1.0); },
        [](double b, double e) { return std::pow(b, e) * std::log(b); }, 1.7, 2.1);
}

FATP_TEST_CASE(fn_hypot)
{
    return checkBinary(
        "hypot", [](Jet<2> a, Jet<2> b) { return hypot(a, b); },
        [](double a, double b) { return std::hypot(a, b); },
        [](double a, double b) { return a / std::hypot(a, b); },
        [](double a, double b) { return b / std::hypot(a, b); }, 1.2, -0.8);
}

FATP_TEST_CASE(fn_atan2)
{
    // Seeded as (y -> dir 0, x -> dir 1). d/dy = x/r^2, d/dx = -y/r^2.
    return checkBinary(
        "atan2", [](Jet<2> y, Jet<2> x) { return atan2(y, x); },
        [](double y, double x) { return std::atan2(y, x); },
        [](double y, double x) { return x / (x * x + y * y); },
        [](double y, double x) { return -y / (x * x + y * y); }, 1.2, -0.8);
}

// ============================================================================
// Domain edges and non-finite behavior
// ============================================================================

FATP_TEST_CASE(edge_guards)
{
    // sqrt at 0: value 0; derivative guarded to a finite 0 rather than +inf.
    Jet<1> rs = sqrt(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_CLOSE(rs.mValue, 0.0, "sqrt(0) value");
    FATP_ASSERT_TRUE(std::isfinite(rs.mPartials[0]), "sqrt(0) derivative is finite");
    FATP_ASSERT_CLOSE(rs.mPartials[0], 0.0, "sqrt(0) derivative guarded to 0");

    // abs: derivative 0 at 0 (subgradient), +/-1 away from 0.
    Jet<1> a0 = abs(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_CLOSE(a0.mPartials[0], 0.0, "abs'(0) taken as 0");

    // hypot and atan2 at the origin: gradient undefined, taken as 0 by convention.
    Jet<2> h0 = hypot(Jet<2>::seed(0.0, 0), Jet<2>::seed(0.0, 1));
    FATP_ASSERT_CLOSE(h0.mValue, 0.0, "hypot(0,0) value");
    FATP_ASSERT_CLOSE(h0.mPartials[0], 0.0, "hypot(0,0) d/dx convention 0");
    FATP_ASSERT_CLOSE(h0.mPartials[1], 0.0, "hypot(0,0) d/dy convention 0");

    Jet<2> t0 = atan2(Jet<2>::seed(0.0, 0), Jet<2>::seed(0.0, 1));
    FATP_ASSERT_CLOSE(t0.mValue, 0.0, "atan2(0,0) value");
    FATP_ASSERT_CLOSE(t0.mPartials[0], 0.0, "atan2(0,0) convention 0");
    FATP_ASSERT_CLOSE(t0.mPartials[1], 0.0, "atan2(0,0) convention 0");

    // pow(x, 0) is the constant 1, including at x == 0 (derivative 0, not NaN).
    Jet<1> p0 = pow(Jet<1>::seed(0.0, 0), 0.0);
    FATP_ASSERT_CLOSE(p0.mValue, 1.0, "pow(0,0) value 1");
    FATP_ASSERT_CLOSE(p0.mPartials[0], 0.0, "pow(0,0) derivative 0 (not NaN)");
    return true;
}

FATP_TEST_CASE(domain_boundaries)
{
    // log(0): value -inf, derivative +inf (both mathematically correct).
    Jet<1> l0 = log(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_TRUE(std::isinf(l0.mValue) && l0.mValue < 0.0, "log(0) value is -inf");
    FATP_ASSERT_TRUE(std::isinf(l0.mPartials[0]) && l0.mPartials[0] > 0.0, "log'(0) is +inf");

    // asin/acos at the boundary +1: finite value, unbounded derivative.
    Jet<1> as1 = asin(Jet<1>::seed(1.0, 0));
    FATP_ASSERT_TRUE(std::isfinite(as1.mValue), "asin(1) value finite");
    FATP_ASSERT_TRUE(std::isinf(as1.mPartials[0]), "asin'(1) is unbounded");

    // 1/0: value +inf, derivative -inf.
    Jet<1> inv = 1.0 / Jet<1>::seed(0.0, 0);
    FATP_ASSERT_TRUE(std::isinf(inv.mValue), "1/0 value is inf");
    FATP_ASSERT_TRUE(std::isinf(inv.mPartials[0]), "d(1/x)/dx at 0 is inf");
    return true;
}

FATP_TEST_CASE(out_of_domain_nan)
{
    // Outside a function's domain the value AND every partial are NaN, so a NaN
    // value never sits beside a finite-looking gradient. (Distinct from the
    // boundary points in domain_boundaries, where an infinite value is paired
    // with the genuinely infinite derivative.)
    Jet<1> rs = sqrt(Jet<1>::seed(-1.0, 0));
    FATP_ASSERT_TRUE(std::isnan(rs.mValue), "sqrt(neg) value NaN");
    FATP_ASSERT_TRUE(std::isnan(rs.mPartials[0]), "sqrt(neg) gradient NaN");

    Jet<1> lg = log(Jet<1>::seed(-2.0, 0));
    FATP_ASSERT_TRUE(std::isnan(lg.mValue), "log(neg) value NaN");
    FATP_ASSERT_TRUE(std::isnan(lg.mPartials[0]), "log(neg) gradient NaN");

    Jet<1> as = asin(Jet<1>::seed(2.0, 0));
    FATP_ASSERT_TRUE(std::isnan(as.mValue), "asin(2) value NaN");
    FATP_ASSERT_TRUE(std::isnan(as.mPartials[0]), "asin(2) gradient NaN");

    Jet<1> ac = acos(Jet<1>::seed(2.0, 0));
    FATP_ASSERT_TRUE(std::isnan(ac.mValue), "acos(2) value NaN");
    FATP_ASSERT_TRUE(std::isnan(ac.mPartials[0]), "acos(2) gradient NaN");

    Jet<1> pw = pow(Jet<1>::seed(-2.0, 0), 0.5);
    FATP_ASSERT_TRUE(std::isnan(pw.mValue), "pow(neg, 0.5) value NaN");
    FATP_ASSERT_TRUE(std::isnan(pw.mPartials[0]), "pow(neg, 0.5) gradient NaN");

    // A NaN argument propagates through abs to both value and gradient.
    Jet<1> ab = abs(Jet<1>{std::numeric_limits<double>::quiet_NaN()});
    FATP_ASSERT_TRUE(std::isnan(ab.mValue), "abs(NaN) value NaN");
    FATP_ASSERT_TRUE(std::isnan(ab.mPartials[0]), "abs(NaN) gradient NaN");

    // Binary functions: a NaN argument propagates to value and partials too.
    Jet<1> hn = hypot(Jet<1>{std::numeric_limits<double>::quiet_NaN()}, Jet<1>::seed(1.0, 0));
    FATP_ASSERT_TRUE(std::isnan(hn.mValue), "hypot(NaN,.) value NaN");
    FATP_ASSERT_TRUE(std::isnan(hn.mPartials[0]), "hypot(NaN,.) gradient NaN");

    Jet<1> tn = atan2(Jet<1>{std::numeric_limits<double>::quiet_NaN()}, Jet<1>::seed(1.0, 0));
    FATP_ASSERT_TRUE(std::isnan(tn.mValue), "atan2(NaN,.) value NaN");
    FATP_ASSERT_TRUE(std::isnan(tn.mPartials[0]), "atan2(NaN,.) gradient NaN");
    return true;
}

FATP_TEST_CASE(constant_propagation)
{
    // A function of a constant Jet has a zero gradient.
    Jet<3> k = sin(Jet<3>{1.0});
    for (double d : k.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "function of a constant has zero gradient");
    }

    // Mixing a seeded variable with a constant: only the seeded direction is
    // nonzero, and the constant contributes its value through the chain rule.
    Jet<2> x = Jet<2>::seed(2.0, 0);
    Jet<2> c{3.0};
    Jet<2> r = x * c; // value 6, d/dx = 3, d/dy = 0
    FATP_ASSERT_CLOSE(r.mValue, 6.0, "x*const value");
    FATP_ASSERT_CLOSE(r.mPartials[0], 3.0, "d(x*const)/dx = const");
    FATP_ASSERT_CLOSE(r.mPartials[1], 0.0, "constant carries no gradient");
    return true;
}

// ============================================================================
// Multi-input Jacobian and finite-difference cross-validation over domains
// ============================================================================

FATP_TEST_CASE(composite_jacobian)
{
    const std::array<double, 4> in{1.3, 0.7, -0.4, 0.9};
    Jet<4> x1 = Jet<4>::seed(in[0], 0);
    Jet<4> y1 = Jet<4>::seed(in[1], 1);
    Jet<4> x2 = Jet<4>::seed(in[2], 2);
    Jet<4> y2 = Jet<4>::seed(in[3], 3);

    Jet<4> kappa = (x1 * y2 - y1 * x2) / pow(x1 * x1 + y1 * y1, 1.5);
    Jet<4> speed = hypot(x1, y1);
    Jet<4> head = atan2(y1, x1);

    auto kappaScalar = [](const std::array<double, 4>& q)
    { return (q[0] * q[3] - q[1] * q[2]) / std::pow(q[0] * q[0] + q[1] * q[1], 1.5); };
    auto speedScalar = [](const std::array<double, 4>& q) { return std::hypot(q[0], q[1]); };
    auto headScalar = [](const std::array<double, 4>& q) { return std::atan2(q[1], q[0]); };

    const double h = 1e-6;
    auto checkRow = [&](const Jet<4>& jet, auto fd, const char* nm) -> bool
    {
        for (std::size_t k = 0; k < 4; ++k)
        {
            std::array<double, 4> pp = in;
            std::array<double, 4> pm = in;
            pp[k] += h;
            pm[k] -= h;
            const double diff = (fd(pp) - fd(pm)) / (2.0 * h);
            FATP_ASSERT_CLOSE_EPS(jet.mPartials[k], diff, 1e-6, nm);
        }
        return true;
    };

    if (!checkRow(kappa, kappaScalar, "curvature Jacobian row"))
        return false;
    if (!checkRow(speed, speedScalar, "speed Jacobian row"))
        return false;
    if (!checkRow(head, headScalar, "heading Jacobian row"))
        return false;
    return true;
}

FATP_TEST_CASE(fd_sweep_unary)
{
    std::mt19937 rng(12345);
    if (!fdSweepUnary("sin", [](Jet<1> v) { return sin(v); }, [](double v) { return std::sin(v); },
                      -2.0, 2.0, rng))
        return false;
    if (!fdSweepUnary("cos", [](Jet<1> v) { return cos(v); }, [](double v) { return std::cos(v); },
                      -2.0, 2.0, rng))
        return false;
    if (!fdSweepUnary("tan", [](Jet<1> v) { return tan(v); }, [](double v) { return std::tan(v); },
                      -1.0, 1.0, rng))
        return false;
    if (!fdSweepUnary("atan", [](Jet<1> v) { return atan(v); },
                      [](double v) { return std::atan(v); }, -2.0, 2.0, rng))
        return false;
    if (!fdSweepUnary("exp", [](Jet<1> v) { return exp(v); }, [](double v) { return std::exp(v); },
                      -2.0, 2.0, rng))
        return false;
    if (!fdSweepUnary("asin", [](Jet<1> v) { return asin(v); },
                      [](double v) { return std::asin(v); }, -0.85, 0.85, rng))
        return false;
    if (!fdSweepUnary("acos", [](Jet<1> v) { return acos(v); },
                      [](double v) { return std::acos(v); }, -0.85, 0.85, rng))
        return false;
    if (!fdSweepUnary("log", [](Jet<1> v) { return log(v); }, [](double v) { return std::log(v); },
                      0.1, 3.0, rng))
        return false;
    if (!fdSweepUnary("sqrt", [](Jet<1> v) { return sqrt(v); },
                      [](double v) { return std::sqrt(v); }, 0.1, 3.0, rng))
        return false;
    if (!fdSweepUnary("abs", [](Jet<1> v) { return abs(v); }, [](double v) { return std::fabs(v); },
                      0.2, 2.0, rng))
        return false;
    return true;
}

FATP_TEST_CASE(fd_sweep_binary)
{
    std::mt19937 rng(67890);
    if (!fdSweepBinary("pow", [](Jet<2> b, Jet<2> e) { return pow(b, e); },
                       [](double b, double e) { return std::pow(b, e); }, 0.3, 3.0, 0.5, 3.0, rng))
        return false;
    if (!fdSweepBinary("hypot", [](Jet<2> a, Jet<2> b) { return hypot(a, b); },
                       [](double a, double b) { return std::hypot(a, b); }, 0.3, 2.0, 0.3, 2.0, rng))
        return false;
    if (!fdSweepBinary("atan2", [](Jet<2> y, Jet<2> x) { return atan2(y, x); },
                       [](double y, double x) { return std::atan2(y, x); }, 0.3, 2.0, 0.3, 2.0, rng))
        return false;
    return true;
}

FATP_TEST_CASE(atan2_extreme_scale)
{
    // The derivative magnitude must survive inputs whose squares overflow or
    // underflow. With x == y == a, d/dy = 1/(2a) and d/dx = -1/(2a); a naive
    // x^2 + y^2 denominator collapses these to 0 for extreme a.
    for (double a : {1.0e200, 1.0e-200, 1.0})
    {
        Jet<2> r = atan2(Jet<2>::seed(a, 0), Jet<2>::seed(a, 1));
        FATP_ASSERT_TRUE(std::isfinite(r.mPartials[0]), "atan2 extreme d/dy finite");
        FATP_ASSERT_CLOSE_EPS(r.mPartials[0] * (2.0 * a), 1.0, 1e-9, "atan2 extreme d/dy = 1/(2a)");
        FATP_ASSERT_CLOSE_EPS(r.mPartials[1] * (2.0 * a), -1.0, 1e-9, "atan2 extreme d/dx = -1/(2a)");
    }
    return true;
}

FATP_TEST_CASE(documented_edge_contracts)
{
    // These pin the CURRENT documented contract for two cases whose policy is
    // open (NaN*0 suppression has not been adopted). If that policy changes,
    // update these expectations deliberately.

    // pow(Jet, Jet) requires a positive base. With a non-positive base the value
    // is still correct, but the exponent-side term carries log(base) = NaN, which
    // poisons the gradient even for a constant exponent. Use pow(x, double) there.
    Jet<1> pn = pow(Jet<1>::seed(-2.0, 0), Jet<1>{2.0});
    FATP_ASSERT_CLOSE(pn.mValue, 4.0, "pow(-2, const 2) value correct");
    FATP_ASSERT_TRUE(std::isnan(pn.mPartials[0]), "pow(-2, const 2) gradient NaN (contract)");

    Jet<1> pz = pow(Jet<1>::seed(0.0, 0), Jet<1>{2.0});
    FATP_ASSERT_CLOSE(pz.mValue, 0.0, "pow(0, const 2) value 0");
    FATP_ASSERT_TRUE(std::isnan(pz.mPartials[0]), "pow(0, const 2) gradient NaN (contract)");

    // pow(x, double) on the same negative base differentiates correctly.
    Jet<1> pd = pow(Jet<1>::seed(-2.0, 0), 2.0);
    FATP_ASSERT_CLOSE(pd.mValue, 4.0, "pow(-2, 2.0) value 4");
    FATP_ASSERT_CLOSE(pd.mPartials[0], -4.0, "pow(-2, 2.0) d/dx = 2x = -4");

    // log at +0 has a +inf slope; at signed -0.0 the IEEE reciprocal is -inf.
    Jet<1> lp = log(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_TRUE(lp.mValue == -std::numeric_limits<double>::infinity(), "log(+0) value -inf");
    FATP_ASSERT_TRUE(lp.mPartials[0] == std::numeric_limits<double>::infinity(), "log(+0) slope +inf");
    Jet<1> ln = log(Jet<1>::seed(-0.0, 0));
    FATP_ASSERT_TRUE(ln.mValue == -std::numeric_limits<double>::infinity(), "log(-0) value -inf");
    FATP_ASSERT_TRUE(ln.mPartials[0] == -std::numeric_limits<double>::infinity(), "log(-0) slope -inf (signed zero)");
    return true;
}

constexpr Jet<1> compoundChainCE()
{
    Jet<1> t = Jet<1>::seed(1.0, 0);
    t += 2.0; // (3; 1)
    t *= 3.0; // (9; 3)
    return t;
}

FATP_TEST_CASE(seed_compile_time)
{
    // seed<K>() matches the runtime seed for the same direction; the direction
    // is checked at compile time rather than asserted at runtime.
    Jet<3> ck = Jet<3>::seed<2>(5.0);
    Jet<3> rk = Jet<3>::seed(5.0, 2);
    FATP_ASSERT_CLOSE(ck.mValue, rk.mValue, "seed<K> value");
    for (std::size_t i = 0; i < 3; ++i)
    {
        FATP_ASSERT_CLOSE(ck.mPartials[i], rk.mPartials[i], "seed<K> partial matches runtime seed");
    }
    constexpr Jet<2> c = Jet<2>::seed<1>(2.0);
    static_assert(c.mPartials[1] == 1.0 && c.mPartials[0] == 0.0);
    FATP_ASSERT_CLOSE(c.mValue, 2.0, "constexpr seed<K> value");
    return true;
}

FATP_TEST_CASE(compound_assignment)
{
    // Each compound form must equal the matching binary operator in value and
    // gradient, for Jet and scalar right-hand sides.
    const Jet<1> x = Jet<1>::seed(2.0, 0);
    const Jet<1> y = Jet<1>::seed(3.0, 0);

    const Jet<1> rAdd = x + y; Jet<1> a = x; a += y;
    FATP_ASSERT_CLOSE(a.mValue, rAdd.mValue, "+=(Jet) value");
    FATP_ASSERT_CLOSE(a.mPartials[0], rAdd.mPartials[0], "+=(Jet) grad");
    const Jet<1> rSub = x - y; Jet<1> s = x; s -= y;
    FATP_ASSERT_CLOSE(s.mValue, rSub.mValue, "-=(Jet) value");
    FATP_ASSERT_CLOSE(s.mPartials[0], rSub.mPartials[0], "-=(Jet) grad");
    const Jet<1> rMul = x * y; Jet<1> m = x; m *= y;
    FATP_ASSERT_CLOSE(m.mValue, rMul.mValue, "*=(Jet) value");
    FATP_ASSERT_CLOSE(m.mPartials[0], rMul.mPartials[0], "*=(Jet) grad");
    const Jet<1> rDiv = x / y; Jet<1> d = x; d /= y;
    FATP_ASSERT_CLOSE(d.mValue, rDiv.mValue, "/=(Jet) value");
    FATP_ASSERT_CLOSE(d.mPartials[0], rDiv.mPartials[0], "/=(Jet) grad");

    const Jet<1> rAddS = x + 4.0; Jet<1> as = x; as += 4.0;
    FATP_ASSERT_CLOSE(as.mValue, rAddS.mValue, "+=(double) value");
    FATP_ASSERT_CLOSE(as.mPartials[0], rAddS.mPartials[0], "+=(double) grad");
    const Jet<1> rSubS = x - 4.0; Jet<1> ss = x; ss -= 4.0;
    FATP_ASSERT_CLOSE(ss.mValue, rSubS.mValue, "-=(double) value");
    const Jet<1> rMulS = x * 3.0; Jet<1> ms = x; ms *= 3.0;
    FATP_ASSERT_CLOSE(ms.mValue, rMulS.mValue, "*=(double) value");
    FATP_ASSERT_CLOSE(ms.mPartials[0], rMulS.mPartials[0], "*=(double) grad");
    const Jet<1> rDivS = x / 2.0; Jet<1> ds = x; ds /= 2.0;
    FATP_ASSERT_CLOSE(ds.mValue, rDivS.mValue, "/=(double) value");
    FATP_ASSERT_CLOSE(ds.mPartials[0], rDivS.mPartials[0], "/=(double) grad");

    static_assert(compoundChainCE().mValue == 9.0 && compoundChainCE().mPartials[0] == 3.0);
    return true;
}

FATP_TEST_CASE(scalar_function_overloads)
{
    // Scalar overloads must agree with the Jet/Jet form fed a constant Jet, in
    // value and gradient; the constant argument contributes no partial.
    const Jet<1> x = Jet<1>::seed(3.0, 0);

    const Jet<1> hRef = hypot(x, Jet<1>{4.0});
    Jet<1> h1 = hypot(x, 4.0);
    FATP_ASSERT_CLOSE(h1.mValue, hRef.mValue, "hypot(Jet,double) value");
    FATP_ASSERT_CLOSE(h1.mPartials[0], hRef.mPartials[0], "hypot(Jet,double) grad");
    FATP_ASSERT_CLOSE(h1.mPartials[0], 3.0 / 5.0, "hypot(Jet,double) grad x/h");
    Jet<1> h2 = hypot(4.0, x);
    FATP_ASSERT_CLOSE(h2.mValue, 5.0, "hypot(double,Jet) value");
    FATP_ASSERT_CLOSE(h2.mPartials[0], 3.0 / 5.0, "hypot(double,Jet) grad");

    const Jet<1> a1Ref = atan2(x, Jet<1>{1.0});
    Jet<1> a1 = atan2(x, 1.0);
    FATP_ASSERT_CLOSE(a1.mValue, a1Ref.mValue, "atan2(Jet,double) value");
    FATP_ASSERT_CLOSE(a1.mPartials[0], a1Ref.mPartials[0], "atan2(Jet,double) grad");
    const Jet<1> a2Ref = atan2(Jet<1>{1.0}, x);
    Jet<1> a2 = atan2(1.0, x);
    FATP_ASSERT_CLOSE(a2.mValue, a2Ref.mValue, "atan2(double,Jet) value");
    FATP_ASSERT_CLOSE(a2.mPartials[0], a2Ref.mPartials[0], "atan2(double,Jet) grad");

    Jet<1> pe = pow(2.0, x);
    FATP_ASSERT_CLOSE(pe.mValue, 8.0, "pow(double,Jet) value");
    FATP_ASSERT_CLOSE(pe.mPartials[0], 8.0 * std::log(2.0), "pow(double,Jet) grad = v ln(base)");
    const Jet<1> pOne = pow(1.0, x);
    FATP_ASSERT_CLOSE(pOne.mPartials[0], 0.0, "pow(1,exp) grad 0 (constant)");
    const Jet<1> pn = pow(-2.0, x);
    FATP_ASSERT_TRUE(std::isnan(pn.mPartials[0]), "pow(neg,Jet) gradient NaN (contract)");
    return true;
}

} // namespace fat_p::testing::jet

// ============================================================================
// Public interface
// ============================================================================

namespace fat_p::testing
{

bool test_Jet()
{
    FATP_PRINT_HEADER(JET)

    TestRunner runner;
    auto& out = *get_test_config().output;

    out << colors::blue() << "--- Construction & Seeding ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, default_construction);
    FATP_RUN_TEST_NS(runner, jet, constant_construction);
    FATP_RUN_TEST_NS(runner, jet, seed_basic);
    FATP_RUN_TEST_NS(runner, jet, seed_high_dimension);
    FATP_RUN_TEST_NS(runner, jet, value_semantics);

    out << "\n" << colors::blue() << "--- constexpr Contract ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, constexpr_arithmetic);
    FATP_RUN_TEST_NS(runner, jet, constexpr_seed_and_compare);

    out << "\n" << colors::blue() << "--- Comparison ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, comparison_jet_jet);
    FATP_RUN_TEST_NS(runner, jet, comparison_jet_scalar);
    FATP_RUN_TEST_NS(runner, jet, comparison_value_only);
    FATP_RUN_TEST_NS(runner, jet, comparison_nan_unordered);

    out << "\n" << colors::blue() << "--- Arithmetic ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, add);
    FATP_RUN_TEST_NS(runner, jet, subtract);
    FATP_RUN_TEST_NS(runner, jet, multiply);
    FATP_RUN_TEST_NS(runner, jet, divide);
    FATP_RUN_TEST_NS(runner, jet, unary_negate);
    FATP_RUN_TEST_NS(runner, jet, scalar_arithmetic);

    out << "\n" << colors::blue() << "--- Algebraic Identities ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, additive_identities);
    FATP_RUN_TEST_NS(runner, jet, multiplicative_identities);
    FATP_RUN_TEST_NS(runner, jet, commutativity);

    out << "\n" << colors::blue() << "--- Elementary Functions ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, fn_sin);
    FATP_RUN_TEST_NS(runner, jet, fn_cos);
    FATP_RUN_TEST_NS(runner, jet, fn_tan);
    FATP_RUN_TEST_NS(runner, jet, fn_asin);
    FATP_RUN_TEST_NS(runner, jet, fn_acos);
    FATP_RUN_TEST_NS(runner, jet, fn_atan);
    FATP_RUN_TEST_NS(runner, jet, fn_exp);
    FATP_RUN_TEST_NS(runner, jet, fn_log);
    FATP_RUN_TEST_NS(runner, jet, fn_sqrt);
    FATP_RUN_TEST_NS(runner, jet, fn_abs);
    FATP_RUN_TEST_NS(runner, jet, fn_pow_const);
    FATP_RUN_TEST_NS(runner, jet, fn_pow_jet);
    FATP_RUN_TEST_NS(runner, jet, fn_hypot);
    FATP_RUN_TEST_NS(runner, jet, fn_atan2);

    out << "\n" << colors::blue() << "--- Domain Edges ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, edge_guards);
    FATP_RUN_TEST_NS(runner, jet, domain_boundaries);
    FATP_RUN_TEST_NS(runner, jet, out_of_domain_nan);
    FATP_RUN_TEST_NS(runner, jet, constant_propagation);

    out << "\n" << colors::blue() << "--- Jacobian & Cross-Validation ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, composite_jacobian);
    FATP_RUN_TEST_NS(runner, jet, atan2_extreme_scale);
    FATP_RUN_TEST_NS(runner, jet, documented_edge_contracts);
    FATP_RUN_TEST_NS(runner, jet, seed_compile_time);
    FATP_RUN_TEST_NS(runner, jet, compound_assignment);
    FATP_RUN_TEST_NS(runner, jet, scalar_function_overloads);
    FATP_RUN_TEST_NS(runner, jet, fd_sweep_unary);
    FATP_RUN_TEST_NS(runner, jet, fd_sweep_binary);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Jet() ? 0 : 1;
}
#endif
