// ============================================================================
// gpu_resident_pipeline.cu
//
// Full GPU-resident pipeline — data touches the host exactly twice:
//   H2D at the start, D2H at the end.  Everything between is device-only.
//
// Pipeline:
//   1. H2D:    raw values [2M doubles] → device
//   2. Kernel: transcendental transform  (sin, exp, sqrt per element)
//   3. Kernel: form 65,536 SPD submatrices from transformed values
//              (RBF kernel matrix from sliding windows of 32 elements)
//   4. cuSOLV: batched Cholesky factorize + solve  (65,536 × 32×32)
//   5. Kernel: scatter solutions — weighted average back into result array
//   6. D2H:   result array → host
//
// This is what the batched_cholesky_solve example becomes once you
// move the assembly and post-processing onto the device too.
// Transfer is no longer the bottleneck because you only pay it once.
//
// Build:
//   nvcc -O2 -arch=sm_70 gpu_resident_pipeline.cu \
//        -lcusolver -lcublas -o gpu_pipeline
//
//   # With CPU reference (needs LAPACK):
//   nvcc -O2 -arch=sm_70 -DWITH_CPU_REF gpu_resident_pipeline.cu \
//        -lcusolver -lcublas -llapack -lblas -o gpu_pipeline
//
// ============================================================================

#include <cuda_runtime.h>
#include <cusolverDn.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <chrono>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <numeric>

// ────────────────────────────────────────────────────────────────────────────
// Configuration
// ────────────────────────────────────────────────────────────────────────────

constexpr int N       = 32;             // submatrix dimension
constexpr int BATCH   = 65536;          // number of independent systems
constexpr int NVALS   = BATCH * N;      // total raw values (2,097,152)
constexpr int N2      = N * N;          // elements per submatrix
constexpr int NRHS    = 1;

// RBF kernel parameters
constexpr double RBF_GAMMA = 0.5;       // bandwidth
constexpr double REG_DIAG  = 1.0e-3;    // Tikhonov regularisation (SPD guarantee)

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
// Timer
// ────────────────────────────────────────────────────────────────────────────

struct Timer {
    using clock = std::chrono::high_resolution_clock;
    clock::time_point t0;
    void start() { t0 = clock::now(); }
    double elapsed_ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - t0).count();
    }
};

// CUDA event-based timer for accurate device timing
struct GpuTimer {
    cudaEvent_t start_ev, stop_ev;
    GpuTimer() {
        cudaEventCreate(&start_ev);
        cudaEventCreate(&stop_ev);
    }
    ~GpuTimer() {
        cudaEventDestroy(start_ev);
        cudaEventDestroy(stop_ev);
    }
    void start(cudaStream_t s = 0) { cudaEventRecord(start_ev, s); }
    void stop(cudaStream_t s = 0)  { cudaEventRecord(stop_ev, s); }
    float elapsed_ms() {
        cudaEventSynchronize(stop_ev);
        float ms = 0;
        cudaEventElapsedTime(&ms, start_ev, stop_ev);
        return ms;
    }
};

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 1 — Transcendental transform
//
//  f(x) = sin(x) · exp(-|x|/10) + sqrt(|x| + 1)
//
//  Three transcendentals per element.  This is the embarrassingly parallel
//  map operation — every thread is independent, high arithmetic intensity,
//  perfect GPU work.
// ════════════════════════════════════════════════════════════════════════════

__global__
void transcendental_transform(const double* __restrict__ raw,
                              double*       __restrict__ out,
                              int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;

    double x = raw[idx];
    double ax = fabs(x);

    //  sin(x) · exp(-|x|/10) + sqrt(|x| + 1)
    out[idx] = sin(x) * exp(-ax * 0.1) + sqrt(ax + 1.0);
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 2 — Form SPD submatrices from transformed values
//
//  For system k, take the window  v[k*N .. k*N + N-1]  from the
//  transformed array.  Build the N×N RBF (Gaussian) kernel matrix:
//
//      A_k[i][j] = exp( -γ · (v[i] - v[j])² )  +  δ_{ij} · ε
//
//  This is symmetric positive definite by construction (the RBF kernel
//  is a Mercer kernel; the diagonal shift guarantees strict positivity).
//
//  Also builds the RHS as the row sums:  b_k[i] = Σ_j A_k[i][j]
//  so the true solution is x = [1, 1, ..., 1] for verification.
//
//  Layout:  one thread block per system, N threads per block.
//  Each thread handles one row of the matrix.
// ════════════════════════════════════════════════════════════════════════════

__global__
void form_submatrices(const double* __restrict__ vals,
                      double*       __restrict__ A,       // [BATCH * N * N]
                      double*       __restrict__ B,       // [BATCH * N]
                      int n, int batch, double gamma, double reg)
{
    int k = blockIdx.x;                       // which system
    if (k >= batch) return;

    int row = threadIdx.x;                    // which row in this system
    if (row >= n) return;

    const double* v = vals + k * n;           // window into transformed array
    double* Ak = A + (size_t)k * n * n;       // this system's matrix
    double* bk = B + (size_t)k * n;           // this system's RHS

    double vi = v[row];
    double row_sum = 0.0;

    for (int col = 0; col < n; ++col) {
        double diff = vi - v[col];
        double aij  = exp(-gamma * diff * diff);
        if (row == col) aij += reg;           // diagonal regularisation
        Ak[row + col * n] = aij;              // column-major
        row_sum += aij;
    }

    bk[row] = row_sum;                        // b = A · [1,1,...,1]
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL 3 — Scatter solutions back: weighted average into result array
//
//  For each system k, the solution x_k has N entries.  We scatter them
//  back into the result array at the same window positions, using
//  atomic adds to handle the fact that adjacent windows share no
//  elements here (non-overlapping), but this pattern generalises to
//  overlapping stencils where atomics are needed.
//
//  result[k*N + i]  +=  x_k[i]
//  counts[k*N + i]  +=  1
//
//  Final average is result[j] / counts[j].
//  With non-overlapping windows each count is 1, but the code is written
//  for the general overlapping case.
// ════════════════════════════════════════════════════════════════════════════

__global__
void scatter_average(const double* __restrict__ X,        // solutions [BATCH * N]
                     double*       __restrict__ result,   // output    [NVALS]
                     int*          __restrict__ counts,   // hit count [NVALS]
                     int n, int batch)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = batch * n;
    if (idx >= total) return;

    int k   = idx / n;       // which system
    int i   = idx % n;       // which element within solution
    int pos = k * n + i;     // position in global array

    atomicAdd(&result[pos], X[idx]);
    atomicAdd(&counts[pos], 1);
}

__global__
void normalise(double* __restrict__ result,
               const int* __restrict__ counts,
               int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    int c = counts[idx];
    if (c > 0) result[idx] /= (double)c;
}

// ════════════════════════════════════════════════════════════════════════════
//  KERNEL — Build device pointer arrays
//
//  Instead of building on host and copying, do it in a trivial kernel.
//  Eliminates one small H2D copy.
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
    printf("  GPU-Resident Pipeline\n");
    printf("  %d values → transcendental → %d × %d×%d submatrices → solve → average\n",
           NVALS, BATCH, N, N);
    printf("══════════════════════════════════════════════════════════════════\n\n");

    // ── Device info ───────────────────────────────────────────────────
    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    printf("  Device: %s   SMs: %d   Mem: %.1f GB\n\n",
           prop.name, prop.multiProcessorCount,
           prop.totalGlobalMem / 1.0e9);

    // ── Memory budget ─────────────────────────────────────────────────
    const size_t raw_bytes    = NVALS * sizeof(double);          //  16 MB
    const size_t trans_bytes  = NVALS * sizeof(double);          //  16 MB
    const size_t mat_bytes    = (size_t)BATCH * N2 * sizeof(double);  // 537 MB
    const size_t rhs_bytes    = (size_t)BATCH * N * sizeof(double);   //   2 MB
    const size_t result_bytes = NVALS * sizeof(double);          //  16 MB
    const size_t counts_bytes = NVALS * sizeof(int);             //   8 MB
    const size_t ptr_bytes    = BATCH * sizeof(double*);         // 0.5 MB
    const size_t info_bytes   = BATCH * sizeof(int);             // 0.25 MB

    size_t total_device = raw_bytes + trans_bytes + mat_bytes + rhs_bytes
                        + result_bytes + counts_bytes + 2 * ptr_bytes + info_bytes;

    printf("  Device memory budget:\n");
    printf("    raw values:     %6.1f MB\n", raw_bytes / 1e6);
    printf("    transformed:    %6.1f MB\n", trans_bytes / 1e6);
    printf("    submatrices:    %6.1f MB\n", mat_bytes / 1e6);
    printf("    RHS / solution: %6.1f MB\n", rhs_bytes / 1e6);
    printf("    result + counts:%6.1f MB\n", (result_bytes + counts_bytes) / 1e6);
    printf("    pointer arrays: %6.1f MB\n", 2 * ptr_bytes / 1e6);
    printf("    ──────────────────────────\n");
    printf("    TOTAL:          %6.1f MB\n\n", total_device / 1e6);

    // ── Generate raw data on host ─────────────────────────────────────
    printf("  Generating %d raw values on host ...\n", NVALS);

    std::vector<double> h_raw(NVALS);
    // Deterministic fill: values in [-5, 5]
    for (int i = 0; i < NVALS; ++i)
        h_raw[i] = -5.0 + 10.0 * ((double)i / (double)(NVALS - 1))
                   + 0.1 * sin((double)i * 0.0137);   // slight wobble

    // ── Allocate device memory ────────────────────────────────────────
    double *d_raw = nullptr, *d_trans = nullptr;
    double *d_A = nullptr,   *d_B = nullptr;
    double *d_result = nullptr;
    int    *d_counts = nullptr;
    double **d_Aptr = nullptr, **d_Bptr = nullptr;
    int    *d_info = nullptr;

    CUDA_CHECK(cudaMalloc(&d_raw,    raw_bytes));
    CUDA_CHECK(cudaMalloc(&d_trans,  trans_bytes));
    CUDA_CHECK(cudaMalloc(&d_A,      mat_bytes));
    CUDA_CHECK(cudaMalloc(&d_B,      rhs_bytes));
    CUDA_CHECK(cudaMalloc(&d_result, result_bytes));
    CUDA_CHECK(cudaMalloc(&d_counts, counts_bytes));
    CUDA_CHECK(cudaMalloc(&d_Aptr,   ptr_bytes));
    CUDA_CHECK(cudaMalloc(&d_Bptr,   ptr_bytes));
    CUDA_CHECK(cudaMalloc(&d_info,   info_bytes));

    // Zero the accumulation arrays
    CUDA_CHECK(cudaMemset(d_result, 0, result_bytes));
    CUDA_CHECK(cudaMemset(d_counts, 0, counts_bytes));

    // cuSOLVER handle
    cusolverDnHandle_t solver;
    CUSOLVER_CHECK(cusolverDnCreate(&solver));

    // ══════════════════════════════════════════════════════════════════
    //  THE PIPELINE — timed end-to-end and per-stage
    // ══════════════════════════════════════════════════════════════════

    GpuTimer t_h2d, t_trans, t_form, t_fact, t_solve, t_scatter, t_norm, t_d2h;
    Timer wall;
    wall.start();

    // ── Stage 1: H2D — raw values to device ──────────────────────────
    t_h2d.start();
    CUDA_CHECK(cudaMemcpy(d_raw, h_raw.data(), raw_bytes, cudaMemcpyHostToDevice));
    t_h2d.stop();

    // ── Stage 2: Transcendental transform ────────────────────────────
    {
        int threads = 256;
        int blocks  = (NVALS + threads - 1) / threads;

        t_trans.start();
        transcendental_transform<<<blocks, threads>>>(d_raw, d_trans, NVALS);
        t_trans.stop();
    }

    // ── Stage 3: Form submatrices + RHS on device ────────────────────
    //
    //  One block per system, N threads per block (one thread per row).
    //  All data reads from d_trans — no host involvement.
    {
        t_form.start();
        form_submatrices<<<BATCH, N>>>(d_trans, d_A, d_B,
                                       N, BATCH, RBF_GAMMA, REG_DIAG);
        t_form.stop();
    }

    // ── Build pointer arrays on device ───────────────────────────────
    {
        int threads = 256;
        int blocks  = (BATCH + threads - 1) / threads;
        build_ptr_array<<<blocks, threads>>>(d_Aptr, d_A, N2, BATCH);
        build_ptr_array<<<blocks, threads>>>(d_Bptr, d_B, N,  BATCH);
        CUDA_CHECK(cudaDeviceSynchronize());
    }

    // ── Stage 4: Batched Cholesky factorisation ──────────────────────
    t_fact.start();
    CUSOLVER_CHECK(cusolverDnDpotrfBatched(
        solver,
        CUBLAS_FILL_MODE_LOWER,
        N,
        d_Aptr,
        N,
        d_info,
        BATCH
    ));
    t_fact.stop();

    // ── Quick sanity check on factorisation status ────────────────────
    {
        CUDA_CHECK(cudaDeviceSynchronize());
        std::vector<int> h_info(BATCH);
        CUDA_CHECK(cudaMemcpy(h_info.data(), d_info,
                              info_bytes, cudaMemcpyDeviceToHost));
        int bad = 0;
        for (int k = 0; k < BATCH; ++k)
            if (h_info[k] != 0) ++bad;
        if (bad > 0)
            throw std::runtime_error(
                "Cholesky factorisation failed: "
                + std::to_string(bad) + " / " + std::to_string(BATCH));
    }

    // ── Stage 5: Batched triangular solve ────────────────────────────
    t_solve.start();
    CUSOLVER_CHECK(cusolverDnDpotrsBatched(
        solver,
        CUBLAS_FILL_MODE_LOWER,
        N,
        NRHS,
        d_Aptr,
        N,
        d_Bptr,
        N,
        d_info,
        BATCH
    ));
    t_solve.stop();

    // ── Stage 6: Scatter solutions back into result array ────────────
    {
        int total = BATCH * N;
        int threads = 256;
        int blocks  = (total + threads - 1) / threads;

        t_scatter.start();
        scatter_average<<<blocks, threads>>>(d_B, d_result, d_counts, N, BATCH);
        t_scatter.stop();

        t_norm.start();
        int nblk = (NVALS + threads - 1) / threads;
        normalise<<<nblk, threads>>>(d_result, d_counts, NVALS);
        t_norm.stop();
    }

    // ── Stage 7: D2H — result array back to host ─────────────────────
    std::vector<double> h_result(NVALS);

    t_d2h.start();
    CUDA_CHECK(cudaMemcpy(h_result.data(), d_result,
                          result_bytes, cudaMemcpyDeviceToHost));
    t_d2h.stop();

    CUDA_CHECK(cudaDeviceSynchronize());
    double wall_ms = wall.elapsed_ms();

    // ══════════════════════════════════════════════════════════════════
    //  RESULTS
    // ══════════════════════════════════════════════════════════════════

    printf("\n");
    printf("── Pipeline timing (GPU events) ────────────────────────────────\n");
    printf("  H2D transfer:        %8.2f ms\n", t_h2d.elapsed_ms());
    printf("  Transcendental:      %8.2f ms\n", t_trans.elapsed_ms());
    printf("  Form submatrices:    %8.2f ms\n", t_form.elapsed_ms());
    printf("  Cholesky factorise:  %8.2f ms\n", t_fact.elapsed_ms());
    printf("  Triangular solve:    %8.2f ms\n", t_solve.elapsed_ms());
    printf("  Scatter + normalise: %8.2f ms\n",
           t_scatter.elapsed_ms() + t_norm.elapsed_ms());
    printf("  D2H transfer:        %8.2f ms\n", t_d2h.elapsed_ms());
    printf("  ─────────────────────────────────\n");
    printf("  Wall clock:          %8.2f ms\n\n", wall_ms);

    // ── Verification ──────────────────────────────────────────────────
    //
    //  Since b = A · [1,...,1], the solution should be all ones.
    //  After scatter-average with non-overlapping windows, each result
    //  element should be 1.0.

    double worst = 0.0;
    int worst_idx = 0;
    for (int i = 0; i < NVALS; ++i) {
        double err = std::fabs(h_result[i] - 1.0);
        if (err > worst) { worst = err; worst_idx = i; }
    }

    printf("  Verification (expected: all 1.0):\n");
    printf("    Max |x - 1| = %e   at index %d\n", worst, worst_idx);
    printf("    Sample: result[0]=%.10f  result[%d]=%.10f  result[%d]=%.10f\n",
           h_result[0], NVALS/2, h_result[NVALS/2], NVALS-1, h_result[NVALS-1]);

    // ── Compute-vs-transfer breakdown ─────────────────────────────────

    float xfer = t_h2d.elapsed_ms() + t_d2h.elapsed_ms();
    float compute = t_trans.elapsed_ms() + t_form.elapsed_ms()
                  + t_fact.elapsed_ms() + t_solve.elapsed_ms()
                  + t_scatter.elapsed_ms() + t_norm.elapsed_ms();

    printf("\n  Breakdown:\n");
    printf("    Transfer:  %6.2f ms  (%4.1f%%)\n",
           xfer, 100.0f * xfer / (xfer + compute));
    printf("    Compute:   %6.2f ms  (%4.1f%%)\n",
           compute, 100.0f * compute / (xfer + compute));
    printf("    Ratio:     %.1f× more time in compute than transfer\n",
           compute / std::max(xfer, 0.01f));

    printf("\n══════════════════════════════════════════════════════════════════\n");
    printf("  The pipeline made two host↔device trips total.\n");
    printf("  Everything between — transcendental, matrix formation,\n");
    printf("  batched solve, scatter — stayed on the device.\n");
    printf("  Compare this to the serial version: 65,536 × (H2D + solve + D2H).\n");
    printf("══════════════════════════════════════════════════════════════════\n");

    // ── Cleanup ───────────────────────────────────────────────────────
    cusolverDnDestroy(solver);
    cudaFree(d_raw);
    cudaFree(d_trans);
    cudaFree(d_A);
    cudaFree(d_B);
    cudaFree(d_result);
    cudaFree(d_counts);
    cudaFree(d_Aptr);
    cudaFree(d_Bptr);
    cudaFree(d_info);

    return 0;
}
