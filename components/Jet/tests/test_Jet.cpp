/**
 * @file test_Jet.cpp
 * @brief Unit tests for Jet.h (forward-mode AD scalar).
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: test
  path: components/Jet/tests/test_Jet.cpp
  namespace: fat_p::testing::jet
  layer: Testing
  summary: Unit tests for the Jet forward-mode AD scalar.
  api_stability: in_work
  related:
    headers:
      - include/fat_p/Jet.h
      - include/fat_p/FatPTest.h
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
// Helpers: compare AD value and partials against analytic references.
// The FATP_ASSERT_* macros return false from the enclosing function on failure.
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

// ============================================================================
// Construction & seeding
// ============================================================================

FATP_TEST_CASE(construction)
{
    Jet<3> z;
    FATP_ASSERT_CLOSE(z.mValue, 0.0, "default value is 0");
    for (double d : z.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "default partials are 0");
    }

    Jet<3> c{5.0};
    FATP_ASSERT_CLOSE(c.mValue, 5.0, "constant value");
    for (double d : c.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "constant Jet has zero partials");
    }
    return true;
}

FATP_TEST_CASE(seed)
{
    Jet<4> s = Jet<4>::seed(2.5, 2);
    FATP_ASSERT_CLOSE(s.mValue, 2.5, "seed value");
    FATP_ASSERT_CLOSE(s.mPartials[2], 1.0, "seeded direction is unit");
    FATP_ASSERT_CLOSE(s.mPartials[0], 0.0, "unseeded direction 0");
    FATP_ASSERT_CLOSE(s.mPartials[1], 0.0, "unseeded direction 1");
    FATP_ASSERT_CLOSE(s.mPartials[3], 0.0, "unseeded direction 3");

    Jet<1> one = Jet<1>::seed(9.0, 0);
    FATP_ASSERT_CLOSE(one.mPartials[0], 1.0, "N=1 seed is unit");
    return true;
}

FATP_TEST_CASE(constexpr_evaluation)
{
    constexpr Jet<2> p = Jet<2>::seed(3.0, 0) * Jet<2>::seed(4.0, 1);
    static_assert(p.mValue == 12.0, "constexpr product value");
    static_assert(p.mPartials[0] == 4.0, "constexpr d/dx of x*y is y");
    static_assert(p.mPartials[1] == 3.0, "constexpr d/dy of x*y is x");

    constexpr Jet<1> q = Jet<1>::seed(5.0, 0) + 2.0;
    static_assert(q.mValue == 7.0, "constexpr scalar add value");
    static_assert(q.mPartials[0] == 1.0, "constexpr scalar add derivative");

    FATP_ASSERT_TRUE(true, "constexpr arithmetic evaluated at compile time");
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

    static_assert(std::is_trivially_copyable_v<Jet<3>>, "Jet is a trivially copyable value type");
    return true;
}

// ============================================================================
// Arithmetic & comparison
// ============================================================================

FATP_TEST_CASE(arithmetic)
{
    Jet<2> x = Jet<2>::seed(3.0, 0);
    Jet<2> y = Jet<2>::seed(4.0, 1);

    Jet<2> sum = x + y;
    FATP_ASSERT_CLOSE(sum.mValue, 7.0, "sum value");
    FATP_ASSERT_CLOSE(sum.mPartials[0], 1.0, "d(x+y)/dx");
    FATP_ASSERT_CLOSE(sum.mPartials[1], 1.0, "d(x+y)/dy");

    Jet<2> diff = x - y;
    FATP_ASSERT_CLOSE(diff.mPartials[1], -1.0, "d(x-y)/dy");

    Jet<2> prod = x * y;
    FATP_ASSERT_CLOSE(prod.mValue, 12.0, "product value");
    FATP_ASSERT_CLOSE(prod.mPartials[0], 4.0, "d(xy)/dx = y");
    FATP_ASSERT_CLOSE(prod.mPartials[1], 3.0, "d(xy)/dy = x");

    Jet<2> quot = x / y;
    FATP_ASSERT_CLOSE(quot.mValue, 0.75, "quotient value");
    FATP_ASSERT_CLOSE(quot.mPartials[0], 0.25, "d(x/y)/dx = 1/y");
    FATP_ASSERT_CLOSE(quot.mPartials[1], -3.0 / 16.0, "d(x/y)/dy = -x/y^2");

    Jet<2> neg = -x;
    FATP_ASSERT_CLOSE(neg.mValue, -3.0, "negation value");
    FATP_ASSERT_CLOSE(neg.mPartials[0], -1.0, "d(-x)/dx");

    Jet<2> twoX = 2.0 * x;
    Jet<2> xTwo = x * 2.0;
    Jet<2> xPlus = x + 5.0;
    Jet<2> fiveMinus = 5.0 - x;
    Jet<2> xHalf = x / 2.0;
    Jet<2> sixOver = 6.0 / x;
    FATP_ASSERT_CLOSE(twoX.mPartials[0], 2.0, "d(2x)/dx");
    FATP_ASSERT_CLOSE(xTwo.mPartials[0], 2.0, "d(x*2)/dx");
    FATP_ASSERT_CLOSE(xPlus.mValue, 8.0, "x+5 value");
    FATP_ASSERT_CLOSE(xPlus.mPartials[0], 1.0, "d(x+5)/dx unchanged");
    FATP_ASSERT_CLOSE(fiveMinus.mValue, 2.0, "5-x value");
    FATP_ASSERT_CLOSE(fiveMinus.mPartials[0], -1.0, "d(5-x)/dx");
    FATP_ASSERT_CLOSE(xHalf.mPartials[0], 0.5, "d(x/2)/dx");
    FATP_ASSERT_CLOSE(sixOver.mValue, 2.0, "6/x value");
    FATP_ASSERT_CLOSE(sixOver.mPartials[0], -6.0 / 9.0, "d(6/x)/dx = -6/x^2");
    return true;
}

FATP_TEST_CASE(comparison)
{
    Jet<1> one = Jet<1>::seed(1.0, 0);
    Jet<1> two = Jet<1>::seed(2.0, 0);

    FATP_ASSERT_TRUE(one < two, "1 < 2");
    FATP_ASSERT_TRUE(two > one, "2 > 1");
    FATP_ASSERT_TRUE(one <= one, "1 <= 1");
    FATP_ASSERT_FALSE(one == two, "1 != 2");

    FATP_ASSERT_TRUE(one < 2.0, "jet < scalar");
    FATP_ASSERT_TRUE(2.0 > one, "scalar > jet (synthesized reversed form)");
    FATP_ASSERT_TRUE(one == 1.0, "jet == scalar");

    // Ordering and equality use the value only; partials are ignored.
    Jet<2> a = Jet<2>::seed(1.0, 0);
    Jet<2> b = Jet<2>::seed(1.0, 1);
    FATP_ASSERT_TRUE(a == b, "equal values compare equal regardless of partials");
    FATP_ASSERT_NE(a.mPartials[0], b.mPartials[0], "partials genuinely differ");

    // NaN value yields an unordered comparison (partial order).
    Jet<1> nan{std::numeric_limits<double>::quiet_NaN()};
    const bool unordered = (nan <=> Jet<1>{0.0}) == std::partial_ordering::unordered;
    FATP_ASSERT_TRUE(unordered, "NaN value compares unordered");
    return true;
}

// ============================================================================
// Elementary functions (value vs std, derivative vs analytic)
// ============================================================================

FATP_TEST_CASE(elementary_unary)
{
    if (!checkUnary(
            "sin", [](Jet<1> x) { return sin(x); }, [](double x) { return std::sin(x); },
            [](double x) { return std::cos(x); }, 0.6))
        return false;
    if (!checkUnary(
            "cos", [](Jet<1> x) { return cos(x); }, [](double x) { return std::cos(x); },
            [](double x) { return -std::sin(x); }, 0.6))
        return false;
    if (!checkUnary(
            "tan", [](Jet<1> x) { return tan(x); }, [](double x) { return std::tan(x); },
            [](double x) { return 1.0 + std::tan(x) * std::tan(x); }, 0.6))
        return false;
    if (!checkUnary(
            "asin", [](Jet<1> x) { return asin(x); }, [](double x) { return std::asin(x); },
            [](double x) { return 1.0 / std::sqrt(1.0 - x * x); }, 0.4))
        return false;
    if (!checkUnary(
            "acos", [](Jet<1> x) { return acos(x); }, [](double x) { return std::acos(x); },
            [](double x) { return -1.0 / std::sqrt(1.0 - x * x); }, 0.4))
        return false;
    if (!checkUnary(
            "atan", [](Jet<1> x) { return atan(x); }, [](double x) { return std::atan(x); },
            [](double x) { return 1.0 / (1.0 + x * x); }, 0.4))
        return false;
    if (!checkUnary(
            "exp", [](Jet<1> x) { return exp(x); }, [](double x) { return std::exp(x); },
            [](double x) { return std::exp(x); }, 0.7))
        return false;
    if (!checkUnary(
            "log", [](Jet<1> x) { return log(x); }, [](double x) { return std::log(x); },
            [](double x) { return 1.0 / x; }, 2.3))
        return false;
    if (!checkUnary(
            "sqrt", [](Jet<1> x) { return sqrt(x); }, [](double x) { return std::sqrt(x); },
            [](double x) { return 0.5 / std::sqrt(x); }, 2.3))
        return false;
    if (!checkUnary(
            "abs", [](Jet<1> x) { return abs(x); }, [](double x) { return std::fabs(x); },
            [](double) { return -1.0; }, -1.3))
        return false;
    return true;
}

FATP_TEST_CASE(elementary_binary)
{
    // pow with a constant exponent: (x^p)' = p x^(p-1).
    if (!checkUnary(
            "pow_const", [](Jet<1> x) { return pow(x, 1.5); },
            [](double x) { return std::pow(x, 1.5); },
            [](double x) { return 1.5 * std::pow(x, 0.5); }, 2.3))
        return false;

    // pow(x, y): d/dx = y x^(y-1), d/dy = x^y ln x.
    if (!checkBinary(
            "pow", [](Jet<2> x, Jet<2> y) { return pow(x, y); },
            [](double x, double y) { return std::pow(x, y); },
            [](double x, double y) { return y * std::pow(x, y - 1.0); },
            [](double x, double y) { return std::pow(x, y) * std::log(x); }, 1.7, 2.1))
        return false;

    // hypot(x, y): d/dx = x/h, d/dy = y/h.
    if (!checkBinary(
            "hypot", [](Jet<2> x, Jet<2> y) { return hypot(x, y); },
            [](double x, double y) { return std::hypot(x, y); },
            [](double x, double y) { return x / std::hypot(x, y); },
            [](double x, double y) { return y / std::hypot(x, y); }, 1.2, -0.8))
        return false;

    // atan2(y, x): seeded as (y -> dir 0, x -> dir 1). d/dy = x/r^2, d/dx = -y/r^2.
    if (!checkBinary(
            "atan2", [](Jet<2> y, Jet<2> x) { return atan2(y, x); },
            [](double y, double x) { return std::atan2(y, x); },
            [](double y, double x) { return x / (x * x + y * y); },
            [](double y, double x) { return -y / (x * x + y * y); }, 1.2, -0.8))
        return false;
    return true;
}

FATP_TEST_CASE(edge_cases)
{
    // sqrt at 0: value 0, derivative guarded to 0 rather than +inf.
    Jet<1> rs = sqrt(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_CLOSE(rs.mValue, 0.0, "sqrt(0) value");
    FATP_ASSERT_TRUE(std::isfinite(rs.mPartials[0]), "sqrt(0) derivative is finite");
    FATP_ASSERT_CLOSE(rs.mPartials[0], 0.0, "sqrt(0) derivative guarded to 0");

    // abs: derivative 0 at 0, -1 for negative argument.
    Jet<1> a0 = abs(Jet<1>::seed(0.0, 0));
    FATP_ASSERT_CLOSE(a0.mPartials[0], 0.0, "abs'(0) taken as 0");
    Jet<1> an = abs(Jet<1>::seed(-2.0, 0));
    FATP_ASSERT_CLOSE(an.mValue, 2.0, "abs(-2) value");
    FATP_ASSERT_CLOSE(an.mPartials[0], -1.0, "abs'(-2) = -1");

    // A constant Jet carries zero partials, so its gradient stays zero.
    Jet<3> k = sin(Jet<3>{1.0});
    for (double d : k.mPartials)
    {
        FATP_ASSERT_CLOSE(d, 0.0, "function of a constant has zero gradient");
    }
    return true;
}

// ============================================================================
// Jacobian assembly & finite-difference cross-validation
// ============================================================================

FATP_TEST_CASE(composite_jacobian)
{
    // Curvature of a planar curve from its first two derivatives, w.r.t.
    // (x', y', x'', y'') -- a row of the kind of Jacobian a solver needs.
    const std::array<double, 4> in{1.3, 0.7, -0.4, 0.9};
    Jet<4> x1 = Jet<4>::seed(in[0], 0);
    Jet<4> y1 = Jet<4>::seed(in[1], 1);
    Jet<4> x2 = Jet<4>::seed(in[2], 2);
    Jet<4> y2 = Jet<4>::seed(in[3], 3);

    Jet<4> kappa = (x1 * y2 - y1 * x2) / pow(x1 * x1 + y1 * y1, 1.5);

    auto kappaScalar = [](const std::array<double, 4>& q)
    { return (q[0] * q[3] - q[1] * q[2]) / std::pow(q[0] * q[0] + q[1] * q[1], 1.5); };

    const double h = 1e-6;
    for (std::size_t k = 0; k < 4; ++k)
    {
        std::array<double, 4> pp = in;
        std::array<double, 4> pm = in;
        pp[k] += h;
        pm[k] -= h;
        const double fd = (kappaScalar(pp) - kappaScalar(pm)) / (2.0 * h);
        FATP_ASSERT_CLOSE_EPS(kappa.mPartials[k], fd, 1e-6, "curvature Jacobian row vs FD");
    }
    return true;
}

FATP_TEST_CASE(fd_cross_validation)
{
    std::mt19937 rng(12345); // fixed seed for reproducibility
    std::uniform_real_distribution<double> dist(0.25, 3.0);
    const double h = 1e-6;
    const double eps = 1e-6;

    for (int i = 0; i < 200; ++i)
    {
        const double x0 = dist(rng);

        const double adSin = sin(Jet<1>::seed(x0, 0)).mPartials[0];
        FATP_ASSERT_CLOSE_EPS(adSin, (std::sin(x0 + h) - std::sin(x0 - h)) / (2.0 * h), eps,
                              "sin AD vs FD");

        const double adExp = exp(Jet<1>::seed(x0, 0)).mPartials[0];
        FATP_ASSERT_CLOSE_EPS(adExp, (std::exp(x0 + h) - std::exp(x0 - h)) / (2.0 * h), eps,
                              "exp AD vs FD");

        const double adLog = log(Jet<1>::seed(x0, 0)).mPartials[0];
        FATP_ASSERT_CLOSE_EPS(adLog, (std::log(x0 + h) - std::log(x0 - h)) / (2.0 * h), eps,
                              "log AD vs FD");

        const double adSqrt = sqrt(Jet<1>::seed(x0, 0)).mPartials[0];
        FATP_ASSERT_CLOSE_EPS(adSqrt, (std::sqrt(x0 + h) - std::sqrt(x0 - h)) / (2.0 * h), eps,
                              "sqrt AD vs FD");
    }
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
    FATP_RUN_TEST_NS(runner, jet, construction);
    FATP_RUN_TEST_NS(runner, jet, seed);
    FATP_RUN_TEST_NS(runner, jet, constexpr_evaluation);
    FATP_RUN_TEST_NS(runner, jet, value_semantics);

    out << "\n" << colors::blue() << "--- Arithmetic & Comparison ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, arithmetic);
    FATP_RUN_TEST_NS(runner, jet, comparison);

    out << "\n" << colors::blue() << "--- Elementary Functions ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, elementary_unary);
    FATP_RUN_TEST_NS(runner, jet, elementary_binary);
    FATP_RUN_TEST_NS(runner, jet, edge_cases);

    out << "\n" << colors::blue() << "--- Jacobian & Cross-Validation ---" << colors::reset() << "\n";
    FATP_RUN_TEST_NS(runner, jet, composite_jacobian);
    FATP_RUN_TEST_NS(runner, jet, fd_cross_validation);

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Jet() ? 0 : 1;
}
#endif
