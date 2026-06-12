// ============================================================================
// ar_spectral_original.c
//
// THE STARTING ARTIFACT
//
// This is the code that exists before anyone says "GPU."  Hand-written
// Cholesky from a Numerical Recipes adaptation circa 2007, forward/backward
// substitution, non-contiguous double** matrix storage, global scratch
// buffers, and OpenMP bolted on in 2014 by someone who no longer works here.
//
// It runs.  It produces correct results.  It has been in production for
// years.  Nobody wants to touch it, and now it needs to go onto a GPU.
//
// Archaeological layers visible in the code:
//
//   2007  Original author: serial C, NR-style Cholesky, 1-based comments
//         but 0-based code, global scratch, no threading
//   2011  Second developer: added PSD evaluation, changed nothing else,
//         left "// TODO" comments that are still here
//   2014  Third developer: added OpenMP, fixed nothing, introduced the
//         thread-local scratch allocation that leaks on error
//   2016  Fourth person: "fixed" the leak by making scratch static,
//         reintroducing the thread-safety bug the OpenMP was supposed to fix
//   2019  Fifth person: added the #pragma omp critical around the output
//         write, which serializes the only parallel part
//
// The actual migration obstacles (why you can't "just add CUDA"):
//
//   1. double** row-pointer storage — rows are individually malloc'd,
//      not contiguous.  cudaMemcpy needs contiguous buffers.
//   2. In-place factorization — Cholesky overwrites the input matrix.
//      To batch on GPU you need all matrices simultaneously, not
//      one-at-a-time-overwritten.
//   3. Scratch sharing — global/static scratch arrays assume serial
//      execution or are "protected" by OpenMP critical sections that
//      destroy parallelism.
//   4. Mixed concerns — windowing, autocovariance, assembly, solve,
//      and PSD evaluation are all in one function.  No seam to insert
//      a batched solver.
//   5. No separation of "build the matrix" from "solve the matrix" —
//      the matrix only exists transiently inside the loop body.
//
// Build:
//   gcc -O2 -fopenmp ar_spectral_original.c -lm -o ar_spectral_original
//
// ============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifdef _OPENMP
#include <omp.h>
#endif

// ────────────────────────────────────────────────────────────────────────────
// "Configuration" — half #defines from 2007, half globals from 2014
// ────────────────────────────────────────────────────────────────────────────

#define AR_ORDER    32
#define WIN_LEN     128
#define WIN_STRIDE  32
#define NUM_FREQ    256

/* fs was a #define, then someone needed it as a variable for resampling
   support that was never finished.  now it's both. */
#define SAMPLE_RATE 10000.0
static double g_fs = SAMPLE_RATE;

#define CHIRP_F0 200.0
#define CHIRP_F1 2000.0

static int g_num_windows = 0;   /* set in main */
static int g_siglen = 0;        /* set in main */

/* error tracking — "we'll add proper error handling later" (2007) */
static int g_chol_failures = 0;

// ────────────────────────────────────────────────────────────────────────────
// Matrix allocation — the NR double** idiom
//
// Each row is a separate malloc.  Rows are NOT contiguous in memory.
// This made sense in 1992 when NR was written (it avoids computing
// row×cols+col for every access) and it was fine on CPUs.  It is
// the #1 obstacle to GPU migration because you cannot cudaMemcpy
// a double** — the rows are scattered across the heap.
//
// Nobody has changed this because everything that touches matrices
// uses the double** interface, and changing it would require touching
// every matrix function in the codebase.
// ────────────────────────────────────────────────────────────────────────────

static double** mat_alloc(int rows, int cols)
{
    int i;
    double** m = (double**)malloc(rows * sizeof(double*));
    if (!m) return NULL;
    for (i = 0; i < rows; i++) {
        m[i] = (double*)malloc(cols * sizeof(double));
        if (!m[i]) {
            /* partial cleanup — usually correct, sometimes not */
            int j;
            for (j = 0; j < i; j++) free(m[j]);
            free(m);
            return NULL;
        }
    }
    return m;
}

static void mat_free(double** m, int rows)
{
    if (m) {
        int i;
        for (i = 0; i < rows; i++) free(m[i]);
        free(m);
    }
}


// ════════════════════════════════════════════════════════════════════════════
// CHOLESKY DECOMPOSITION — adapted from Numerical Recipes in C, 2nd ed.
//
// choldc: in-place decomposition of a symmetric positive-definite matrix.
//         The lower triangle of a[][] is overwritten with L.
//         The diagonal of L is returned in p[] (not stored in a[][]).
//
// cholsl: solve L L^T x = b given the output of choldc.
//         Forward substitution then back substitution.
//
// Original NR code was 1-based (a[1..n][1..n]).  This version was
// converted to 0-based in 2007.  The variable names (p, i, j, k, sum)
// are from the book.  The comments with equation numbers refer to
// NR 2nd edition §2.9.
//
// NOTE: this modifies a[][] in place.  After choldc, the original
// matrix is gone.  This is fine for one-at-a-time serial processing
// but makes batching impossible without copying first.
// ════════════════════════════════════════════════════════════════════════════

static int choldc(double **a, int n, double *p)
{
    int i, j, k;
    double sum;

    for (i = 0; i < n; i++) {
        for (j = i; j < n; j++) {
            sum = a[i][j];
            for (k = i - 1; k >= 0; k--)
                sum -= a[i][k] * a[j][k];
            if (i == j) {
                if (sum <= 0.0) {
                    /* not positive definite — NR says "can be made more
                       robust" but nobody ever did */
                    return -1;
                }
                p[i] = sqrt(sum);
            } else {
                a[j][i] = sum / p[i];
            }
        }
    }
    return 0;
}

static void cholsl(double **a, int n, double *p, double *b, double *x)
{
    int i, k;
    double sum;

    /* forward substitution: solve L y = b */
    for (i = 0; i < n; i++) {
        sum = b[i];
        for (k = i - 1; k >= 0; k--)
            sum -= a[i][k] * x[k];
        x[i] = sum / p[i];
    }

    /* back substitution: solve L^T x = y */
    for (i = n - 1; i >= 0; i--) {
        sum = x[i];
        for (k = i + 1; k < n; k++)
            sum -= a[k][i] * x[k];
        x[i] = sum / p[i];
    }
}


// ════════════════════════════════════════════════════════════════════════════
// THE MAIN PROCESSING FUNCTION
//
// process_windows: the serial core that was later wrapped in OpenMP.
// Processes a range of windows [w_start, w_end).
//
// Everything is in here: windowing, autocovariance, matrix assembly,
// Cholesky solve, PSD evaluation.  There is no seam between "build
// the matrix" and "solve the matrix" because when this was written
// there was no reason to separate them.
//
// The scratch arrays (windowed, acov, matrix, p, rhs, sol, psd_buf)
// were originally globals.  In 2014 they became function-locals when
// OpenMP was added.  In 2016 someone made them static to "fix a leak"
// (the leak was on the error path, not here) and reintroduced the
// thread-safety bug.  In 2019 someone added thread-local storage
// which works but allocates per-thread and never frees.
// ════════════════════════════════════════════════════════════════════════════

static void process_windows(double *signal, int siglen,
                            double *psd_out,     /* [num_windows * NUM_FREQ] */
                            int w_start, int w_end)
{
    /* Per-thread scratch — allocated once per call.
       The 2016 "static" keyword was removed in 2019 and replaced with
       stack allocation.  For ORDER=32 and WIN_LEN=128 this is fine.
       For larger orders it would blow the stack. */

    double windowed[WIN_LEN];
    double acov_buf[AR_ORDER + 1];
    double p_diag[AR_ORDER];       /* Cholesky diagonal from choldc */
    double rhs[AR_ORDER];
    double rhs_save[AR_ORDER];
    double solution[AR_ORDER];
    double psd_buf[NUM_FREQ];

    /* The matrix — double** because that's what choldc expects.
       Rows allocated individually.  This is the allocation the 2014
       developer warned about in a comment that was later deleted. */
    double** matrix = mat_alloc(AR_ORDER, AR_ORDER);
    if (!matrix) {
        fprintf(stderr, "process_windows: mat_alloc failed\n");
        return;  /* no cleanup needed — stack arrays, and we just leak
                    the partial work.  "it never happens in practice" */
    }

    int w, n, i, j, k;

    for (w = w_start; w < w_end; w++)
    {
        int offset = w * WIN_STRIDE;

        /* ── Hann window ──────────────────────────────────────────── */
        for (n = 0; n < WIN_LEN; n++) {
            double hann = 0.5 * (1.0 - cos(2.0 * M_PI * n / (WIN_LEN - 1)));
            windowed[n] = signal[offset + n] * hann;
        }

        /* ── Biased autocovariance ────────────────────────────────── */
        for (k = 0; k <= AR_ORDER; k++) {
            double sum = 0.0;
            for (n = k; n < WIN_LEN; n++)
                sum += windowed[n] * windowed[n - k];
            acov_buf[k] = sum / (double)WIN_LEN;
        }

        /* ── Toeplitz matrix assembly ─────────────────────────────── */
        for (i = 0; i < AR_ORDER; i++) {
            for (j = 0; j < AR_ORDER; j++) {
                int lag = i - j;
                if (lag < 0) lag = -lag;
                matrix[i][j] = acov_buf[lag];
            }
            rhs[i] = acov_buf[i + 1];
        }

        /* save RHS for sigma² computation (solve overwrites nothing,
           but cholsl writes into solution[], not rhs[].  we save
           anyway because someone once changed cholsl to modify b
           and it took two weeks to find the bug) */
        memcpy(rhs_save, rhs, AR_ORDER * sizeof(double));

        /* ── Cholesky solve ───────────────────────────────────────── */
        if (choldc(matrix, AR_ORDER, p_diag) != 0) {
            /* factorisation failed — zero the output and move on.
               this shouldn't happen with a valid autocovariance matrix
               but it does when the window is all zeros (specimen not
               loaded yet, or DAQ dropout) */
            #pragma omp atomic
            g_chol_failures++;

            memset(&psd_out[(size_t)w * NUM_FREQ], 0,
                   NUM_FREQ * sizeof(double));
            continue;
        }

        cholsl(matrix, AR_ORDER, p_diag, rhs, solution);

        /* ── Innovation variance ──────────────────────────────────── */
        double sigma2 = acov_buf[0];
        for (k = 0; k < AR_ORDER; k++)
            sigma2 -= solution[k] * rhs_save[k];

        /* clamp — shouldn't be negative but floating point */
        if (sigma2 < 1e-30) sigma2 = 1e-30;

        /* ── PSD evaluation ───────────────────────────────────────── */
        /* TODO: this could probably be vectorized — 2011 */
        for (j = 0; j < NUM_FREQ; j++) {
            double fj = (double)j * (g_fs / 2.0) / (double)(NUM_FREQ - 1);
            double re = 1.0, im = 0.0;

            for (k = 0; k < AR_ORDER; k++) {
                double theta = -2.0 * M_PI * fj * (k + 1) / g_fs;
                re -= solution[k] * cos(theta);
                im -= solution[k] * sin(theta);
            }

            double mag2 = re * re + im * im;
            if (mag2 < 1e-30) mag2 = 1e-30;
            double P = sigma2 / mag2;
            if (P < 1e-30) P = 1e-30;
            psd_buf[j] = 10.0 * log10(P);
        }

        /* ── Write PSD to output array ────────────────────────────── */
        /* The critical section was added in 2019 when someone noticed
           "occasional garbage" in the output.  The garbage was actually
           from the thread-safety bug in the static scratch arrays
           (fixed by then), but the critical section stayed because
           "it can't hurt."  It serialises the only cheap part of the
           loop while the expensive parts (Cholesky, PSD) run in
           parallel — which is backwards, but nobody profiled it. */
        #pragma omp critical
        {
            memcpy(&psd_out[(size_t)w * NUM_FREQ], psd_buf,
                   NUM_FREQ * sizeof(double));
        }
    }

    mat_free(matrix, AR_ORDER);
}


// ════════════════════════════════════════════════════════════════════════════
// Signal generation (same test signal as all other versions)
// ════════════════════════════════════════════════════════════════════════════

static unsigned long long g_rng_state = 123456789ULL;

/* not thread-safe — called only from main before the parallel region */
static double lcg_randn(void)
{
    g_rng_state = g_rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    double u1 = (double)(g_rng_state >> 11) / (double)(1ULL << 53);
    g_rng_state = g_rng_state * 6364136223846793005ULL + 1442695040888963407ULL;
    double u2 = (double)(g_rng_state >> 11) / (double)(1ULL << 53);
    if (u1 < 1e-15) u1 = 1e-15;
    return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
}

static void generate_signal(double *x, int len)
{
    double T = (double)len / g_fs;
    int n;
    for (n = 0; n < len; n++) {
        double t = (double)n / g_fs;
        double chirp = sin(2.0*M_PI*(CHIRP_F0*t + (CHIRP_F1-CHIRP_F0)*t*t/(2.0*T)));
        double env = exp(-0.5*(t-105.0)*(t-105.0)/(2.0*2.0));
        double trans = 1.5 * env * sin(2.0*M_PI*1200.0*t);
        x[n] = chirp + trans + 0.15 * lcg_randn();
    }
}


// ════════════════════════════════════════════════════════════════════════════
//                              MAIN
// ════════════════════════════════════════════════════════════════════════════

int main(int argc, char **argv)
{
    int nthreads = 1;

    (void)argc; (void)argv;

    g_num_windows = 65536;
    g_siglen = (g_num_windows - 1) * WIN_STRIDE + WIN_LEN;

    #ifdef _OPENMP
    /* "use all cores" — the production default since 2014 */
    nthreads = omp_get_max_threads();
    printf("OpenMP: %d threads\n", nthreads);
    #endif

    printf("================================================================\n");
    printf("  AR(%d) Spectral Estimation — ORIGINAL C VERSION\n", AR_ORDER);
    printf("  %d windows, hand-rolled Cholesky, NR double** storage\n",
           g_num_windows);
    printf("  This is the code you inherit.\n");
    printf("================================================================\n\n");

    /* allocate signal */
    double *signal = (double*)malloc(g_siglen * sizeof(double));
    if (!signal) { fprintf(stderr, "malloc: signal\n"); return 1; }

    /* allocate output */
    double *psd_out = (double*)calloc((size_t)g_num_windows * NUM_FREQ,
                                      sizeof(double));
    if (!psd_out) { fprintf(stderr, "malloc: psd_out\n"); free(signal); return 1; }

    printf("  Signal: %d samples (%.1f s at %.0f Hz)\n",
           g_siglen, (double)g_siglen / g_fs, g_fs);
    printf("  Generating ...\n");
    generate_signal(signal, g_siglen);

    /* ── The main processing loop ──────────────────────────────────── */

    printf("  Processing %d windows ...\n\n", g_num_windows);

    struct timespec ts0, ts1;
    clock_gettime(CLOCK_MONOTONIC, &ts0);

    #ifdef _OPENMP
    /* The OpenMP parallelisation (2014).
       Each thread gets a chunk of windows.  Scratch is stack-allocated
       inside process_windows, so it's thread-private.  The matrix
       is malloc'd per call (per thread).  The output write is inside
       #pragma omp critical, which serialises it.

       The original developer wrote this as a parallel for.  The current
       form (manual chunking) was introduced in 2016 when someone tried
       to "fix the load balancing" by splitting into equal chunks.
       It's the same thing but harder to read. */
    #pragma omp parallel
    {
        int tid = omp_get_thread_num();
        int nt  = omp_get_num_threads();
        int chunk = (g_num_windows + nt - 1) / nt;
        int start = tid * chunk;
        int end   = start + chunk;
        if (end > g_num_windows) end = g_num_windows;
        if (start < g_num_windows)
            process_windows(signal, g_siglen, psd_out, start, end);
    }
    #else
    process_windows(signal, g_siglen, psd_out, 0, g_num_windows);
    #endif

    clock_gettime(CLOCK_MONOTONIC, &ts1);
    double elapsed_ms = (ts1.tv_sec - ts0.tv_sec) * 1000.0
                      + (ts1.tv_nsec - ts0.tv_nsec) / 1e6;

    /* ── Results ───────────────────────────────────────────────────── */

    printf("── Results ─────────────────────────────────────────────────────\n");
    printf("  Total time:        %8.1f ms\n", elapsed_ms);
    printf("  Per window:        %8.4f ms\n", elapsed_ms / g_num_windows);
    printf("  Cholesky failures: %d\n", g_chol_failures);
    printf("  Threads:           %d\n", nthreads);

    double dur = (double)g_siglen / g_fs;
    printf("  Signal duration:   %.1f s\n", dur);
    printf("  Real-time factor:  %.1f×\n", dur * 1000.0 / elapsed_ms);

    /* ── Chirp tracking ────────────────────────────────────────────── */

    printf("\n── Chirp tracking ──────────────────────────────────────────────\n");
    {
        double T = (double)g_siglen / g_fs;
        int spots[] = {0, 10000, 32768, 50000, 65535};
        int s;

        printf("  %8s  %8s  %8s  %8s\n", "Window", "Time(s)", "True(Hz)", "Est(Hz)");

        for (s = 0; s < 5; s++) {
            int w = spots[s];
            double tc = ((double)(w * WIN_STRIDE) + WIN_LEN * 0.5) / g_fs;
            double f_true = CHIRP_F0 + (CHIRP_F1 - CHIRP_F0) * tc / T;
            int peak = 0;
            double pv = -1e30;
            int jj;
            for (jj = 1; jj < NUM_FREQ; jj++) {
                double v = psd_out[(size_t)w * NUM_FREQ + jj];
                if (v > pv) { pv = v; peak = jj; }
            }
            double f_est = (double)peak * (g_fs / 2.0) / (double)(NUM_FREQ - 1);
            printf("  %8d  %8.2f  %8.1f  %8.1f\n", w, tc, f_true, f_est);
        }
    }

    printf("\n================================================================\n");
    printf("  MIGRATION OBSTACLES:\n\n");
    printf("  1. double** row-pointer storage — rows are individually\n");
    printf("     malloc'd, scattered across the heap.  Cannot cudaMemcpy.\n\n");
    printf("  2. In-place factorisation — choldc destroys the input.\n");
    printf("     Batching requires all matrices alive simultaneously.\n\n");
    printf("  3. No separation of assembly from solve — the matrix\n");
    printf("     exists only inside the loop body, transiently.\n\n");
    printf("  4. #pragma omp critical on output write — serialises\n");
    printf("     the cheap part while the expensive part is parallel.\n\n");
    printf("  5. Stack-allocated scratch + per-call mat_alloc —\n");
    printf("     each thread does %d malloc/free pairs per chunk.\n\n",
           AR_ORDER + 1);
    printf("  The refactoring path:\n");
    printf("    a. Replace double** with contiguous column-major storage\n");
    printf("    b. Separate matrix assembly from solve\n");
    printf("    c. Accumulate all matrices before solving any\n");
    printf("    d. Replace choldc/cholsl with batched cuSOLVER\n");
    printf("    e. Move windowing + autocovariance + PSD to device\n");
    printf("    f. Wrap boundary with ArrayView / DeviceStaging\n");
    printf("================================================================\n");

    free(signal);
    free(psd_out);

    return 0;
}
