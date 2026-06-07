/**
 * @file dubins_chain_jet.cpp
 * @brief Multi-segment Dubins tour: total length and its exact gradient,
 *        assembled per segment, in the shape a gradient-based solver consumes.
 *
 * A tour visits a fixed start, a sequence of interior waypoints, and a fixed
 * goal; the cost is the sum of single-segment Dubins shortest-path lengths over
 * consecutive poses. The interior waypoint coordinates are the decision
 * variables. Each segment length is differentiated with respect to its own six
 * endpoint coordinates by one Jet<6> evaluation; the resulting 1x6 block is
 * scattered into the global gradient, so an interior waypoint accumulates the
 * contributions of the two segments that meet there. This is the per-waypoint
 * Jacobian assembly a solver such as SNOPT performs: the segment-length
 * Jacobian J is block-bidiagonal, and the objective gradient is its column sum.
 *
 * The assembled gradient is checked against a single monolithic Jet over all
 * decision variables (exact-to-exact) and against central finite differences. A
 * backtracking gradient descent then confirms the gradient is a descent
 * direction by driving the tour length toward the straight-line lower bound.
 *
 * @see dubins_geometry.h, fat_p/Jet.h, dubins_jet.cpp
 */
/*
FATP_META:
  meta_version: 1
  component: Jet
  file_role: example
  path: components/Jet/examples/dubins_chain_jet.cpp
  namespace: dubins_chain
  layer: Examples
  summary: Multi-segment Dubins tour with per-segment Jacobian assembly via Jet, in solver-ready form.
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
#include <cstddef>
#include <cstdio>
#include <vector>

#include "dubins_geometry.h"

namespace dubins_chain
{

using dubins::dubinsLength;
using dubins::dubinsShortest;
using dubins::kPi;
using dubins::kWordNames;
using dubins::Pose;
using fat_p::autodiff::Jet;

// ============================================================================
// Tour: fixed endpoints + interior waypoints (the decision variables)
// ============================================================================

/**
 * @brief A Dubins tour with fixed endpoints and free interior waypoints.
 *
 * Decision-vector layout: interior waypoint k (k = 0 .. interior.size()-1)
 * occupies flat columns 3k, 3k+1, 3k+2 = (x, y, theta). The fixed start and
 * goal contribute no columns.
 */
struct Tour
{
    Pose start{};               // fixed
    std::vector<Pose> interior; // decision variables
    Pose goal{};                // fixed
    double rho = 1.0;

    std::size_t numSegments() const { return interior.size() + 1; }
    std::size_t numVars() const { return 3 * interior.size(); }

    /// Pose at chain index i in [0, numSegments()].
    Pose poseAt(std::size_t i) const
    {
        if (i == 0) return start;
        if (i == interior.size() + 1) return goal;
        return interior[i - 1];
    }

    /// If chain index i is an interior (decision) pose, set its column base.
    bool interiorColumns(std::size_t i, std::size_t& colBase) const
    {
        if (i == 0 || i == interior.size() + 1) return false;
        colBase = 3 * (i - 1);
        return true;
    }
};

// ============================================================================
// Objective (double)
// ============================================================================

/// Total tour length: the sum of single-segment Dubins lengths.
inline double totalLength(const Tour& t)
{
    double total = 0.0;
    for (std::size_t i = 0; i < t.numSegments(); ++i)
    {
        const Pose a = t.poseAt(i);
        const Pose b = t.poseAt(i + 1);
        total += dubinsLength<double>(a.x, a.y, a.th, b.x, b.y, b.th, t.rho);
    }
    return total;
}

/// Total tour length with the interior waypoints taken from a flat vector.
inline double totalLengthAt(const Tour& base, const std::vector<double>& z)
{
    Tour t = base;
    for (std::size_t k = 0; k < t.interior.size(); ++k)
        t.interior[k] = {z[3 * k], z[3 * k + 1], z[3 * k + 2]};
    return totalLength(t);
}

// ============================================================================
// Per-segment gradient assembly (the solver-shaped quantity)
// ============================================================================

/// Result of assembling the tour gradient from per-segment Jet blocks.
struct Assembled
{
    double length = 0.0;                       // total tour length
    std::vector<int> segWord;                  // chosen word index per segment
    std::vector<double> segLength;             // each segment length
    std::vector<std::array<double, 6>> segGrad; // d(seg length)/d(its 6 endpoint coords)
    std::vector<std::vector<double>> jac;      // segment-length Jacobian, M x numVars
    std::vector<double> grad;                  // objective gradient, numVars
};

/**
 * @brief Differentiate each segment over its six endpoint coordinates with one
 *        Jet<6>, then scatter the blocks into the global Jacobian and gradient.
 *
 * Segment i contributes its first three partials to the columns of pose i and
 * its last three to the columns of pose i+1, but only where those poses are
 * interior (decision) poses; contributions to the fixed endpoints are dropped.
 * The objective gradient is the column sum of the segment-length Jacobian.
 */
inline Assembled assemble(const Tour& t)
{
    using Jet6 = Jet<6>;
    const std::size_t numSeg = t.numSegments();
    const std::size_t numVar = t.numVars();

    Assembled out;
    out.jac.assign(numSeg, std::vector<double>(numVar, 0.0));
    out.grad.assign(numVar, 0.0);

    for (std::size_t i = 0; i < numSeg; ++i)
    {
        const Pose a = t.poseAt(i);
        const Pose b = t.poseAt(i + 1);

        // Seed the six endpoint coordinates in directions 0..5.
        const Jet6 ax  = Jet6::seed(a.x, 0);
        const Jet6 ay  = Jet6::seed(a.y, 1);
        const Jet6 ath = Jet6::seed(a.th, 2);
        const Jet6 bx  = Jet6::seed(b.x, 3);
        const Jet6 by  = Jet6::seed(b.y, 4);
        const Jet6 bth = Jet6::seed(b.th, 5);

        const auto seg   = dubinsShortest<Jet6>(ax, ay, ath, bx, by, bth, Jet6(t.rho));
        const Jet6 len   = seg.length;

        out.length += len.mValue;
        out.segWord.push_back(seg.word);
        out.segLength.push_back(len.mValue);

        std::array<double, 6> block{};
        for (int d = 0; d < 6; ++d) block[static_cast<std::size_t>(d)] = len.mPartials[static_cast<std::size_t>(d)];
        out.segGrad.push_back(block);

        // Scatter: endpoint a -> partials 0..2, endpoint b -> partials 3..5.
        std::size_t base = 0;
        if (t.interiorColumns(i, base))
            for (std::size_t d = 0; d < 3; ++d) out.jac[i][base + d] += block[d];
        if (t.interiorColumns(i + 1, base))
            for (std::size_t d = 0; d < 3; ++d) out.jac[i][base + d] += block[3 + d];
    }

    for (std::size_t i = 0; i < numSeg; ++i)
        for (std::size_t j = 0; j < numVar; ++j) out.grad[j] += out.jac[i][j];

    return out;
}

// ============================================================================
// Cross-checks
// ============================================================================

/// Monolithic gradient: one Jet over all nine decision variables at once.
inline std::array<double, 9> monolithicGradient9(const Tour& t)
{
    using Jet9 = Jet<9>;
    auto poseAsJet = [&](std::size_t chainIdx) -> std::array<Jet9, 3> {
        const Pose p = t.poseAt(chainIdx);
        std::size_t base = 0;
        if (t.interiorColumns(chainIdx, base))
            return {Jet9::seed(p.x, base + 0), Jet9::seed(p.y, base + 1), Jet9::seed(p.th, base + 2)};
        return {Jet9(p.x), Jet9(p.y), Jet9(p.th)};
    };

    Jet9 total(0.0);
    for (std::size_t i = 0; i < t.numSegments(); ++i)
    {
        const auto a = poseAsJet(i);
        const auto b = poseAsJet(i + 1);
        total += dubinsLength<Jet9>(a[0], a[1], a[2], b[0], b[1], b[2], Jet9(t.rho));
    }

    std::array<double, 9> g{};
    for (std::size_t d = 0; d < 9; ++d) g[d] = total.mPartials[d];
    return g;
}

/// Central finite-difference gradient of the total length.
inline std::vector<double> fdGradient(const Tour& t, double h)
{
    const std::size_t numVar = t.numVars();
    std::vector<double> z(numVar, 0.0);
    for (std::size_t k = 0; k < t.interior.size(); ++k)
    {
        z[3 * k]     = t.interior[k].x;
        z[3 * k + 1] = t.interior[k].y;
        z[3 * k + 2] = t.interior[k].th;
    }
    std::vector<double> g(numVar, 0.0);
    for (std::size_t j = 0; j < numVar; ++j)
    {
        std::vector<double> zp = z;
        std::vector<double> zm = z;
        zp[j] += h;
        zm[j] -= h;
        g[j] = (totalLengthAt(t, zp) - totalLengthAt(t, zm)) / (2.0 * h);
    }
    return g;
}

} // namespace dubins_chain

// ============================================================================
// Demonstration
// ============================================================================

int main()
{
    using namespace dubins_chain;

    Tour tour;
    tour.rho      = 1.0;
    tour.start    = {0.0, 0.0, 0.0};
    tour.interior = {{2.5, 1.5, kPi / 4.0}, {5.0, -1.0, -kPi / 6.0}, {7.5, 1.0, kPi / 3.0}};
    tour.goal     = {10.0, 0.0, 0.0};

    const std::size_t numSeg = tour.numSegments();
    const std::size_t numVar = tour.numVars();

    std::printf("=== Dubins tour ===\n");
    std::printf("start (%.1f, %.1f, %.3f)   goal (%.1f, %.1f, %.3f)   rho=%.1f\n", tour.start.x,
                tour.start.y, tour.start.th, tour.goal.x, tour.goal.y, tour.goal.th, tour.rho);
    std::printf("%zu interior waypoints -> %zu segments, %zu decision variables\n",
                tour.interior.size(), numSeg, numVar);

    const Assembled a = assemble(tour);

    std::printf("\n=== per-segment length and its gradient over the 6 endpoint coords ===\n");
    std::printf("seg  word   length     d/dxa     d/dya     d/dtha    d/dxb     d/dyb     d/dthb\n");
    for (std::size_t i = 0; i < numSeg; ++i)
    {
        std::printf(" %zu   %s   %7.4f", i, kWordNames[a.segWord[i]], a.segLength[i]);
        for (int d = 0; d < 6; ++d) std::printf("  %+8.4f", a.segGrad[i][static_cast<std::size_t>(d)]);
        std::printf("\n");
    }
    std::printf("total length = %.6f\n", a.length);

    std::printf("\n=== segment-length Jacobian sparsity (block-bidiagonal) ===\n");
    std::printf("        ");
    for (std::size_t k = 0; k < tour.interior.size(); ++k) std::printf("   q%-2zu      ", k + 1);
    std::printf("\n");
    for (std::size_t i = 0; i < numSeg; ++i)
    {
        std::printf("  seg %zu ", i);
        for (std::size_t j = 0; j < numVar; ++j)
            std::printf(" %s", (a.jac[i][j] != 0.0) ? "X" : ".");
        std::printf("\n");
    }

    std::printf("\n=== assembled objective gradient (column sum of J) ===\n");
    for (std::size_t k = 0; k < tour.interior.size(); ++k)
        std::printf("  q%zu : dL/dx=%+.6f  dL/dy=%+.6f  dL/dth=%+.6f\n", k + 1, a.grad[3 * k],
                    a.grad[3 * k + 1], a.grad[3 * k + 2]);

    // Cross-check 1: one monolithic Jet over all decision variables.
    const std::array<double, 9> gMono = monolithicGradient9(tour);
    double maxMono = 0.0;
    for (std::size_t j = 0; j < numVar; ++j)
        maxMono = std::max(maxMono, std::fabs(a.grad[j] - gMono[j]));

    // Cross-check 2: central finite differences.
    const double h                 = 1e-6;
    const std::vector<double> gFd  = fdGradient(tour, h);
    double maxFd                   = 0.0;
    for (std::size_t j = 0; j < numVar; ++j) maxFd = std::max(maxFd, std::fabs(a.grad[j] - gFd[j]));

    std::printf("\n=== gradient agreement ===\n");
    std::printf("assembled (Jet<6> blocks) vs monolithic Jet<9> : max |diff| = %.2e\n", maxMono);
    std::printf("assembled vs central finite difference (h=%.0e): max |diff| = %.2e\n", h, maxFd);

    // Smoke test: the assembled gradient is a descent direction.
    {
        Tour td = tour;
        std::vector<double> z(numVar, 0.0);
        auto loadZ = [&]() {
            for (std::size_t k = 0; k < td.interior.size(); ++k)
            {
                z[3 * k]     = td.interior[k].x;
                z[3 * k + 1] = td.interior[k].y;
                z[3 * k + 2] = td.interior[k].th;
            }
        };
        auto setZ = [&](const std::vector<double>& zz) {
            for (std::size_t k = 0; k < td.interior.size(); ++k)
                td.interior[k] = {zz[3 * k], zz[3 * k + 1], zz[3 * k + 2]};
        };
        loadZ();
        const double l0 = totalLength(td);
        double length   = l0;
        for (int it = 0; it < 300; ++it)
        {
            const Assembled ai = assemble(td);
            double gn2         = 0.0;
            for (double v : ai.grad) gn2 += v * v;
            if (gn2 < 1e-18) break;

            double step = 1.0;
            const double c = 1e-4;
            bool stepped   = false;
            std::vector<double> zNew(numVar, 0.0);
            for (int bt = 0; bt < 50; ++bt)
            {
                for (std::size_t j = 0; j < numVar; ++j) zNew[j] = z[j] - step * ai.grad[j];
                const double ln = totalLengthAt(td, zNew);
                if (ln <= length - c * step * gn2)
                {
                    z = zNew;
                    setZ(z);
                    length  = ln;
                    stepped = true;
                    break;
                }
                step *= 0.5;
            }
            if (!stepped) break;
        }
        const double bound = std::hypot(tour.goal.x - tour.start.x, tour.goal.y - tour.start.y);
        std::printf("\n=== backtracking descent on the assembled gradient ===\n");
        std::printf("length %.6f -> %.6f   (straight-line lower bound = %.6f)\n", l0, length, bound);
    }

    return 0;
}
