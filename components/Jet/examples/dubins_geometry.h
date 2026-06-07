#pragma once

/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: example
  path: components/Jet/examples/dubins_geometry.h
  namespace: dubins
  layer: Examples
  summary: Shared scalar-templated Dubins path geometry for the Jet examples.
  api_stability: in_work
  related:
    docs_search: "Jet"
  hygiene:
    pragma_once: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file dubins_geometry.h
 * @brief Dubins shortest-path geometry, templated on the scalar type.
 *
 * The geometry is written once over a scalar T, so the same code yields the
 * path with T = double and the path length plus its gradient with T = Jet<N>.
 * The closed-form word magnitudes follow A. Walker's dubins.c, after Shkel &
 * Lumelsky. Shared by dubins_jet.cpp (single segment) and dubins_chain_jet.cpp
 * (a multi-segment tour with per-segment Jacobian assembly).
 *
 * @see fat_p/Jet.h
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "fat_p/Jet.h"

namespace dubins
{

using fat_p::autodiff::Jet;

/// pi and 2*pi.
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

// ============================================================================
// Scalar helpers
// ============================================================================

/// Scalar value of T, read for branch and validity decisions.
inline double valueOf(double x)
{
    return x;
}
template <std::size_t N>
inline double valueOf(const Jet<N>& j)
{
    return j.mValue;
}

/**
 * @brief Wrap an angle to [0, 2*pi): x - 2*pi*floor(x / 2*pi).
 *
 * For a Jet this subtracts a constant multiple of 2*pi, so the derivative
 * passes through unchanged -- a wrap is locally an additive constant.
 */
template <class T>
T mod2pi(const T& x)
{
    const double k = std::floor(valueOf(x) / kTwoPi);
    return x - kTwoPi * k;
}

/// Segment kinds: left arc, straight, right arc.
enum class Seg
{
    Left,
    Straight,
    Right
};

/// One word's normalized segment magnitudes (unit-radius frame).
template <class T>
struct Word
{
    bool valid = false;
    T t{};
    T p{};
    T q{};
};

// ============================================================================
// The six Dubins words. Validity is decided on the scalar value; sqrt and acos
// are evaluated only on in-range arguments.
// ============================================================================

/// Word L-S-L.
template <class T>
Word<T> wordLSL(const T& a, const T& b, const T& d)
{
    using std::atan2;
    using std::cos;
    using std::sin;
    using std::sqrt;
    Word<T> w;
    const T psq = 2.0 + d * d - 2.0 * cos(a - b) + 2.0 * d * (sin(a) - sin(b));
    if (valueOf(psq) < 0.0) return w;
    const T u = atan2(cos(b) - cos(a), d + sin(a) - sin(b));
    w.t     = mod2pi(-a + u);
    w.p     = sqrt(psq);
    w.q     = mod2pi(b - u);
    w.valid = true;
    return w;
}

/// Word R-S-R.
template <class T>
Word<T> wordRSR(const T& a, const T& b, const T& d)
{
    using std::atan2;
    using std::cos;
    using std::sin;
    using std::sqrt;
    Word<T> w;
    const T psq = 2.0 + d * d - 2.0 * cos(a - b) + 2.0 * d * (sin(b) - sin(a));
    if (valueOf(psq) < 0.0) return w;
    const T u = atan2(cos(a) - cos(b), d - sin(a) + sin(b));
    w.t     = mod2pi(a - u);
    w.p     = sqrt(psq);
    w.q     = mod2pi(-b + u);
    w.valid = true;
    return w;
}

/// Word L-S-R.
template <class T>
Word<T> wordLSR(const T& a, const T& b, const T& d)
{
    using std::atan2;
    using std::cos;
    using std::sin;
    using std::sqrt;
    Word<T> w;
    const T psq = -2.0 + d * d + 2.0 * cos(a - b) + 2.0 * d * (sin(a) + sin(b));
    if (valueOf(psq) < 0.0) return w;
    const T p = sqrt(psq);
    const T u = atan2(-cos(a) - cos(b), d + sin(a) + sin(b)) - atan2(-2.0, p);
    w.t     = mod2pi(-a + u);
    w.p     = p;
    w.q     = mod2pi(-b + u);
    w.valid = true;
    return w;
}

/// Word R-S-L.
template <class T>
Word<T> wordRSL(const T& a, const T& b, const T& d)
{
    using std::atan2;
    using std::cos;
    using std::sin;
    using std::sqrt;
    Word<T> w;
    const T psq = -2.0 + d * d + 2.0 * cos(a - b) - 2.0 * d * (sin(a) + sin(b));
    if (valueOf(psq) < 0.0) return w;
    const T p = sqrt(psq);
    const T u = atan2(cos(a) + cos(b), d - sin(a) - sin(b)) - atan2(2.0, p);
    w.t     = mod2pi(a - u);
    w.p     = p;
    w.q     = mod2pi(b - u);
    w.valid = true;
    return w;
}

/// Word R-L-R.
template <class T>
Word<T> wordRLR(const T& a, const T& b, const T& d)
{
    using std::acos;
    using std::atan2;
    using std::cos;
    using std::sin;
    Word<T> w;
    const T tmp = (6.0 - d * d + 2.0 * cos(a - b) + 2.0 * d * (sin(a) - sin(b))) / 8.0;
    if (std::fabs(valueOf(tmp)) > 1.0) return w;
    const T p = mod2pi(kTwoPi - acos(tmp));
    const T t = mod2pi(a - atan2(cos(a) - cos(b), d - sin(a) + sin(b)) + mod2pi(p / 2.0));
    w.t     = t;
    w.p     = p;
    w.q     = mod2pi(a - b - t + mod2pi(p));
    w.valid = true;
    return w;
}

/// Word L-R-L.
template <class T>
Word<T> wordLRL(const T& a, const T& b, const T& d)
{
    using std::acos;
    using std::atan2;
    using std::cos;
    using std::sin;
    Word<T> w;
    const T tmp = (6.0 - d * d + 2.0 * cos(a - b) + 2.0 * d * (-sin(a) + sin(b))) / 8.0;
    if (std::fabs(valueOf(tmp)) > 1.0) return w;
    const T p = mod2pi(kTwoPi - acos(tmp));
    const T t = mod2pi(-a - atan2(cos(a) - cos(b), d + sin(a) - sin(b)) + p / 2.0);
    w.t     = t;
    w.p     = p;
    w.q     = mod2pi(mod2pi(b) - a - t + mod2pi(p));
    w.valid = true;
    return w;
}

/// Word names, indexed as the words are evaluated below.
inline constexpr const char* kWordNames[6] = {"LSL", "LSR", "RSL", "RSR", "RLR", "LRL"};

/// Segment kinds per word, parallel to kWordNames.
inline constexpr std::array<std::array<Seg, 3>, 6> kSegs = {{
    {{Seg::Left, Seg::Straight, Seg::Left}},
    {{Seg::Left, Seg::Straight, Seg::Right}},
    {{Seg::Right, Seg::Straight, Seg::Left}},
    {{Seg::Right, Seg::Straight, Seg::Right}},
    {{Seg::Right, Seg::Left, Seg::Right}},
    {{Seg::Left, Seg::Right, Seg::Left}},
}};

// ============================================================================
// Shortest-path selection
// ============================================================================

/// The chosen word and the resulting path.
template <class T>
struct Result
{
    int word = -1; // index into kWordNames / kSegs, -1 if no word is valid
    T t{};
    T p{};
    T q{};      // normalized magnitudes of the chosen word
    T length{}; // actual total length = (t + p + q) * rho
    T alpha{};
    T beta{};
    T d{}; // the normalized problem, for inspection
};

/**
 * @brief Shortest Dubins path between two oriented poses, minimum radius rho.
 *
 * Normalizes to the unit-radius frame, evaluates all six words, and returns the
 * shortest valid one. With T = double this is the path; with T = Jet<N> the
 * returned length carries the gradient with respect to the seeded inputs.
 *
 * @return The chosen word, its segment magnitudes, and the actual total length;
 *         word == -1 only at degenerate configurations where no word is valid.
 */
template <class T>
Result<T> dubinsShortest(T x0, T y0, T th0, T x1, T y1, T th1, T rho)
{
    using std::atan2;
    using std::hypot;

    const T dx    = x1 - x0;
    const T dy    = y1 - y0;
    const T dist  = hypot(dx, dy);
    const T d     = dist / rho;
    const T theta = mod2pi(atan2(dy, dx));
    const T alpha = mod2pi(th0 - theta);
    const T beta  = mod2pi(th1 - theta);

    const Word<T> words[6] = {wordLSL(alpha, beta, d), wordLSR(alpha, beta, d),
                              wordRSL(alpha, beta, d), wordRSR(alpha, beta, d),
                              wordRLR(alpha, beta, d), wordLRL(alpha, beta, d)};

    Result<T> r;
    r.alpha = alpha;
    r.beta  = beta;
    r.d     = d;

    double best = std::numeric_limits<double>::infinity();
    for (int i = 0; i < 6; ++i)
    {
        if (!words[i].valid) continue;
        const T normLen = words[i].t + words[i].p + words[i].q; // unit-radius total
        const double lv = valueOf(normLen);
        if (lv < best)
        {
            best     = lv;
            r.word   = i;
            r.t      = words[i].t;
            r.p      = words[i].p;
            r.q      = words[i].q;
            r.length = normLen * rho; // actual length
        }
    }
    return r;
}

/// Shortest Dubins path length only (drops the word and segment breakdown).
template <class T>
T dubinsLength(T x0, T y0, T th0, T x1, T y1, T th1, T rho)
{
    return dubinsShortest<T>(x0, y0, th0, x1, y1, th1, rho).length;
}

// ============================================================================
// Sampling (double)
// ============================================================================

/// A planar pose: position and heading.
struct Pose
{
    double x;
    double y;
    double th;
};

/**
 * @brief Advance a pose along one segment.
 *
 * @param mag For Seg::Straight, the normalized straight length (actual length
 *            is mag * rho); for Seg::Left / Seg::Right, the turn angle in
 *            radians at radius rho.
 */
inline Pose advance(Pose c, Seg seg, double mag, double rho)
{
    Pose n = c;
    if (seg == Seg::Straight)
    {
        const double len = mag * rho;
        n.x  = c.x + len * std::cos(c.th);
        n.y  = c.y + len * std::sin(c.th);
        n.th = c.th;
    }
    else if (seg == Seg::Left)
    {
        const double phi = mag;
        n.x  = c.x + rho * (std::sin(c.th + phi) - std::sin(c.th));
        n.y  = c.y + rho * (std::cos(c.th) - std::cos(c.th + phi));
        n.th = c.th + phi;
    }
    else // Seg::Right
    {
        const double phi = mag;
        n.x  = c.x + rho * (std::sin(c.th) - std::sin(c.th - phi));
        n.y  = c.y + rho * (std::cos(c.th - phi) - std::cos(c.th));
        n.th = c.th - phi;
    }
    return n;
}

/// Sample the chosen path into waypoints, perSeg points per segment.
inline std::vector<Pose> samplePath(Pose start, const Result<double>& r, double rho, int perSeg)
{
    std::vector<Pose> pts;
    pts.push_back(start);
    Pose c               = start;
    const auto& kinds    = kSegs[static_cast<std::size_t>(r.word)];
    const double mags[3] = {r.t, r.p, r.q};
    for (int s = 0; s < 3; ++s)
    {
        for (int k = 1; k <= perSeg; ++k)
        {
            const double frac = static_cast<double>(k) / perSeg;
            pts.push_back(advance(c, kinds[static_cast<std::size_t>(s)], mags[s] * frac, rho));
        }
        c = advance(c, kinds[static_cast<std::size_t>(s)], mags[s], rho);
    }
    return pts;
}

} // namespace dubins
