// ============================================================================
// ar_spectral_kokkos.cpp
//
// Kokkos port of the AR spectral estimation pipeline.
// Input: const double* (host pointer to raw signal array).
//
// Kokkos idioms demonstrated:
//   • Unmanaged View wrapping a raw host pointer (the input interface)
//   • deep_copy for H2D and D2H
//   • mdrange_right_2d (explicit Iterate::Right) for 2D parallel maps
//   • TeamPolicy with team scratch memory for shared-memory reductions
//   • Raw pointer extraction (.data()) for cuSOLVER library interop
//   • Kokkos::fence() at the Kokkos↔cuSOLVER boundary
//
// cuSOLVER calls remain raw CUDA — Kokkos does not wrap vendor dense
// linear algebra.  The pattern is:
//   Kokkos kernel → fence → extract .data() → cuSOLVER → fence → Kokkos kernel
//
// Build (Kokkos with CUDA backend):
//
//   # CMake (preferred):
//   cmake -DKokkos_ROOT=/path/to/kokkos/install \
//         -DCMAKE_CXX_COMPILER=nvcc_wrapper \
//         -DCMAKE_BUILD_TYPE=Release ..
//
//   # Manual:
//   ${KOKKOS_PATH}/bin/nvcc_wrapper -O2 -arch=sm_70 \
//       -I${KOKKOS_PATH}/include \
//       -L${KOKKOS_PATH}/lib -lkokkoscore -lkokkoscontainers \
//       -lcusolver \
//       ar_spectral_kokkos.cpp -o ar_spectral_kokkos
//
// ============================================================================

#include <Kokkos_Core.hpp>
#include <cuda_runtime.h>
#include <cusolverDn.h>

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>
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

constexpr double RBF_GAMMA = 0.5;   // not used in AR version, kept for parity

// ────────────────────────────────────────────────────────────────────────────
// Type aliases — the Kokkos vocabulary
//
// Layout policy follows the access pattern, not a blanket default:
//
//   Window-local 2D arrays (window, local_index) → LayoutRight
//     With explicit Iterate::Right, adjacent GPU threads vary the
//     rightmost rank (local_index).  LayoutRight makes that index
//     contiguous → coalesced access.  (CUDA default is Iterate::Left,
//     which would mismatch LayoutRight — see mdrange_right_2d below.)
//     TeamPolicy kernels also benefit: threads within one team
//     access different local indices for the same window.
//
//   cuSOLVER matrix/RHS buffers → flat 1D (view_1d)
//     Each batch item is a contiguous block, column-major inside.
//     Manual indexing: base + row + col * ORDER.  This is what
//     cuSOLVER expects and should not be wrapped in a 2D view.
// ────────────────────────────────────────────────────────────────────────────

using ExecSpace   = Kokkos::DefaultExecutionSpace;           // Cuda
using MemSpace    = ExecSpace::memory_space;                 // CudaSpace
using HostSpace   = Kokkos::HostSpace;

// 1D device view — cuSOLVER buffers and per-window scalars
using view_1d          = Kokkos::View<double*, MemSpace>;
// 2D device view — window-local data (window, local_index), LayoutRight
using window_view_2d   = Kokkos::View<double**, Kokkos::LayoutRight, MemSpace>;
// 1D host view (unmanaged — wraps a raw pointer without owning it)
using host_view_um     = Kokkos::View<const double*, HostSpace, Kokkos::MemoryUnmanaged>;
// Host mirror of window-local 2D view
using host_window_view_2d = Kokkos::View<double**, Kokkos::LayoutRight, HostSpace>;

// MDRangePolicy with explicit right iteration order.
// Default MDRange iteration order on CUDA aligns with LayoutLeft (leftmost
// rank fastest).  Since window-local views use LayoutRight, the iteration
// order must be set explicitly so adjacent threads vary the rightmost rank.
using mdrange_right_2d = Kokkos::MDRangePolicy<
    Kokkos::Rank<2, Kokkos::Iterate::Right, Kokkos::Iterate::Right>>;

// Team policy types
using team_policy  = Kokkos::TeamPolicy<ExecSpace>;
using team_member  = team_policy::member_type;

// ────────────────────────────────────────────────────────────────────────────
// cuSOLVER macro
// ────────────────────────────────────────────────────────────────────────────

#define CUSOLVER_CHECK(call)                                                   \
    do {                                                                       \
        cusolverStatus_t st = (call);                                          \
        if (st != CUSOLVER_STATUS_SUCCESS)                                     \
            throw std::runtime_error(                                          \
                std::string("cuSOLVER error at ") + __FILE__ + ":"             \
                + std::to_string(__LINE__)                                     \
                + " — code " + std::to_string((int)st));                      \
    } while (0)

#define CUDA_CHECK(call)                                                      \
    do {                                                                       \
        cudaError_t err = (call);                                              \
        if (err != cudaSuccess)                                                \
            throw std::runtime_error(                                          \
                std::string("CUDA error at ") + __FILE__ + ":"                 \
                + std::to_string(__LINE__) + " — "                            \
                + cudaGetErrorString(err));                                    \
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
//  HOST: Generate test signal (same as CUDA version)
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
//  THE PIPELINE — Kokkos + cuSOLVER
//
//  Input:   const double*  raw host pointer (signal array)
//  Output:  host_window_view_2d   PSD time-frequency matrix [BATCH × NFREQ]
//
// ════════════════════════════════════════════════════════════════════════════

host_window_view_2d run_pipeline(const double* h_signal_ptr, int sig_len)
{
    // ══════════════════════════════════════════════════════════════════
    //  ALLOCATE — all device memory is Kokkos-managed Views
    // ══════════════════════════════════════════════════════════════════

    view_1d  d_sig    ("signal",    sig_len);
    window_view_2d  d_xw     ("windowed",  BATCH, WLEN);
    view_1d  d_A      ("matrices",  (size_t)BATCH * N2);       // flat for cuSOLVER
    view_1d  d_B      ("rhs",       (size_t)BATCH * ORDER);    // flat for cuSOLVER
    view_1d  d_B_orig ("rhs_orig",  (size_t)BATCH * ORDER);
    view_1d  d_r0     ("r0",        BATCH);
    view_1d  d_sigma2 ("sigma2",    BATCH);
    window_view_2d  d_psd    ("psd",       BATCH, NFREQ);

    // cuSOLVER-specific: pointer arrays and info (raw CUDA, not Kokkos)
    double **d_Aptr = nullptr, **d_Bptr = nullptr;
    int     *d_info = nullptr;
    CUDA_CHECK(cudaMalloc(&d_Aptr, BATCH * sizeof(double*)));
    CUDA_CHECK(cudaMalloc(&d_Bptr, BATCH * sizeof(double*)));
    CUDA_CHECK(cudaMalloc(&d_info, BATCH * sizeof(int)));

    cusolverDnHandle_t solver;
    CUSOLVER_CHECK(cusolverDnCreate(&solver));

    Timer t_total;
    t_total.start();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 1 — H2D: wrap raw pointer, deep_copy to device
    //
    //  This is the interface pattern.  The caller owns the allocation.
    //  The unmanaged View provides Kokkos type safety without copying
    //  or taking ownership.
    // ══════════════════════════════════════════════════════════════════

    Timer t_h2d;
    t_h2d.start();

    host_view_um h_sig(h_signal_ptr, sig_len);     // wrap, don't copy
    Kokkos::deep_copy(d_sig, h_sig);               // H2D

    Kokkos::fence("H2D complete");
    double ms_h2d = t_h2d.ms();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 2 — Hann windowing
    //
    //  mdrange_right_2d: two-dimensional dispatch, rightmost index fastest.
    //  Each (w, n) pair is one work item — window w, sample n.
    //  The cos() call is the transcendental.
    // ══════════════════════════════════════════════════════════════════

    Timer t_hann;
    t_hann.start();

    Kokkos::parallel_for("hann_window",
        mdrange_right_2d({0, 0}, {BATCH, WLEN}),
        KOKKOS_LAMBDA(const int w, const int n)
    {
        const double hann = 0.5 * (1.0 - Kokkos::cos(
            2.0 * PI * static_cast<double>(n)
                       / static_cast<double>(WLEN - 1)));
        d_xw(w, n) = d_sig(w * STRIDE + n) * hann;
    });

    Kokkos::fence("hann complete");
    double ms_hann = t_hann.ms();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 3 — Autocovariance + Toeplitz assembly
    //
    //  TeamPolicy: one team per window, ORDER+1 threads per team.
    //  Team scratch memory holds the p+1 autocovariance lags.
    //
    //  Pattern:
    //    1. Each thread computes one lag → team scratch
    //    2. team_barrier()
    //    3. Each thread fills one row of the Toeplitz matrix
    //
    //  The matrix is stored flat in d_A for cuSOLVER compatibility.
    //  Column-major: A(row, col) at offset  w*N2 + row + col*ORDER.
    // ══════════════════════════════════════════════════════════════════

    Timer t_acov;
    t_acov.start();

    const int scratch_bytes = (ORDER + 1) * sizeof(double);

    Kokkos::parallel_for("autocov_assemble",
        team_policy(BATCH, ORDER + 1)
            .set_scratch_size(0, Kokkos::PerTeam(scratch_bytes)),
        KOKKOS_LAMBDA(const team_member& team)
    {
        const int w   = team.league_rank();       // window index
        const int tid = team.team_rank();         // lag / row index

        // ── Scratch view for autocovariance lags ──────────────────
        Kokkos::View<double*, ExecSpace::scratch_memory_space,
                     Kokkos::MemoryUnmanaged>
            s_r(team.team_scratch(0), ORDER + 1);

        // ── Step 1: compute biased autocovariance lag `tid` ───────
        //    r[k] = (1/W) Σ_{n=k}^{W-1} x̃[n] · x̃[n−k]
        if (tid <= ORDER) {
            double rk = 0.0;
            for (int n = tid; n < WLEN; ++n)
                rk += d_xw(w, n) * d_xw(w, n - tid);
            rk /= static_cast<double>(WLEN);
            s_r(tid) = rk;
        }

        team.team_barrier();

        // ── Save r(0) ─────────────────────────────────────────────
        if (tid == 0)
            d_r0(w) = s_r(0);

        // ── Step 2: assemble Toeplitz row + RHS ───────────────────
        if (tid < ORDER) {
            const size_t base = static_cast<size_t>(w) * N2;

            // Matrix: R[row][col] = r[|row − col|]  (column-major)
            // Diagonal loading: ε·I for strict positive definiteness
            for (int col = 0; col < ORDER; ++col) {
                int lag = (tid >= col) ? (tid - col) : (col - tid);
                double val = s_r(lag);
                if (tid == col)
                    val += 1.0e-10 * s_r(0) + 1.0e-14;
                d_A(base + tid + col * ORDER) = val;
            }

            // RHS: b[row] = r[row + 1]
            d_B(static_cast<size_t>(w) * ORDER + tid) = s_r(tid + 1);
        }
    });

    Kokkos::fence("autocov complete");
    double ms_acov = t_acov.ms();

    // ══════════════════════════════════════════════════════════════════
    //  Save RHS before cuSOLVER overwrites it with the solution
    // ══════════════════════════════════════════════════════════════════

    Kokkos::deep_copy(d_B_orig, d_B);
    Kokkos::fence("rhs saved");

    // ══════════════════════════════════════════════════════════════════
    //  Build cuSOLVER pointer arrays — Kokkos kernel writing raw ptrs
    //
    //  This is the Kokkos↔cuSOLVER bridge.  We extract the raw device
    //  pointer from each View with .data(), then compute offsets into
    //  the flat arrays.
    // ══════════════════════════════════════════════════════════════════

    {
        double* raw_A = d_A.data();
        double* raw_B = d_B.data();

        Kokkos::parallel_for("build_ptrs",
            Kokkos::RangePolicy<ExecSpace>(0, BATCH),
            KOKKOS_LAMBDA(const int k)
        {
            d_Aptr[k] = raw_A + static_cast<size_t>(k) * N2;
            d_Bptr[k] = raw_B + static_cast<size_t>(k) * ORDER;
        });

        Kokkos::fence("ptrs built");
    }

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 4a — Batched Cholesky factorisation (cuSOLVER)
    //
    //  Kokkos::fence() before and after the library call.
    //  cuSOLVER operates on the same default stream as Kokkos,
    //  but the fence makes the boundary explicit and safe.
    // ══════════════════════════════════════════════════════════════════

    Timer t_fact;
    t_fact.start();

    CUSOLVER_CHECK(cusolverDnDpotrfBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, d_Aptr, ORDER, d_info, BATCH));

    cudaDeviceSynchronize();
    double ms_fact = t_fact.ms();

    // ── Check factorisations ──────────────────────────────────────────
    //  Diagonal loading guarantees SPD.  Failure here is fatal.
    {
        std::vector<int> h_info(BATCH);
        CUDA_CHECK(cudaMemcpy(h_info.data(), d_info,
                              BATCH * sizeof(int), cudaMemcpyDeviceToHost));
        int bad = 0;
        for (int k = 0; k < BATCH; ++k)
            if (h_info[k] != 0) ++bad;
        if (bad > 0)
            throw std::runtime_error(
                "Cholesky factorisation failed: "
                + std::to_string(bad) + " / " + std::to_string(BATCH));
        printf("  All %d factorisations succeeded\n", BATCH);
    }

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 4b — Batched triangular solve (cuSOLVER)
    //
    //  After this, d_B contains the AR coefficients for each window.
    // ══════════════════════════════════════════════════════════════════

    Timer t_solve;
    t_solve.start();

    CUSOLVER_CHECK(cusolverDnDpotrsBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, 1, d_Aptr, ORDER, d_Bptr, ORDER, d_info, BATCH));

    cudaDeviceSynchronize();
    double ms_solve = t_solve.ms();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 4c — Innovation variance σ²
    //
    //  σ²_w = r(0) − aᵀ · r_orig
    //
    //  Simple RangePolicy — one thread per window.
    // ══════════════════════════════════════════════════════════════════

    Timer t_sig2;
    t_sig2.start();

    Kokkos::parallel_for("sigma2",
        Kokkos::RangePolicy<ExecSpace>(0, BATCH),
        KOKKOS_LAMBDA(const int w)
    {
        const size_t off = static_cast<size_t>(w) * ORDER;
        double dot = 0.0;
        for (int k = 0; k < ORDER; ++k)
            dot += d_B(off + k) * d_B_orig(off + k);
        d_sigma2(w) = Kokkos::fmax(d_r0(w) - dot, 1.0e-30);
    });

    Kokkos::fence("sigma2 complete");
    double ms_sig2 = t_sig2.ms();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 5 — PSD evaluation
    //
    //  mdrange_right_2d over (window, frequency_bin), so adjacent lanes vary j.
    //  16.8 million independent evaluations of the AR transfer function.
    //
    //  P_w(f_j) = σ² / |1 − Σ_k a_k e^{−j2πf_j k/f_s}|²
    //
    //  Output in dB: 10·log10(P).
    // ══════════════════════════════════════════════════════════════════

    Timer t_psd;
    t_psd.start();

    Kokkos::parallel_for("psd_eval",
        mdrange_right_2d({0, 0}, {BATCH, NFREQ}),
        KOKKOS_LAMBDA(const int w, const int j)
    {
        const double fj = static_cast<double>(j)
                        * (FS * 0.5)
                        / static_cast<double>(NFREQ - 1);

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

        d_psd(w, j) = 10.0 * Kokkos::log10(Kokkos::fmax(P, 1.0e-30));
    });

    Kokkos::fence("psd complete");
    double ms_psd = t_psd.ms();

    // ══════════════════════════════════════════════════════════════════
    //  STAGE 6 — D2H: deep_copy PSD to host mirror
    // ══════════════════════════════════════════════════════════════════

    Timer t_d2h;
    t_d2h.start();

    host_window_view_2d h_psd("psd_host", BATCH, NFREQ);
    Kokkos::deep_copy(h_psd, d_psd);

    Kokkos::fence("D2H complete");
    double ms_d2h = t_d2h.ms();

    double ms_total = t_total.ms();

    // ══════════════════════════════════════════════════════════════════
    //  TIMING REPORT
    // ══════════════════════════════════════════════════════════════════

    double ms_xfer = ms_h2d + ms_d2h;
    double ms_comp = ms_hann + ms_acov + ms_fact + ms_solve + ms_sig2 + ms_psd;

    printf("\n── Timing ──────────────────────────────────────────────────────\n");
    printf("  H2D (deep_copy):     %8.2f ms\n", ms_h2d);
    printf("  Hann (MDRange):      %8.2f ms\n", ms_hann);
    printf("  Autocov (TeamPolicy):%8.2f ms\n", ms_acov);
    printf("  Cholesky (cuSOLVER): %8.2f ms\n", ms_fact);
    printf("  Solve (cuSOLVER):    %8.2f ms\n", ms_solve);
    printf("  σ² (RangePolicy):   %8.2f ms\n", ms_sig2);
    printf("  PSD (MDRange):       %8.2f ms\n", ms_psd);
    printf("  D2H (deep_copy):     %8.2f ms\n", ms_d2h);
    printf("  ──────────────────────────────────\n");
    printf("  Transfer:            %8.2f ms  (%4.1f%%)\n",
           ms_xfer, 100.0 * ms_xfer / ms_total);
    printf("  Compute:             %8.2f ms  (%4.1f%%)\n",
           ms_comp, 100.0 * ms_comp / ms_total);
    printf("  TOTAL:               %8.2f ms\n", ms_total);

    // ── Cleanup cuSOLVER resources ────────────────────────────────────
    cusolverDnDestroy(solver);
    cudaFree(d_Aptr);
    cudaFree(d_Bptr);
    cudaFree(d_info);

    return h_psd;
}

// ════════════════════════════════════════════════════════════════════════════
//                                MAIN
// ════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[])
{
    Kokkos::initialize(argc, argv);
    {
        printf("══════════════════════════════════════════════════════════\n");
        printf("  AR(%d) Spectral Estimation — Kokkos + cuSOLVER\n", ORDER);
        printf("  %d windows × %d samples, stride %d\n",
               BATCH, WLEN, STRIDE);
        printf("  Signal: %d samples (%.1f s at %.0f Hz)\n",
               SIGLEN, (double)SIGLEN / FS, FS);
        printf("  Execution space: %s\n",
               typeid(ExecSpace).name());
        printf("══════════════════════════════════════════════════════════\n\n");

        // ── Generate test signal ──────────────────────────────────────
        //
        //  This is the "data comes in as double pointer to array" case.
        //  The pipeline function accepts const double* and wraps it.

        printf("  Generating test signal ...\n");
        std::vector<double> signal_buffer(SIGLEN);
        generate_signal(signal_buffer.data(), SIGLEN);

        // ── Run pipeline ──────────────────────────────────────────────
        //
        //  Call with raw pointer.  This is the production interface:
        //  the caller owns the buffer, the pipeline borrows it.

        const double* raw_ptr = signal_buffer.data();

        host_window_view_2d h_psd = run_pipeline(raw_ptr, SIGLEN);

        // ── Verification: chirp tracking ──────────────────────────────

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
                if (h_psd(w, j) > peak_val) {
                    peak_val = h_psd(w, j);
                    peak_bin = j;
                }
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

        printf("\n  Mean tracking error: %.1f Hz\n", err_sum / fmax(n_tracked, 1));
        printf("  Max  tracking error: %.1f Hz\n", err_max);

        double T_sig = (double)SIGLEN / FS;
        printf("\n  Real-time factor: pipeline processes %.1f s of signal\n", T_sig);
        printf("══════════════════════════════════════════════════════════\n");
    }
    Kokkos::finalize();
    return 0;
}
