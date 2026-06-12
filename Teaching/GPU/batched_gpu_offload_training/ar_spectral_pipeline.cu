// ============================================================================
// ar_spectral_pipeline.cu
//
// Moving-Window AR Spectral Estimation — Full GPU-Resident Pipeline
//
// Estimates a time-frequency representation of a non-stationary signal
// by fitting AR(p) models to overlapping windows and evaluating the
// parametric PSD at each window position.
//
// Pipeline (six stages, two host transfers):
//   1. H2D:    raw signal → device
//   2. Kernel: Hann windowing (transcendental: cos)
//   3. Kernel: autocovariance estimation + Toeplitz matrix assembly
//   4. cuSOLV: batched Cholesky solve — Yule-Walker equations
//   5. Kernel: PSD evaluation — AR coefficients → P(f) at each (window, freq)
//   6. D2H:   time-frequency matrix → host
//
// Build:
//   nvcc -O2 -arch=sm_70 ar_spectral_pipeline.cu \
//        -lcusolver -o ar_spectral
//
// See ar_spectral_estimation_theory.md for the physics and mathematics.
// ============================================================================

#include <cuda_runtime.h>
#include <cusolverDn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ────────────────────────────────────────────────────────────────────────────
// Configuration
// ────────────────────────────────────────────────────────────────────────────

constexpr int    ORDER  = 32;           // AR model order (= matrix dimension)
constexpr int    WLEN   = 128;          // window length in samples
constexpr int    STRIDE = 32;           // window stride (75% overlap)
constexpr int    BATCH  = 65536;        // number of windows
constexpr int    NFREQ  = 256;          // frequency bins (0 to fs/2)
constexpr int    NRHS   = 1;
constexpr int    N2     = ORDER * ORDER;

// Signal parameters
constexpr double FS       = 10000.0;    // sample rate (Hz)
constexpr double CHIRP_F0 = 200.0;     // chirp start frequency (Hz)
constexpr double CHIRP_F1 = 2000.0;    // chirp end frequency (Hz)
constexpr double TRANS_F  = 1200.0;    // transient burst frequency (Hz)
constexpr double TRANS_T0 = 105.0;     // transient centre time (s)
constexpr double TRANS_SD = 2.0;       // transient Gaussian width (s)
constexpr double TRANS_A  = 1.5;       // transient amplitude
constexpr double NOISE_SD = 0.15;      // noise standard deviation

// Derived
constexpr int SIGLEN = (BATCH - 1) * STRIDE + WLEN;   // 2,097,248 samples

// ────────────────────────────────────────────────────────────────────────────
// Error-checking macros
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
// Timing
// ────────────────────────────────────────────────────────────────────────────

struct GpuTimer {
    cudaEvent_t e0, e1;
    GpuTimer()  { cudaEventCreate(&e0); cudaEventCreate(&e1); }
    ~GpuTimer() { cudaEventDestroy(e0); cudaEventDestroy(e1); }
    void start(cudaStream_t s = 0) { cudaEventRecord(e0, s); }
    void stop(cudaStream_t s = 0)  { cudaEventRecord(e1, s); }
    float ms() { cudaEventSynchronize(e1); float t; cudaEventElapsedTime(&t, e0, e1); return t; }
};

// ════════════════════════════════════════════════════════════════════════════
//  HOST: Generate test signal
//
//  Three components with known time-frequency content:
//    1. Linear chirp  f₀ → f₁
//    2. Gaussian-modulated sinusoidal transient at f_t
//    3. White noise
// ════════════════════════════════════════════════════════════════════════════

static void generate_signal(double* x, int len)
{
    double T = (double)len / FS;            // total duration (seconds)

    // Simple LCG for reproducible noise
    uint64_t rng = 123456789ULL;
    auto randn = [&]() -> double {          // Box-Muller
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        double u1 = (double)(rng >> 11) / (double)(1ULL << 53);
        rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
        double u2 = (double)(rng >> 11) / (double)(1ULL << 53);
        u1 = fmax(u1, 1e-15);
        return sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    };

    for (int n = 0; n < len; ++n) {
        double t = (double)n / FS;

        // 1. Linear chirp: instantaneous phase = 2π(f₀t + (f₁−f₀)t²/(2T))
        double chirp_phase = 2.0 * M_PI * (CHIRP_F0 * t
                           + (CHIRP_F1 - CHIRP_F0) * t * t / (2.0 * T));
        double chirp = sin(chirp_phase);

        // 2. Gaussian-modulated transient at f_t
        double env = exp(-0.5 * (t - TRANS_T0) * (t - TRANS_T0)
                         / (TRANS_SD * TRANS_SD));
        double transient = TRANS_A * env * sin(2.0 * M_PI * TRANS_F * t);

        // 3. White noise
        double noise = NOISE_SD * randn();

        x[n] = chirp + transient + noise;
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 1 — Hann windowing (the transcendental stage)
//
//  For each window w, compute:  x̃_w[n] = x[w·S + n] · 0.5·(1 − cos(2πn/(W−1)))
//
//  Output: d_xw[w * WLEN + n] = windowed sample
//
//  Grid:  BATCH blocks × WLEN threads
// ════════════════════════════════════════════════════════════════════════════

__global__
void hann_window_kernel(const double* __restrict__ sig,
                        double*       __restrict__ xw,
                        int wlen, int stride, int batch)
{
    int w = blockIdx.x;
    int n = threadIdx.x;
    if (w >= batch || n >= wlen) return;

    int sig_idx = w * stride + n;
    double hann = 0.5 * (1.0 - cos(2.0 * M_PI * (double)n / (double)(wlen - 1)));

    xw[(size_t)w * wlen + n] = sig[sig_idx] * hann;
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 2 — Autocovariance estimation + Toeplitz matrix assembly
//
//  For each window w:
//    1. Compute biased autocovariance r[k] = (1/W) Σ_{n=k}^{W-1} x̃[n]·x̃[n−k]
//       for k = 0, …, p.
//    2. Assemble p×p Toeplitz matrix:  R[i][j] = r[|i−j|]  (column-major)
//    3. Assemble RHS vector:           b[i] = r[i+1]
//
//  Grid:  BATCH blocks × ORDER threads
//  Each thread handles one row of the matrix.
//  Autocovariance lags computed cooperatively (each thread does one lag,
//  stored in shared memory, then all threads read to fill their rows).
// ════════════════════════════════════════════════════════════════════════════

__global__
void autocov_and_assemble(const double* __restrict__ xw,      // [BATCH × WLEN]
                          double*       __restrict__ A,        // [BATCH × ORDER × ORDER]
                          double*       __restrict__ B,        // [BATCH × ORDER]
                          double*       __restrict__ r0_out,   // [BATCH] — r(0) for σ²
                          int p, int wlen, int batch)
{
    int w   = blockIdx.x;                    // which window
    int row = threadIdx.x;                   // this thread handles lag `row` then row `row`
    if (w >= batch || row > p) return;       // need p+1 lags, p rows

    const double* xw_w = xw + (size_t)w * wlen;

    // ── Step 1: compute autocovariance lag `row` ──────────────────────
    //    r[k] = (1/W) Σ_{n=k}^{W-1} x[n] · x[n-k]

    extern __shared__ double s_r[];          // shared: p+1 doubles

    double rk = 0.0;
    if (row <= p) {
        for (int n = row; n < wlen; ++n)
            rk += xw_w[n] * xw_w[n - row];
        rk /= (double)wlen;
        s_r[row] = rk;
    }

    __syncthreads();

    // ── Save r(0) for innovation variance computation later ───────────
    if (row == 0)
        r0_out[w] = s_r[0];

    // ── Step 2: assemble Toeplitz row + RHS ──────────────────────────
    if (row < p) {
        double* Aw = A + (size_t)w * p * p;
        double* Bw = B + (size_t)w * p;

        // Matrix row: R[row][col] = r[|row - col|]   (column-major: Aw[row + col*p])
        // Diagonal loading: R += ε·I guarantees strict positive definiteness
        // even for degenerate windows (dead signal, DAQ dropout).  The relative
        // term scales with signal energy; the absolute term catches r(0) ≈ 0.
        for (int col = 0; col < p; ++col) {
            int lag = (row >= col) ? (row - col) : (col - row);
            double val = s_r[lag];
            if (row == col)
                val += 1.0e-10 * s_r[0] + 1.0e-14;
            Aw[row + col * p] = val;
        }

        // RHS: b[row] = r[row + 1]
        Bw[row] = s_r[row + 1];
    }
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 3 — Innovation variance
//
//  σ²_w = r(0) − Σₖ aₖ · r(k+1)
//
//  But after the Cholesky solve, B holds the AR coefficients a,
//  and we stored r(0) in r0_out.  We need r(1)…r(p) again.
//
//  Simpler approach: recompute σ² = r(0) − aᵀb_original.
//  Since the solve overwrote b with the solution a, and we want
//  σ² = r(0) − aᵀ·r, we need either:
//    (a) save the original RHS before the solve, or
//    (b) recompute r(1..p) from xw.
//
//  We take option (a): duplicate the RHS before the solve,
//  then this kernel computes the dot product.
//
//  Grid:  (BATCH + 255) / 256 blocks × 256 threads
// ════════════════════════════════════════════════════════════════════════════

__global__
void compute_sigma2(const double* __restrict__ ar_coeff,   // [BATCH × ORDER]  (a)
                    const double* __restrict__ rhs_orig,   // [BATCH × ORDER]  (r)
                    const double* __restrict__ r0,         // [BATCH]
                    double*       __restrict__ sigma2,     // [BATCH]
                    int p, int batch)
{
    int w = blockIdx.x * blockDim.x + threadIdx.x;
    if (w >= batch) return;

    const double* a = ar_coeff + (size_t)w * p;
    const double* r = rhs_orig + (size_t)w * p;

    double dot = 0.0;
    for (int k = 0; k < p; ++k)
        dot += a[k] * r[k];

    sigma2[w] = fmax(r0[w] - dot, 1.0e-30);
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 4 — PSD evaluation
//
//  For each (window w, frequency bin j):
//
//    f_j = j · (f_s / 2) / (NFREQ − 1)
//
//    A(f_j) = 1 − Σₖ aₖ · e^{−j2πf_j·k/f_s}
//    P(f_j) = σ² / |A(f_j)|²
//
//  Output:  d_psd[w * NFREQ + j] = 10·log10(P) in dB
//
//  Grid: 2D — (BATCH, ceil(NFREQ/32)) blocks × (1, 32) threads
//  Each thread computes one (window, frequency) pair.
// ════════════════════════════════════════════════════════════════════════════

__global__
void evaluate_psd(const double* __restrict__ ar_coeff,   // [BATCH × ORDER]
                  const double* __restrict__ sigma2,     // [BATCH]
                  double*       __restrict__ psd,        // [BATCH × NFREQ]
                  int p, int nfreq, int batch, double fs)
{
    int w = blockIdx.x;
    int j = blockIdx.y * blockDim.y + threadIdx.y;
    if (w >= batch || j >= nfreq) return;

    double fj = (double)j * (fs * 0.5) / (double)(nfreq - 1);

    const double* a = ar_coeff + (size_t)w * p;

    // Evaluate A(f_j) = 1 − Σ_k a_k · exp(−j·2π·f_j·k / f_s)
    double re_A = 1.0;
    double im_A = 0.0;

    for (int k = 0; k < p; ++k) {
        double theta = -2.0 * M_PI * fj * (double)(k + 1) / fs;
        re_A -= a[k] * cos(theta);
        im_A -= a[k] * sin(theta);
    }

    double mag2 = re_A * re_A + im_A * im_A;

    // P(f) = σ² / |A(f)|²,  output in dB
    double P = sigma2[w] / fmax(mag2, 1e-30);
    psd[(size_t)w * nfreq + j] = 10.0 * log10(fmax(P, 1e-30));
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL — Build pointer arrays on device
// ════════════════════════════════════════════════════════════════════════════

__global__
void build_ptr_array(double** ptrs, double* base, int stride, int batch)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= batch) return;
    ptrs[k] = base + (size_t)k * stride;
}

// ════════════════════════════════════════════════════════════════════════════
//                                MAIN
// ════════════════════════════════════════════════════════════════════════════

int main()
{
    printf("══════════════════════════════════════════════════════════════════\n");
    printf("  Moving-Window AR(%d) Spectral Estimation — GPU Pipeline\n", ORDER);
    printf("  %d windows × %d samples, stride %d, %d freq bins\n",
           BATCH, WLEN, STRIDE, NFREQ);
    printf("  Signal: %d samples (%.1f s at %.0f Hz)\n",
           SIGLEN, (double)SIGLEN / FS, FS);
    printf("══════════════════════════════════════════════════════════════════\n\n");

    // ── Device info ───────────────────────────────────────────────────
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    printf("  Device: %s   SMs: %d   Mem: %.1f GB\n\n",
           prop.name, prop.multiProcessorCount, prop.totalGlobalMem / 1e9);

    // ── Memory budget ─────────────────────────────────────────────────
    const size_t sig_bytes  = SIGLEN * sizeof(double);
    const size_t xw_bytes   = (size_t)BATCH * WLEN * sizeof(double);
    const size_t mat_bytes  = (size_t)BATCH * N2 * sizeof(double);
    const size_t rhs_bytes  = (size_t)BATCH * ORDER * sizeof(double);
    const size_t r0_bytes   = BATCH * sizeof(double);
    const size_t sig2_bytes = BATCH * sizeof(double);
    const size_t psd_bytes  = (size_t)BATCH * NFREQ * sizeof(double);
    const size_t ptr_bytes  = BATCH * sizeof(double*);
    const size_t info_bytes = BATCH * sizeof(int);

    size_t total = sig_bytes + xw_bytes + mat_bytes + 2 * rhs_bytes   // A, B, B_orig
                 + r0_bytes + sig2_bytes + psd_bytes + 2 * ptr_bytes + info_bytes;

    printf("  Device memory:\n");
    printf("    signal:         %6.1f MB\n", sig_bytes / 1e6);
    printf("    windowed:       %6.1f MB\n", xw_bytes / 1e6);
    printf("    matrices:       %6.1f MB\n", mat_bytes / 1e6);
    printf("    RHS (×2):       %6.1f MB\n", 2 * rhs_bytes / 1e6);
    printf("    PSD matrix:     %6.1f MB\n", psd_bytes / 1e6);
    printf("    TOTAL:          %6.1f MB\n\n", total / 1e6);

    // ── Generate test signal on host ──────────────────────────────────
    printf("  Generating test signal (chirp %.0f→%.0f Hz + transient %.0f Hz) ...\n",
           CHIRP_F0, CHIRP_F1, TRANS_F);

    std::vector<double> h_sig(SIGLEN);
    generate_signal(h_sig.data(), SIGLEN);

    // ── Allocate device memory ────────────────────────────────────────
    double *d_sig = nullptr, *d_xw = nullptr;
    double *d_A = nullptr, *d_B = nullptr, *d_B_orig = nullptr;
    double *d_r0 = nullptr, *d_sigma2 = nullptr;
    double *d_psd = nullptr;
    double **d_Aptr = nullptr, **d_Bptr = nullptr;
    int    *d_info = nullptr;

    CUDA_CHECK(cudaMalloc(&d_sig,    sig_bytes));
    CUDA_CHECK(cudaMalloc(&d_xw,     xw_bytes));
    CUDA_CHECK(cudaMalloc(&d_A,      mat_bytes));
    CUDA_CHECK(cudaMalloc(&d_B,      rhs_bytes));
    CUDA_CHECK(cudaMalloc(&d_B_orig, rhs_bytes));
    CUDA_CHECK(cudaMalloc(&d_r0,     r0_bytes));
    CUDA_CHECK(cudaMalloc(&d_sigma2, sig2_bytes));
    CUDA_CHECK(cudaMalloc(&d_psd,    psd_bytes));
    CUDA_CHECK(cudaMalloc(&d_Aptr,   ptr_bytes));
    CUDA_CHECK(cudaMalloc(&d_Bptr,   ptr_bytes));
    CUDA_CHECK(cudaMalloc(&d_info,   info_bytes));

    // cuSOLVER handle
    cusolverDnHandle_t solver;
    CUSOLVER_CHECK(cusolverDnCreate(&solver));

    // ══════════════════════════════════════════════════════════════════
    //  THE PIPELINE
    // ══════════════════════════════════════════════════════════════════

    GpuTimer t_h2d, t_hann, t_acov, t_fact, t_solve, t_sig2, t_psd, t_d2h;

    printf("\n── Pipeline ────────────────────────────────────────────────────\n");

    // ── Stage 1: H2D ──────────────────────────────────────────────────
    t_h2d.start();
    CUDA_CHECK(cudaMemcpy(d_sig, h_sig.data(), sig_bytes, cudaMemcpyHostToDevice));
    t_h2d.stop();

    // ── Stage 2: Hann windowing ───────────────────────────────────────
    t_hann.start();
    hann_window_kernel<<<BATCH, WLEN>>>(d_sig, d_xw, WLEN, STRIDE, BATCH);
    t_hann.stop();

    // ── Stage 3: Autocovariance + Toeplitz assembly ───────────────────
    //
    //  Block:  ORDER+1 threads (need lag 0..ORDER)
    //  Shared: (ORDER+1) doubles for autocovariance lags
    {
        int smem = (ORDER + 1) * sizeof(double);

        t_acov.start();
        autocov_and_assemble<<<BATCH, ORDER + 1, smem>>>(
            d_xw, d_A, d_B, d_r0,
            ORDER, WLEN, BATCH);
        t_acov.stop();
    }

    // ── Save original RHS for σ² computation ──────────────────────────
    CUDA_CHECK(cudaMemcpy(d_B_orig, d_B, rhs_bytes, cudaMemcpyDeviceToDevice));

    // ── Build pointer arrays on device ────────────────────────────────
    {
        int thr = 256;
        int blk = (BATCH + thr - 1) / thr;
        build_ptr_array<<<blk, thr>>>(d_Aptr, d_A, N2,    BATCH);
        build_ptr_array<<<blk, thr>>>(d_Bptr, d_B, ORDER, BATCH);
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    // ── Stage 4a: Batched Cholesky factorisation ──────────────────────
    t_fact.start();
    CUSOLVER_CHECK(cusolverDnDpotrfBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, d_Aptr, ORDER, d_info, BATCH));
    t_fact.stop();

    // ── Check factorisations ──────────────────────────────────────────
    //  Diagonal loading in the assembly kernel guarantees strict SPD
    //  for any non-degenerate window.  A failure here means something
    //  is structurally wrong — do not proceed with garbage factors.
    {
        CUDA_CHECK(cudaDeviceSynchronize());
        std::vector<int> h_info(BATCH);
        CUDA_CHECK(cudaMemcpy(h_info.data(), d_info, info_bytes, cudaMemcpyDeviceToHost));
        int bad = 0;
        for (int k = 0; k < BATCH; ++k)
            if (h_info[k] != 0) ++bad;
        if (bad > 0)
            throw std::runtime_error(
                "Cholesky factorisation failed: "
                + std::to_string(bad) + " / " + std::to_string(BATCH));
        printf("  All %d factorisations succeeded\n", BATCH);
    }

    // ── Stage 4b: Batched triangular solve ────────────────────────────
    //  After this, d_B contains the AR coefficients a₁…aₚ for each window.
    t_solve.start();
    CUSOLVER_CHECK(cusolverDnDpotrsBatched(
        solver, CUBLAS_FILL_MODE_LOWER,
        ORDER, NRHS, d_Aptr, ORDER, d_Bptr, ORDER, d_info, BATCH));
    t_solve.stop();

    // ── Stage 4c: Innovation variance σ² ──────────────────────────────
    {
        int thr = 256;
        int blk = (BATCH + thr - 1) / thr;

        t_sig2.start();
        compute_sigma2<<<blk, thr>>>(d_B, d_B_orig, d_r0, d_sigma2,
                                     ORDER, BATCH);
        t_sig2.stop();
    }

    // ── Stage 5: PSD evaluation ───────────────────────────────────────
    //
    //  2D grid: x = window index (BATCH blocks), y = frequency bin
    //  Block: (1, 32) — 32 frequency bins per block
    {
        dim3 block(1, 32);
        dim3 grid(BATCH, (NFREQ + 31) / 32);

        t_psd.start();
        evaluate_psd<<<grid, block>>>(d_B, d_sigma2, d_psd,
                                      ORDER, NFREQ, BATCH, FS);
        t_psd.stop();
    }

    // ── Stage 6: D2H — retrieve PSD matrix ───────────────────────────
    std::vector<double> h_psd(BATCH * NFREQ);

    t_d2h.start();
    CUDA_CHECK(cudaMemcpy(h_psd.data(), d_psd, psd_bytes, cudaMemcpyDeviceToHost));
    t_d2h.stop();

    CUDA_CHECK(cudaDeviceSynchronize());

    // ══════════════════════════════════════════════════════════════════
    //  TIMING REPORT
    // ══════════════════════════════════════════════════════════════════

    float ms_h2d  = t_h2d.ms();
    float ms_hann = t_hann.ms();
    float ms_acov = t_acov.ms();
    float ms_fact = t_fact.ms();
    float ms_solv = t_solve.ms();
    float ms_sig2 = t_sig2.ms();
    float ms_psd  = t_psd.ms();
    float ms_d2h  = t_d2h.ms();

    float ms_xfer = ms_h2d + ms_d2h;
    float ms_comp = ms_hann + ms_acov + ms_fact + ms_solv + ms_sig2 + ms_psd;
    float ms_total = ms_xfer + ms_comp;

    printf("\n── Timing (GPU events) ─────────────────────────────────────────\n");
    printf("  H2D transfer:        %8.2f ms\n", ms_h2d);
    printf("  Hann windowing:      %8.2f ms\n", ms_hann);
    printf("  Autocov + assembly:  %8.2f ms\n", ms_acov);
    printf("  Cholesky factorise:  %8.2f ms\n", ms_fact);
    printf("  Triangular solve:    %8.2f ms\n", ms_solv);
    printf("  Innovation σ²:      %8.2f ms\n", ms_sig2);
    printf("  PSD evaluation:      %8.2f ms\n", ms_psd);
    printf("  D2H transfer:        %8.2f ms\n", ms_d2h);
    printf("  ──────────────────────────────────\n");
    printf("  Transfer:            %8.2f ms  (%4.1f%%)\n",
           ms_xfer, 100.f * ms_xfer / ms_total);
    printf("  Compute:             %8.2f ms  (%4.1f%%)\n",
           ms_comp, 100.f * ms_comp / ms_total);
    printf("  TOTAL:               %8.2f ms\n", ms_total);

    // ══════════════════════════════════════════════════════════════════
    //  VERIFICATION — chirp frequency tracking
    //
    //  For each window, find the PSD peak frequency and compare to the
    //  known chirp instantaneous frequency at that window's centre time.
    // ══════════════════════════════════════════════════════════════════

    printf("\n── Verification: chirp tracking ────────────────────────────────\n");

    double T = (double)SIGLEN / FS;
    double peak_err_sum = 0.0;
    double peak_err_max = 0.0;
    int    n_tracked = 0;

    // Print a few representative windows
    printf("\n  %8s  %8s  %8s  %8s\n",
           "Window", "Time(s)", "True(Hz)", "Est(Hz)");
    printf("  %8s  %8s  %8s  %8s\n",
           "------", "-------", "--------", "-------");

    int print_indices[] = {0, 1000, 5000, 10000, 20000, 32768,
                           40000, 50000, 60000, 65535};

    for (int w = 0; w < BATCH; ++w) {
        double t_centre = ((double)(w * STRIDE) + (double)WLEN * 0.5) / FS;

        // Known chirp frequency at this time
        double f_true = CHIRP_F0 + (CHIRP_F1 - CHIRP_F0) * t_centre / T;

        // Find PSD peak
        int peak_bin = 0;
        double peak_val = -1e30;
        for (int j = 1; j < NFREQ; ++j) {   // skip DC
            double v = h_psd[w * NFREQ + j];
            if (v > peak_val) { peak_val = v; peak_bin = j; }
        }
        double f_est = (double)peak_bin * (FS * 0.5) / (double)(NFREQ - 1);

        // Track error (only where chirp is the dominant component —
        // exclude the transient region)
        double dt = fabs(t_centre - TRANS_T0);
        if (dt > 3.0 * TRANS_SD) {          // outside transient envelope
            double err = fabs(f_est - f_true);
            peak_err_sum += err;
            peak_err_max = fmax(peak_err_max, err);
            ++n_tracked;
        }

        // Print selected rows
        for (int pi = 0; pi < 10; ++pi) {
            if (w == print_indices[pi]) {
                printf("  %8d  %8.2f  %8.1f  %8.1f%s\n",
                       w, t_centre, f_true, f_est,
                       (dt <= 3.0 * TRANS_SD) ? "  [transient]" : "");
                break;
            }
        }
    }

    printf("\n  Chirp tracking (outside transient region):\n");
    printf("    Mean |f_est − f_true| = %.1f Hz\n",
           peak_err_sum / fmax(n_tracked, 1));
    printf("    Max  |f_est − f_true| = %.1f Hz\n", peak_err_max);
    printf("    Tracked windows:        %d / %d\n\n", n_tracked, BATCH);

    // ── Print PSD snapshot at a few windows ───────────────────────────

    printf("── PSD snapshot (dB) at window 32768 (t ≈ %.1f s) ──────────────\n",
           ((double)(32768 * STRIDE) + WLEN * 0.5) / FS);
    printf("  %8s  %10s\n", "Freq(Hz)", "PSD(dB)");
    for (int j = 0; j < NFREQ; j += 16) {
        double f = (double)j * (FS * 0.5) / (double)(NFREQ - 1);
        printf("  %8.1f  %10.2f\n", f, h_psd[32768 * NFREQ + j]);
    }

    // ══════════════════════════════════════════════════════════════════
    //  SUMMARY
    // ══════════════════════════════════════════════════════════════════

    printf("\n══════════════════════════════════════════════════════════════════\n");
    printf("  Pipeline processed %.1f seconds of signal in %.1f ms.\n",
           T, ms_total);
    printf("  Real-time factor: %.0f×  (pipeline runs %.0f× faster than real time)\n",
           (T * 1000.0) / ms_total, (T * 1000.0) / ms_total);
    printf("\n");
    printf("  Six stages, two host↔device transfers.\n");
    printf("  The spectrogram (%.1f MB) is the deliverable.\n", psd_bytes / 1e6);
    printf("══════════════════════════════════════════════════════════════════\n");

    // ── Cleanup ───────────────────────────────────────────────────────
    cusolverDnDestroy(solver);
    cudaFree(d_sig);    cudaFree(d_xw);
    cudaFree(d_A);      cudaFree(d_B);     cudaFree(d_B_orig);
    cudaFree(d_r0);     cudaFree(d_sigma2);
    cudaFree(d_psd);
    cudaFree(d_Aptr);   cudaFree(d_Bptr);
    cudaFree(d_info);

    return 0;
}
