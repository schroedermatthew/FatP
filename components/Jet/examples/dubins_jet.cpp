/**
 * @file dubins_jet.cpp
 * @brief Single-segment Dubins shortest-path example over the Jet AD scalar.
 *
 * Computes the shortest Dubins path between two poses with T = double, then the
 * length and its exact gradient with respect to the goal pose and the turning
 * radius with T = Jet<4>, cross-checked against central finite differences. The
 * path geometry lives in dubins_geometry.h; this file is the demonstration.
 *
 * @see dubins_geometry.h, fat_p/Jet.h
 * @see dubins_chain_jet.cpp for the multi-segment tour with assembled gradients.
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: example
  path: components/Jet/examples/dubins_jet.cpp
  namespace: dubins
  layer: Examples
  summary: Single-segment Dubins shortest-path demonstration with exact gradients via Jet.
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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

#include "dubins_geometry.h"

// ============================================================================
// Demonstration
// ============================================================================

int main()
{
    using namespace dubins;

    // Sanity: goal straight ahead, headings aligned -> pure straight.
    {
        const Result<double> r = dubinsShortest<double>(0, 0, 0, 4, 0, 0, 1.0);
        std::printf("sanity  start(0,0,0) goal(4,0,0) rho=1 : word=%s length=%.6f (expect 4.000000)\n",
                    r.word >= 0 ? kWordNames[r.word] : "NONE", r.length);
    }

    const double x0 = 0.0, y0 = 0.0, th0 = 0.0;
    const double x1 = 3.0, y1 = 3.0, th1 = kPi / 2.0; // goal pose
    const double rho = 1.0;

    const Result<double> r = dubinsShortest<double>(x0, y0, th0, x1, y1, th1, rho);
    std::printf("\n=== Dubins shortest path ===\n");
    std::printf("start (%.2f, %.2f, %.2f rad)   goal (%.2f, %.2f, %.2f rad)   rho=%.2f\n",
                x0, y0, th0, x1, y1, th1, rho);
    std::printf("normalized: d=%.4f  alpha=%.4f  beta=%.4f\n", r.d, r.alpha, r.beta);
    std::printf("word = %s   segments (t,p,q) = (%.4f, %.4f, %.4f)\n", kWordNames[r.word], r.t, r.p, r.q);
    std::printf("total length = %.6f\n", r.length);

    // Sampled waypoints and endpoint check.
    const std::vector<Pose> pts = samplePath({x0, y0, th0}, r, rho, 16);
    std::printf("\nsampled path (%zu pts), every 6th:\n", pts.size());
    for (std::size_t i = 0; i < pts.size(); i += 6)
        std::printf("  s%2zu: (%7.4f, %7.4f)  th=%7.4f\n", i, pts[i].x, pts[i].y, pts[i].th);
    const Pose end = pts.back();
    double dth     = std::fabs(mod2pi(end.th - th1));
    if (dth > kPi) dth = kTwoPi - dth;
    std::printf("  endpoint (%.6f, %.6f, %.6f)  vs goal -> dpos=%.2e dth=%.2e\n", end.x, end.y, end.th,
                std::hypot(end.x - x1, end.y - y1), dth);

    // Exact gradient of length with respect to (x1, y1, th1, rho) via Jet<4>.
    using Jet4         = Jet<4>;
    const Jet4 jx1     = Jet4::seed(x1, 0);
    const Jet4 jy1     = Jet4::seed(y1, 1);
    const Jet4 jth1    = Jet4::seed(th1, 2);
    const Jet4 jrho    = Jet4::seed(rho, 3);
    const Result<Jet4> rj =
        dubinsShortest<Jet4>(Jet4(x0), Jet4(y0), Jet4(th0), jx1, jy1, jth1, jrho);

    std::printf("\n=== exact gradient of length (Jet<4>, one pass) ===\n");
    std::printf("active word matches the double path: %s\n", (rj.word == r.word) ? "yes" : "NO");
    std::printf("dL/dx1   = %+.8f\n", rj.length.mPartials[0]);
    std::printf("dL/dy1   = %+.8f\n", rj.length.mPartials[1]);
    std::printf("dL/dth1  = %+.8f\n", rj.length.mPartials[2]);
    std::printf("dL/drho  = %+.8f\n", rj.length.mPartials[3]);

    // Finite-difference cross-check.
    auto lengthAt = [&](double a, double b, double c, double e) {
        return dubinsShortest<double>(x0, y0, th0, a, b, c, e).length;
    };
    const double h      = 1e-6;
    const double gFd[4] = {
        (lengthAt(x1 + h, y1, th1, rho) - lengthAt(x1 - h, y1, th1, rho)) / (2 * h),
        (lengthAt(x1, y1 + h, th1, rho) - lengthAt(x1, y1 - h, th1, rho)) / (2 * h),
        (lengthAt(x1, y1, th1 + h, rho) - lengthAt(x1, y1, th1 - h, rho)) / (2 * h),
        (lengthAt(x1, y1, th1, rho + h) - lengthAt(x1, y1, th1, rho - h)) / (2 * h),
    };
    const double gAd[4] = {rj.length.mPartials[0], rj.length.mPartials[1], rj.length.mPartials[2],
                           rj.length.mPartials[3]};
    std::printf("\n=== AD vs central finite difference (step h=%.0e) ===\n", h);
    const char* names[4] = {"x1", "y1", "th1", "rho"};
    double maxErr        = 0.0;
    for (int i = 0; i < 4; ++i)
    {
        const double e = std::fabs(gAd[i] - gFd[i]);
        maxErr         = std::max(maxErr, e);
        std::printf("  d/d%-3s  AD=%+.8f  FD=%+.8f  |diff|=%.2e\n", names[i], gAd[i], gFd[i], e);
    }
    std::printf("max |AD-FD| = %.2e\n", maxErr);
    return 0;
}
