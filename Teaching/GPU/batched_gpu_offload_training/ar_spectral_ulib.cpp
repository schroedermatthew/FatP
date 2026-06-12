// ============================================================================
// ar_spectral_ulib.cpp
//
// AR spectral estimation pipeline using the ulib four-layer stack:
//
//   LifetimeToken  — guards the caller's signal buffer in debug builds
//   ArrayView      — wraps raw double* without copying or owning
//   DeviceStaging  — handles host↔device transfers (DeviceInput / DeviceOutput)
//   ScopeGuard     — RAII cleanup of cuSOLVER handle + raw CUDA allocations
//
// Component boundaries:
//
//   ulib         — owns the host↔device boundary (signal in, PSD out)
//   Kokkos::View — owns device-only intermediates (matrices, AR coeffs, etc.)
//   cuSOLVER     — owns the dense linear algebra (batched Cholesky)
//
// The device-only intermediates have no host counterpart — they are born
// and die on the device.  DeviceStaging is not used for them because
// staging implies a host buffer on the other end.  Kokkos::View is the
// right abstraction for device-local scratch.
//
// Build (Kokkos with CUDA backend):
//
//   ${KOKKOS_PATH}/bin/nvcc_wrapper -O2 -arch=sm_70 \
//       -I${KOKKOS_PATH}/include -I${ULIB_PATH}/include \
//       -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
//       -lcusolver \
//       ar_spectral_ulib.cpp -o ar_spectral_ulib
//
// ============================================================================

#include "ArrayView.h"
#include "DeviceStaging.h"
#include "ScopeGuard.h"
#include "LifetimeToken.h"

#include <Kokkos_Core.hpp>
#include <cuda_runtime.h>
#include <cusolverDn.h>

#include <cstdio>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <chrono>

inline constexpr double PI = std::numbers::pi;

// ────────────────────────────────────────────────────────────────────────────
// Configuration
// ────────────────────────────────────────────────────────────────────────────

constexpr int    ORDER  = 32;
constexpr int    WLEN   = 128;
constexpr int    STRIDE = 32;
constexpr int    BATCH  = 65536;
constexpr int    NFREQ  = 256;
constexpr int    N2     = ORDER * ORDER;
constexpr int    SIGLEN = (BATCH - 1) * STRIDE + WLEN;

constexpr double FS       = 10000.0;
constexpr double CHIRP_F0 = 200.0;
constexpr double CHIRP_F1 = 2000.0;
constexpr double TRANS_F  = 1200.0;
constexpr double TRANS_T0 = 105.0;
constexpr double TRANS_SD = 2.0;
constexpr double TRANS_A  = 1.5;
constexpr double NOISE_SD = 0.15;

// ────────────────────────────────────────────────────────────────────────────
// Kokkos type aliases
//
// Window-local 2D arrays use LayoutRight (rightmost index contiguous).
// MDRangePolicy must explicitly request Iterate::Right to match — the
// default iteration order on CUDA aligns with LayoutLeft, not LayoutRight.
// cuSOLVER buffers stay flat 1D with manual column-major indexing.
// ────────────────────────────────────────────────────────────────────────────

using ExecSpace      = Kokkos::DefaultExecutionSpace;
using MemSpace       = ExecSpace::memory_space;
using view_1d        = Kokkos::View<double*, MemSpace>;
using window_view_2d = Kokkos::View<double**, Kokkos::LayoutRight, MemSpace>;

using mdrange_right_2d = Kokkos::MDRangePolicy<
    Kokkos::Rank<2, Kokkos::Iterate::Right, Kokkos::Iterate::Right>>;

using team_policy = Kokkos::TeamPolicy<ExecSpace>;
using team_member = team_policy::member_type;

// ────────────────────────────────────────────────────────────────────────────
// Error macros
// ────────────────────────────────────────────────────────────────────────────

#define CUDA_CHECK(call)                                                      \
    do {                                                                       \
        cudaError_t err = (call);                                              \
        if (err != cudaSuccess)                                                \
            throw std::runtime_error(                                          \
                std::string("CUDA error at ") + __FILE__ + ":"                 \
                + std::to_string(__LINE__) + " — "                            \
                + cudaGetErrorString(err));                                    \
    } while (0)

#define CUSOLVER_CHECK(call)                                                   \
    do {                                                                       \
        cusolverStatus_t st = (call);                                          \
        if (st != CUSOLVER_STATUS_SUCCESS)                                     \
            throw std::runtime_error(                                          \
                std::string("cuSOLVER error at ") + __FILE__ + ":"             \
                + std::to_string(__LINE__)                                     \
                + " — code " + std::to_string((int)st));                      \
    } while (0)

// ────────────────────────────────────────────────────────────────────────────
// Timer
// ────────────────────────────────────────────────────────────────────────────

struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0;
    void start() { t0 = clock::now(); }
    double ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    }
};

// ════════════════════════════════════════════════════════════════════════════
//  Signal generation (same test signal as previous versions)
// ════════════════════════════════════════════════════════════════════════════

static void generate_signal(double* x, int len)
{
    double T = (double)len / FS;
    uint64_t rng = 123456789ULL;
    auto randn = [&]() -> double {
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        double u1 = (double)(rng >> 11) / (double)(1ULL << 53);
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        double u2 = (double)(rng >> 11) / (double)(1ULL << 53);
        u1 = fmax(u1, 1e-15);
        return sqrt(-2.0 * log(u1)) * cos(2.0 * PI * u2);
    };
    for (int n = 0; n < len; ++n) {
        double t = (double)n / FS;
        double chirp = sin(2.0*PI*(CHIRP_F0*t + (CHIRP_F1-CHIRP_F0)*t*t/(2.0*T)));
        double env   = exp(-0.5*(t-TRANS_T0)*(t-TRANS_T0)/(TRANS_SD*TRANS_SD));
        x[n] = chirp + TRANS_A * env * sin(2.0*PI*TRANS_F*t) + NOISE_SD * randn();
    }
}


// ════════════════════════════════════════════════════════════════════════════
//
//  THE PIPELINE
//
//  Signature: accepts raw caller-owned pointers for input and output.
//  ulib wraps them at the boundary; everything inside is Kokkos + cuSOLVER.
//
//  const double* h_signal   — caller's signal buffer  (read-only)
//  int           sig_len    — signal length
//  double*       h_psd_out  — caller's PSD output buffer  [batch × nfreq]
//  int           batch      — number of analysis windows
//  int           nfreq      — number of frequency bins
//
// ════════════════════════════════════════════════════════════════════════════

void ar_spectral_pipeline(const double*         h_signal,
                          int                   sig_len,
                          ulib::LifetimeToken&  sig_token,
                          double*               h_psd_out,
                          int                   batch,
                          int                   nfreq)
{
    Timer t_total;
    t_total.start();

    // ══════════════════════════════════════════════════════════════════
    //  BOUNDARY LAYER — ulib wraps the caller's raw pointers
    // ══════════════════════════════════════════════════════════════════

    // ── Wrap signal in ArrayView with lifetime tracking ───────────────
    //
    //  ArrayView does not own the data.  The LifetimeToken reference
    //  ensures that any access after the caller's buffer dies will
    //  assert in debug builds rather than silently corrupt.

    ulib::ArrayView<const double, ulib::dynamic_extent>
        sig_view(h_signal, sig_token, sig_len);

    // ── Stage signal to device (read-only, one-way H2D) ──────────────
    //
    //  DeviceInput copies host→device on construction.
    //  .view() returns a const-element device view — kernels can read
    //  but not write the signal.  No copyBack path exists.

    Timer t_h2d;
    t_h2d.start();

    auto dev_signal = ulib::staging::makeDeviceInput(sig_view, "signal");

    Kokkos::fence("H2D signal");
    double ms_h2d = t_h2d.ms();
    printf("  H2D signal (DeviceInput): %6.2f ms\n", ms_h2d);

    // ── Allocate PSD output (device-only, will copyTo host at end) ────
    //
    //  DeviceOutput allocates device storage without a host source.
    //  copyTo() at the end writes the results into the caller's buffer.
    //
    //  LAYOUT REQUIREMENT: the PSD kernel uses mdrange_right_2d over
    //  (BATCH, NFREQ).  Because the policy explicitly requests
    //  Iterate::Right, the rightmost rank (frequency bin j) is the
    //  fastest-varying rank.  dev_psd.view()(w, j) must be contiguous
    //  in j — i.e. LayoutRight.  If makeDeviceOutput returns a LayoutLeft
    //  view, the PSD writes will be fully uncoalesced (stride = batch).
    //
    //  The static_assert below fires at compile time if the layout
    //  is wrong.

    auto dev_psd = ulib::staging::makeDeviceOutput<
        double, ulib::dynamic_extent, ulib::dynamic_extent>(
            "psd", batch, nfreq);

    // ══════════════════════════════════════════════════════════════════
    //  DEVICE-ONLY INTERMEDIATES — Kokkos::View, no host counterpart
    //
    //  These buffers are born and die on the device.  They have no
    //  host-side C array to stage from/to, so DeviceStaging is not
    //  the right abstraction.  Kokkos::View manages their lifecycle.
    // ══════════════════════════════════════════════════════════════════

    window_view_2d  d_xw     ("windowed",  batch, WLEN);
    view_1d  d_A      ("matrices",  (size_t)batch * N2);
    view_1d  d_B      ("rhs",       (size_t)batch * ORDER);
    view_1d  d_B_orig ("rhs_orig",  (size_t)batch * ORDER);
    view_1d  d_r0     ("r0",        batch);
    view_1d  d_sigma2 ("sigma2",    batch);

    // ══════════════════════════════════════════════════════════════════
    //  cuSOLVER RESOURCES — ScopeGuard for RAII cleanup
    //
    //  The pointer arrays and info buffer are cuSOLVER infrastructure.
    //  ScopeGuard ensures cleanup on any exit path — normal or exception.
    // ══════════════════════════════════════════════════════════════════

    cusolverDnHandle_t solver;
    CUSOLVER_CHECK(cusolverDnCreate(&solver));
    ULIB_SCOPE_EXIT { cusolverDnDestroy(solver); };

    double **d_Aptr = nullptr, **d_Bptr = nullptr;
    int     *d_info = nullptr;

    CUDA_CHECK(cudaMalloc(&d_Aptr, batch * sizeof(double*)));
    CUDA_CHECK(cudaMalloc(&d_Bptr, batch * sizeof(double*)));
    CUDA_CHECK(cudaMalloc(&d_info, batch * sizeof(int)));

    ULIB_SCOPE_EXIT { cudaFree(d_Aptr); };
    ULIB_SCOPE_EXIT { cudaFree(d_Bptr); };
    ULIB_SCOPE_EXIT { cudaFree(d_info); };

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 2 — Hann windowing
    //
    //  dev_signal.view() returns a const device view.
    //  The lambda captures it by value (Kokkos view semantics: shallow).
    // ══════════════════════════════════════════════════════════════════

    Timer t_hann;
    t_hann.start();

    auto d_sig_view = dev_signal.view();    // const device view

    Kokkos::parallel_for("hann_window",
        mdrange_right_2d({0, 0}, {batch, WLEN}),
        KOKKOS_LAMBDA(const int w, const int n)
    {
        const double hann = 0.5 * (1.0 - Kokkos::cos(
            2.0 * PI * static_cast<double>(n)
                       / static_cast<double>(WLEN - 1)));
        d_xw(w, n) = d_sig_view(w * STRIDE + n) * hann;
    });

    Kokkos::fence("hann");
    printf("  Hann windowing:           %6.2f ms\n", t_hann.ms());

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 3 — Autocovariance + Toeplitz assembly
    //
    //  TeamPolicy: one team per window, ORDER+1 threads.
    //  Team scratch holds the autocovariance lags.
    // ══════════════════════════════════════════════════════════════════

    Timer t_acov;
    t_acov.start();

    const int scratch_bytes = (ORDER + 1) * sizeof(double);

    Kokkos::parallel_for("autocov_assemble",
        team_policy(batch, ORDER + 1)
            .set_scratch_size(0, Kokkos::PerTeam(scratch_bytes)),
        KOKKOS_LAMBDA(const team_member& team)
    {
        const int w   = team.league_rank();
        const int tid = team.team_rank();

        Kokkos::View<double*, ExecSpace::scratch_memory_space,
                     Kokkos::MemoryUnmanaged>
            s_r(team.team_scratch(0), ORDER + 1);

        // Autocovariance lag `tid`
        if (tid <= ORDER) {
            double rk = 0.0;
            for (int n = tid; n < WLEN; ++n)
                rk += d_xw(w, n) * d_xw(w, n - tid);
            rk /= static_cast<double>(WLEN);
            s_r(tid) = rk;
        }
        team.team_barrier();

        if (tid == 0)
            d_r0(w) = s_r(0);

        // Toeplitz matrix row + RHS
        // Diagonal loading: ε·I for strict positive definiteness
        if (tid < ORDER) {
            const size_t base = static_cast<size_t>(w) * N2;
            for (int col = 0; col < ORDER; ++col) {
                int lag = (tid >= col) ? (tid - col) : (col - tid);
                double val = s_r(lag);
                if (tid == col)
                    val += 1.0e-10 * s_r(0) + 1.0e-14;
                d_A(base + tid + col * ORDER) = val;
            }
            d_B(static_cast<size_t>(w) * ORDER + tid) = s_r(tid + 1);
        }
    });

    Kokkos::fence("autocov");
    printf("  Autocov + assembly:       %6.2f ms\n", t_acov.ms());

    // ── Save RHS before cuSOLVER overwrites it ────────────────────────
    Kokkos::deep_copy(d_B_orig, d_B);
    Kokkos::fence("rhs saved");

    // ── Build cuSOLVER pointer arrays on device ───────────────────────

    {
        double* raw_A = d_A.data();
        double* raw_B = d_B.data();

        Kokkos::parallel_for("build_ptrs",
            Kokkos::RangePolicy<ExecSpace>(0, batch),
            KOKKOS_LAMBDA(const int k)
        {
            d_Aptr[k] = raw_A + static_cast<size_t>(k) * N2;
            d_Bptr[k] = raw_B + static_cast<size_t>(k) * ORDER;
        });
        Kokkos::fence("ptrs");
    }

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 4 — Batched Cholesky (cuSOLVER)
    //
    //  Kokkos::fence() before entry, cudaDeviceSynchronize() after.
    //  The ScopeGuard on the solver handle ensures cleanup.
    // ══════════════════════════════════════════════════════════════════

    Timer t_fact;
    t_fact.start();

    CUSOLVER_CHECK(cusolverDnDpotrfBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, d_Aptr, ORDER, d_info, batch));
    cudaDeviceSynchronize();

    printf("  Cholesky factorise:       %6.2f ms\n", t_fact.ms());

    // Check factorisations — diagonal loading guarantees SPD.  Failure is fatal.
    {
        std::vector<int> h_info(batch);
        CUDA_CHECK(cudaMemcpy(h_info.data(), d_info,
                              batch * sizeof(int), cudaMemcpyDeviceToHost));
        int bad = 0;
        for (int k = 0; k < batch; ++k)
            if (h_info[k] != 0) ++bad;
        if (bad > 0)
            throw std::runtime_error(
                "Cholesky factorisation failed: "
                + std::to_string(bad) + " / " + std::to_string(batch));
    }

    Timer t_solve;
    t_solve.start();

    CUSOLVER_CHECK(cusolverDnDpotrsBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, 1, d_Aptr, ORDER, d_Bptr, ORDER, d_info, batch));
    cudaDeviceSynchronize();

    printf("  Triangular solve:         %6.2f ms\n", t_solve.ms());

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 4c — Innovation variance
    // ══════════════════════════════════════════════════════════════════

    Timer t_sig2;
    t_sig2.start();

    Kokkos::parallel_for("sigma2",
        Kokkos::RangePolicy<ExecSpace>(0, batch),
        KOKKOS_LAMBDA(const int w)
    {
        const size_t off = static_cast<size_t>(w) * ORDER;
        double dot = 0.0;
        for (int k = 0; k < ORDER; ++k)
            dot += d_B(off + k) * d_B_orig(off + k);
        d_sigma2(w) = Kokkos::fmax(d_r0(w) - dot, 1.0e-30);
    });

    Kokkos::fence("sigma2");
    printf("  Innovation variance:      %6.2f ms\n", t_sig2.ms());

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 5 — PSD evaluation → DeviceOutput
    //
    //  Writes into dev_psd.view(), which is the DeviceOutput's
    //  owning device view.  copyTo() at the end transfers results.
    // ══════════════════════════════════════════════════════════════════

    Timer t_psd;
    t_psd.start();

    auto d_psd_view = dev_psd.view();

    // Verify DeviceOutput provides LayoutRight for coalesced (w,j) writes.
    // If makeDeviceOutput defaults to LayoutLeft, this fires at compile time.
    static_assert(
        std::is_same_v<
            typename decltype(d_psd_view)::array_layout,
            Kokkos::LayoutRight>,
        "PSD output view must be LayoutRight for coalesced (w,j) writes");

    Kokkos::parallel_for("psd_eval",
        mdrange_right_2d({0, 0}, {batch, nfreq}),
        KOKKOS_LAMBDA(const int w, const int j)
    {
        const double fj = static_cast<double>(j)
                        * (FS * 0.5)
                        / static_cast<double>(nfreq - 1);

        const size_t off = static_cast<size_t>(w) * ORDER;

        double re_A = 1.0;
        double im_A = 0.0;
        for (int k = 0; k < ORDER; ++k) {
            const double theta = -2.0 * PI * fj
                               * static_cast<double>(k + 1) / FS;
            re_A -= d_B(off + k) * Kokkos::cos(theta);
            im_A -= d_B(off + k) * Kokkos::sin(theta);
        }

        const double mag2 = re_A * re_A + im_A * im_A;
        const double P    = d_sigma2(w) / Kokkos::fmax(mag2, 1.0e-30);

        d_psd_view(w, j) = 10.0 * Kokkos::log10(Kokkos::fmax(P, 1.0e-30));
    });

    Kokkos::fence("psd");
    printf("  PSD evaluation:           %6.2f ms\n", t_psd.ms());

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 6 — D2H via DeviceOutput::copyTo
    //
    //  Wrap the caller's output buffer in an ArrayView, then copyTo.
    //  DeviceOutput handles the transfer and any layout repack needed.
    // ══════════════════════════════════════════════════════════════════

    Timer t_d2h;
    t_d2h.start();

    ulib::ArrayView<double, ulib::dynamic_extent, ulib::dynamic_extent>
        psd_out_view(h_psd_out, batch, nfreq);

    dev_psd.copyTo(psd_out_view);
    Kokkos::fence("D2H psd");

    printf("  D2H PSD (DeviceOutput):   %6.2f ms\n", t_d2h.ms());

    double ms_total = t_total.ms();
    printf("  ────────────────────────────────\n");
    printf("  TOTAL:                    %6.2f ms\n", ms_total);
    printf("  (%.1f s of signal in %.1f ms = %.0f× real-time)\n",
           (double)sig_len / FS, ms_total, (double)sig_len / FS * 1000.0 / ms_total);

    // cuSOLVER handle + cudaMalloc'd pointers cleaned up by ULIB_SCOPE_EXIT
}


// ════════════════════════════════════════════════════════════════════════════
//                                MAIN
// ════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[])
{
    Kokkos::initialize(argc, argv);
    {
        printf("══════════════════════════════════════════════════════════════\n");
        printf("  AR(%d) Spectral Estimation — ulib + Kokkos + cuSOLVER\n", ORDER);
        printf("  %d windows × %d samples, stride %d, %d freq bins\n",
               BATCH, WLEN, STRIDE, NFREQ);
        printf("══════════════════════════════════════════════════════════════\n\n");

        // ── The caller's data ─────────────────────────────────────────
        //
        //  In production this is whatever buffer the acquisition system
        //  or DAQ driver hands you.  The pipeline function accepts the
        //  raw pointer — it does not care how it was allocated.
        //
        //  The LifetimeToken is co-scoped with the buffer.  If the
        //  buffer dies before the token, debug builds will catch it.

        std::vector<double> signal_buf(SIGLEN);
        ulib::LifetimeToken signal_token;           // co-scoped with signal_buf

        printf("  Generating test signal ...\n");
        generate_signal(signal_buf.data(), SIGLEN);

        // ── Caller's output buffer ────────────────────────────────────
        //
        //  The pipeline writes the PSD matrix here via DeviceOutput::copyTo.
        //  Row-major: psd_buf[w * NFREQ + j] = PSD at window w, freq bin j.

        std::vector<double> psd_buf((size_t)BATCH * NFREQ);

        // ── Run the pipeline ──────────────────────────────────────────

        printf("\n── Pipeline ────────────────────────────────────────────────\n");

        ar_spectral_pipeline(
            signal_buf.data(),          // const double*
            SIGLEN,
            signal_token,               // lifetime guard
            psd_buf.data(),             // double* output
            BATCH,
            NFREQ);

        // ── Verification ──────────────────────────────────────────────

        printf("\n── Chirp tracking ──────────────────────────────────────────\n");

        double T = (double)SIGLEN / FS;
        double err_sum = 0.0, err_max = 0.0;
        int n_tracked = 0;

        printf("\n  %8s  %8s  %8s  %8s\n",
               "Window", "Time(s)", "True(Hz)", "Est(Hz)");

        int spots[] = {0, 5000, 10000, 20000, 32768, 50000, 60000, 65535};

        for (int w = 0; w < BATCH; ++w) {
            double tc = ((double)(w * STRIDE) + WLEN * 0.5) / FS;
            double f_true = CHIRP_F0 + (CHIRP_F1 - CHIRP_F0) * tc / T;

            int peak_bin = 0;
            double peak_val = -1e30;
            for (int j = 1; j < NFREQ; ++j) {
                double v = psd_buf[(size_t)w * NFREQ + j];
                if (v > peak_val) { peak_val = v; peak_bin = j; }
            }
            double f_est = (double)peak_bin * (FS * 0.5) / (double)(NFREQ - 1);

            double dt = fabs(tc - TRANS_T0);
            if (dt > 3.0 * TRANS_SD) {
                err_sum += fabs(f_est - f_true);
                err_max = fmax(err_max, fabs(f_est - f_true));
                ++n_tracked;
            }

            for (int s = 0; s < 8; ++s) {
                if (w == spots[s]) {
                    printf("  %8d  %8.2f  %8.1f  %8.1f%s\n",
                           w, tc, f_true, f_est,
                           (dt <= 3.0*TRANS_SD) ? "  [transient]" : "");
                    break;
                }
            }
        }

        printf("\n  Mean tracking error: %.1f Hz\n",
               err_sum / fmax(n_tracked, 1));
        printf("  Max  tracking error: %.1f Hz\n", err_max);

        printf("\n══════════════════════════════════════════════════════════════\n");
        printf("  ulib components used:\n");
        printf("    LifetimeToken  — guards signal buffer (debug builds)\n");
        printf("    ArrayView      — wraps raw double* at pipeline boundary\n");
        printf("    DeviceInput    — one-way H2D staging (signal)\n");
        printf("    DeviceOutput   — device alloc + copyTo (PSD result)\n");
        printf("    ULIB_SCOPE_EXIT — RAII cleanup of cuSOLVER + cudaMalloc\n");
        printf("══════════════════════════════════════════════════════════════\n");
    }
    Kokkos::finalize();
    return 0;
}
