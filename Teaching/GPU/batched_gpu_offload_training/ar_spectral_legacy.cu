// ============================================================================
// ar_spectral_legacy.cu
//
// "Just move the solve to the GPU."
//
// This is the version that exists in production when someone is told to
// GPU-accelerate the linear algebra without restructuring the pipeline.
// Every anti-pattern is real — these are not invented for pedagogical
// effect.  They are what happens when a serial C codebase acquires CUDA
// calls one at a time over six months.
//
// What's wrong (partial list):
//
//   • cudaMalloc / cudaFree INSIDE the 65,536-iteration loop
//   • one H2D + one D2H per iteration (65,536 × 2 = 131,072 transfers)
//   • cuSOLVER handle created and destroyed per call to the solve function
//   • cuSOLVER workspace queried and allocated per call
//   • Hann window recomputed from scratch every iteration (cos × 128)
//   • autocovariance computed on CPU, one lag at a time, one window at a time
//   • PSD evaluated on CPU, one frequency bin at a time
//   • no lifetime tracking — caller's buffer can go stale silently
//   • cleanup skipped on error paths (cudaFree never reached if dpotrf fails)
//   • global error flag checked intermittently, not on every call
//   • mixed int/size_t indexing with silent truncation
//   • "it works on my machine" comments
//
// The punchline: this version is SLOWER than pure CPU LAPACK because
// the transfer overhead per 32×32 matrix exceeds the compute savings.
// The GPU spends more time waiting for PCIe than doing arithmetic.
//
// Build:
//   nvcc -O2 -arch=sm_70 ar_spectral_legacy.cu \
//        -lcusolver -llapack -lblas -o ar_spectral_legacy
//
//   # Without CPU reference path:
//   nvcc -O2 -arch=sm_70 -DSKIP_CPU ar_spectral_legacy.cu \
//        -lcusolver -o ar_spectral_legacy
//
// ============================================================================

#include <cuda_runtime.h>
#include <cusolverDn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ────────────────────────────────────────────────────────────────────────────
// "Constants" — some are #defines, some are globals, because of course
// ────────────────────────────────────────────────────────────────────────────

#define ORDER    32
#define WLEN     128
#define STRIDE   32
#define N_WINDOWS 65536
#define NFREQ    256
#define FS       10000.0

#define CHIRP_F0 200.0
#define CHIRP_F1 2000.0
#define TRANS_F  1200.0
#define TRANS_T0 105.0
#define TRANS_SD 2.0
#define TRANS_A  1.5
#define NOISE_SD 0.15

// "we'll fix the global later"
static int g_gpu_errors = 0;
static int g_solve_failures = 0;

#define SIGLEN  ((N_WINDOWS - 1) * STRIDE + WLEN)

// ────────────────────────────────────────────────────────────────────────────
// Timer (the only thing done right)
// ────────────────────────────────────────────────────────────────────────────

static double get_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// ────────────────────────────────────────────────────────────────────────────
// Signal generation
// ────────────────────────────────────────────────────────────────────────────

static unsigned long long g_rng = 123456789ULL;

static double randn(void)
{
    g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    double u1 = (double)(g_rng >> 11) / (double)(1ULL << 53);
    g_rng = g_rng * 6364136223846793005ULL + 1442695040888963407ULL;
    double u2 = (double)(g_rng >> 11) / (double)(1ULL << 53);
    if (u1 < 1e-15) u1 = 1e-15;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

static void generate_signal(double* x, int len)
{
    double T = (double)len / FS;
    int n;
    for (n = 0; n < len; n++) {
        double t = (double)n / FS;
        double chirp = sin(2.0*M_PI*(CHIRP_F0*t + (CHIRP_F1-CHIRP_F0)*t*t/(2.0*T)));
        double env = exp(-0.5*(t-TRANS_T0)*(t-TRANS_T0)/(TRANS_SD*TRANS_SD));
        x[n] = chirp + TRANS_A * env * sin(2.0*M_PI*TRANS_F*t) + NOISE_SD * randn();
    }
}

// ════════════════════════════════════════════════════════════════════════════
//
// THE LEGACY SOLVE FUNCTION
//
// "gpu_solve_cholesky" — called once per window, once per iteration of the
// outer loop.  Creates a cuSOLVER handle, allocates device memory, copies
// the matrix, solves, copies back, frees everything.  65,536 times.
//
// This function was added by someone who read the cuSOLVER docs, got one
// solve working, wrapped it in a function, and called it from the loop.
// It is correct.  It is catastrophically slow.
//
// ════════════════════════════════════════════════════════════════════════════

static int gpu_solve_cholesky(double* A,    /* ORDER×ORDER, column-major, overwritten */
                              double* b,    /* ORDER, overwritten with solution */
                              int n)
{
    cusolverDnHandle_t handle;
    cusolverStatus_t status;
    cudaError_t cerr;

    double *d_A = NULL, *d_b = NULL;
    int *d_info = NULL;
    int h_info = 0;
    double *d_work = NULL;
    int lwork = 0;

    // Create handle — yes, every single call
    status = cusolverDnCreate(&handle);
    if (status != CUSOLVER_STATUS_SUCCESS) {
        g_gpu_errors++;
        return -1;
    }

    // Allocate device memory
    cerr = cudaMalloc(&d_A, n * n * sizeof(double));
    if (cerr != cudaSuccess) { g_gpu_errors++; cusolverDnDestroy(handle); return -1; }

    cerr = cudaMalloc(&d_b, n * sizeof(double));
    if (cerr != cudaSuccess) {
        g_gpu_errors++;
        cudaFree(d_A);
        cusolverDnDestroy(handle);
        return -1;
    }

    cerr = cudaMalloc(&d_info, sizeof(int));
    if (cerr != cudaSuccess) {
        g_gpu_errors++;
        cudaFree(d_A);
        cudaFree(d_b);
        cusolverDnDestroy(handle);
        return -1;
        // at least we remembered to free... this time
    }

    // Copy matrix and RHS to device
    cudaMemcpy(d_A, A, n * n * sizeof(double), cudaMemcpyHostToDevice);
    cudaMemcpy(d_b, b, n * sizeof(double),     cudaMemcpyHostToDevice);

    // Query workspace size — every call, same matrix size, same answer
    cusolverDnDpotrf_bufferSize(handle, CUBLAS_FILL_MODE_LOWER, n, d_A, n, &lwork);

    // Allocate workspace — every call
    cerr = cudaMalloc(&d_work, lwork * sizeof(double));
    if (cerr != cudaSuccess) {
        // give up, but we don't free d_info — oops
        g_gpu_errors++;
        cudaFree(d_A);
        cudaFree(d_b);
        cusolverDnDestroy(handle);
        return -1;  // d_info leaked
    }

    // Factorize
    cusolverDnDpotrf(handle, CUBLAS_FILL_MODE_LOWER, n, d_A, n,
                     d_work, lwork, d_info);
    cudaDeviceSynchronize();

    cudaMemcpy(&h_info, d_info, sizeof(int), cudaMemcpyDeviceToHost);
    if (h_info != 0) {
        g_solve_failures++;
        // cleanup? sure, some of it
        cudaFree(d_A);
        cudaFree(d_b);
        cudaFree(d_work);
        // forgot d_info again
        cusolverDnDestroy(handle);
        return -2;
    }

    // Solve
    cusolverDnDpotrs(handle, CUBLAS_FILL_MODE_LOWER, n, 1,
                     d_A, n, d_b, n, d_info);
    cudaDeviceSynchronize();

    // Copy solution back
    cudaMemcpy(b, d_b, n * sizeof(double), cudaMemcpyDeviceToHost);

    // Free everything
    cudaFree(d_A);
    cudaFree(d_b);
    cudaFree(d_info);
    cudaFree(d_work);
    cusolverDnDestroy(handle);

    return 0;
}


// ════════════════════════════════════════════════════════════════════════════
// CPU-side windowing, autocovariance, matrix assembly, PSD evaluation
//
// All done per-window, inside the serial loop.  None of this is on the GPU.
// The "GPU acceleration" is just the 32×32 solve.
// ════════════════════════════════════════════════════════════════════════════

// Compute Hann window — recomputed every iteration because "it's cheap"
static void apply_hann(const double* sig, int offset, double* out, int wlen)
{
    int n;
    for (n = 0; n < wlen; n++) {
        double w = 0.5 * (1.0 - cos(2.0 * M_PI * n / (wlen - 1)));
        out[n] = sig[offset + n] * w;
    }
}

// Biased autocovariance — one lag at a time
static void autocov(const double* x, int wlen, int maxlag, double* r)
{
    int k, n;
    for (k = 0; k <= maxlag; k++) {
        double sum = 0.0;
        for (n = k; n < wlen; n++)
            sum += x[n] * x[n - k];
        r[k] = sum / (double)wlen;
    }
}

// Assemble Toeplitz matrix (column-major for LAPACK/cuSOLVER)
static void assemble_toeplitz(const double* r, int p, double* A, double* b)
{
    int i, j;
    for (j = 0; j < p; j++) {
        for (i = 0; i < p; i++) {
            int lag = abs(i - j);
            A[i + j * p] = r[lag];
        }
    }
    for (i = 0; i < p; i++)
        b[i] = r[i + 1];
}

// PSD from AR coefficients — per frequency bin, on CPU
static void evaluate_psd(const double* a, double sigma2, int p,
                         int nfreq, double fs, double* psd)
{
    int j, k;
    for (j = 0; j < nfreq; j++) {
        double fj = (double)j * (fs / 2.0) / (double)(nfreq - 1);
        double re = 1.0, im = 0.0;
        for (k = 0; k < p; k++) {
            double theta = -2.0 * M_PI * fj * (k + 1) / fs;
            re -= a[k] * cos(theta);
            im -= a[k] * sin(theta);
        }
        double mag2 = re*re + im*im;
        if (mag2 < 1e-30) mag2 = 1e-30;
        double P = sigma2 / mag2;
        if (P < 1e-30) P = 1e-30;
        psd[j] = 10.0 * log10(P);
    }
}


// ════════════════════════════════════════════════════════════════════════════
//                              MAIN
// ════════════════════════════════════════════════════════════════════════════

int main()
{
    printf("================================================================\n");
    printf("  AR(%d) Spectral Estimation — LEGACY VERSION\n", ORDER);
    printf("  %d windows, GPU-accelerated solve (per-iteration transfer)\n", N_WINDOWS);
    printf("  This version exists to demonstrate what NOT to do.\n");
    printf("================================================================\n\n");

    // Device info
    struct cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("  Device: %s\n\n", prop.name);

    // Allocate signal
    double* signal = (double*)malloc(SIGLEN * sizeof(double));
    if (!signal) { fprintf(stderr, "malloc failed\n"); return 1; }

    printf("  Generating signal ...\n");
    generate_signal(signal, SIGLEN);

    // Allocate PSD output
    double* psd_all = (double*)malloc((size_t)N_WINDOWS * NFREQ * sizeof(double));
    if (!psd_all) { fprintf(stderr, "malloc failed\n"); free(signal); return 1; }

    // Per-window scratch — allocated once, at least
    double* windowed = (double*)malloc(WLEN * sizeof(double));
    double* acov = (double*)malloc((ORDER + 1) * sizeof(double));
    double* matrix = (double*)malloc(ORDER * ORDER * sizeof(double));
    double* rhs = (double*)malloc(ORDER * sizeof(double));
    double* rhs_save = (double*)malloc(ORDER * sizeof(double));

    // ── THE LOOP ──────────────────────────────────────────────────────
    //
    // 65,536 iterations.  Each one:
    //   CPU: Hann window (128 cos calls)
    //   CPU: autocovariance (33 lags × 128 multiply-adds)
    //   CPU: Toeplitz assembly (32×32 writes)
    //   GPU: cudaMalloc × 4, memcpy H2D × 2, solve, memcpy D2H × 1,
    //        cudaFree × 4, cusolverDnCreate, cusolverDnDestroy
    //   CPU: innovation variance
    //   CPU: PSD evaluation (256 × 32 sin/cos)
    //
    // Total GPU calls per iteration: ~15 (create, malloc×4, memcpy×3,
    //   bufferSize, potrf, potrs, sync×2, free×4, destroy)
    // Total GPU calls for the run: ~1,000,000
    //
    // Each cudaMemcpy moves 8 KB (matrix) or 256 bytes (RHS).
    // PCIe round-trip latency: ~10 μs.  Useful compute: ~1 μs.
    // The GPU is a space heater.

    printf("\n  Running %d iterations (this will be slow) ...\n", N_WINDOWS);
    fflush(stdout);

    double t_start = get_time_ms();

    int w;
    int print_interval = N_WINDOWS / 10;

    for (w = 0; w < N_WINDOWS; w++)
    {
        // Progress — because you'll be waiting
        if (w > 0 && w % print_interval == 0)
        {
            double elapsed = get_time_ms() - t_start;
            double pct = 100.0 * w / N_WINDOWS;
            double eta = elapsed / pct * (100.0 - pct);
            printf("    %5.1f%%  elapsed %.1f s  ETA %.1f s\n",
                   pct, elapsed / 1000.0, eta / 1000.0);
            fflush(stdout);
        }

        // CPU: Hann window
        apply_hann(signal, w * STRIDE, windowed, WLEN);

        // CPU: autocovariance
        autocov(windowed, WLEN, ORDER, acov);

        // CPU: assemble Toeplitz matrix + RHS
        assemble_toeplitz(acov, ORDER, matrix, rhs);

        // Save RHS for sigma² computation
        memcpy(rhs_save, rhs, ORDER * sizeof(double));

        // GPU: solve (the "accelerated" part)
        //
        // This single call does:
        //   cusolverDnCreate
        //   cudaMalloc × 4
        //   cudaMemcpy H2D × 2
        //   cusolverDnDpotrf_bufferSize
        //   cudaMalloc (workspace)
        //   cusolverDnDpotrf
        //   cudaDeviceSynchronize
        //   cudaMemcpy D2H (info check)
        //   cusolverDnDpotrs
        //   cudaDeviceSynchronize
        //   cudaMemcpy D2H (solution)
        //   cudaFree × 4
        //   cusolverDnDestroy
        //
        // For a 32×32 matrix.  ~11,000 FLOPs of useful work.

        int err = gpu_solve_cholesky(matrix, rhs, ORDER);
        if (err != 0) {
            // "just skip it" — the production classic
            memset(&psd_all[(size_t)w * NFREQ], 0, NFREQ * sizeof(double));
            continue;
        }

        // CPU: innovation variance
        double sigma2 = acov[0];
        {
            int k;
            for (k = 0; k < ORDER; k++)
                sigma2 -= rhs[k] * rhs_save[k];
        }

        // CPU: PSD evaluation (256 frequency bins × 32 sin/cos each)
        evaluate_psd(rhs, sigma2, ORDER, NFREQ, FS, &psd_all[(size_t)w * NFREQ]);
    }

    double t_end = get_time_ms();
    double elapsed = t_end - t_start;

    printf("\n  Done.\n\n");

    // ── Timing report ─────────────────────────────────────────────────

    printf("── Results ─────────────────────────────────────────────────────\n");
    printf("  Total time:          %8.1f ms  (%.1f s)\n", elapsed, elapsed / 1000.0);
    printf("  Per window:          %8.3f ms\n", elapsed / N_WINDOWS);
    printf("  GPU errors:          %d\n", g_gpu_errors);
    printf("  Solve failures:      %d\n", g_solve_failures);

    double signal_duration = (double)SIGLEN / FS;
    printf("  Signal duration:     %.1f s\n", signal_duration);
    printf("  Real-time factor:    %.1f×\n",
           signal_duration * 1000.0 / elapsed);

    printf("\n  Estimated overhead breakdown:\n");
    printf("    cudaMalloc/Free:   %d calls  (~%.0f ms at 5 μs each)\n",
           N_WINDOWS * 8, N_WINDOWS * 8 * 0.005);
    printf("    cudaMemcpy:        %d calls  (~%.0f ms at 10 μs each)\n",
           N_WINDOWS * 4, N_WINDOWS * 4 * 0.010);
    printf("    cusolverDnCreate:  %d calls  (~%.0f ms at 50 μs each)\n",
           N_WINDOWS, N_WINDOWS * 0.050);
    printf("    Useful GPU FLOPS:  %.0f M  (%.1f ms at 7 TFLOP/s)\n",
           (double)N_WINDOWS * 11000.0 / 1e6,
           (double)N_WINDOWS * 11000.0 / 7e12 * 1e3);
    printf("\n  The GPU did %.1f ms of useful work buried under\n"
           "  %.0f ms of dispatch and transfer overhead.\n",
           (double)N_WINDOWS * 11000.0 / 7e12 * 1e3, elapsed);

    // ── Chirp tracking (abbreviated) ──────────────────────────────────

    printf("\n── Chirp tracking ──────────────────────────────────────────────\n");
    {
        double T = (double)SIGLEN / FS;
        int spots[] = {0, 10000, 32768, 50000, 65535};
        printf("  %8s  %8s  %8s  %8s\n", "Window", "Time(s)", "True(Hz)", "Est(Hz)");
        int s;
        for (s = 0; s < 5; s++) {
            int ww = spots[s];
            double tc = ((double)(ww * STRIDE) + WLEN * 0.5) / FS;
            double f_true = CHIRP_F0 + (CHIRP_F1 - CHIRP_F0) * tc / T;
            int peak = 0;
            double pv = -1e30;
            int j;
            for (j = 1; j < NFREQ; j++) {
                double v = psd_all[(size_t)ww * NFREQ + j];
                if (v > pv) { pv = v; peak = j; }
            }
            double f_est = (double)peak * (FS/2.0) / (double)(NFREQ-1);
            printf("  %8d  %8.2f  %8.1f  %8.1f\n", ww, tc, f_true, f_est);
        }
    }

    printf("\n================================================================\n");
    printf("  This version creates and destroys a cuSOLVER handle %d times.\n", N_WINDOWS);
    printf("  It calls cudaMalloc %d times and cudaMemcpy %d times.\n",
           N_WINDOWS * 8, N_WINDOWS * 4);
    printf("  The GPU computes for ~%.1f ms total.\n",
           (double)N_WINDOWS * 11000.0 / 7e12 * 1e3);
    printf("  The rest is overhead.\n");
    printf("================================================================\n");

    // Cleanup — at least we got this right
    free(signal);
    free(psd_all);
    free(windowed);
    free(acov);
    free(matrix);
    free(rhs);
    free(rhs_save);

    return 0;
}
