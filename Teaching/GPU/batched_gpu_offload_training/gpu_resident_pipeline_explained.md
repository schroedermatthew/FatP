# The GPU-Resident Pipeline: A Complete Walkthrough

*A line-by-line and concept-by-concept reading of `gpu_resident_pipeline.cu`*

---

## 0. What this program is, in one paragraph

The program takes 2,097,152 raw `double` values, pushes them to the GPU **once**, and then performs an entire multi-stage numerical workload — a transcendental element-wise transform, the assembly of 65,536 small symmetric-positive-definite matrices, a batched Cholesky factorization and solve of all of them, and a scatter-average reduction of the results — without ever bringing intermediate data back to the host. Only at the very end does the result array return to the CPU. The point of the exercise is not the particular math (which is synthetic and self-verifying) but the **architecture**: data crosses the PCIe bus exactly twice, so the slow bus stops being the bottleneck and the GPU's compute throughput is allowed to dominate. This document explains every stage and the technique behind it.

---

## 1. The thesis: why "host touches data twice" is the whole point

### 1.1 Two very different pipes

A discrete GPU lives at the end of a comparatively narrow pipe. The numbers tell the story (figures are typical for the `sm_70` / V100-class target this code builds for):

| Path | Bandwidth (typical) |
|---|---|
| Host → Device over PCIe 3.0 ×16 | ~12–16 GB/s |
| On-device global memory (V100 HBM2) | ~900 GB/s |

That is a **50–70× gap**. Every byte that crosses PCIe costs roughly fifty to seventy times what the same byte costs to move inside the device. Worse, each transfer also pays a fixed latency and (without pinned memory) a hidden staging copy. The first law of GPU performance engineering follows directly: *move data across the bus as few times as possible, and once it is on the device, keep it there until you are completely done with it.*

### 1.2 The anti-pattern this design replaces

The naïve way to solve 65,536 small linear systems is a loop:

```
for k in 0 .. 65535:
    H2D   copy A_k, b_k to device      // bus crossing
    solve A_k x_k = b_k on device      // tiny amount of compute
    D2H   copy x_k back to host        // bus crossing
```

This pays **131,072 bus crossings**, each one a launch + transfer + synchronize round trip, and each one moving an amount of data (a 32×32 matrix, 8 KB) so small that the transfer is pure latency with no chance to amortize. The compute kernel in the middle is trivial; the program spends essentially all of its wall-clock time stalled on the bus and on launch overhead. This is the textbook *transfer-bound* regime, and it is the regime where people conclude "the GPU isn't worth it" — when in reality they simply structured the work so that the GPU never got to do any.

### 1.3 What the resident pipeline does instead

```
H2D   copy all raw data once                 // 1 bus crossing
  transcendental transform   (on device)
  form 65,536 matrices       (on device)
  batched Cholesky factor    (on device)
  batched triangular solve   (on device)
  scatter + normalise        (on device)
D2H   copy result once                        // 1 bus crossing
```

Two crossings total, regardless of batch size. The data is *resident* on the device for the entire computational lifetime. Because the matrix assembly and the post-processing also run on the GPU, there is never a reason to ship an intermediate back to the CPU. The transfer cost becomes a fixed, one-time tax that the program reports at the end as a single-digit-percent slice of the runtime — and the rest is genuine compute, which is exactly what you bought the accelerator for.

This is the lesson the file's header comment is making: *"This is what the batched\_cholesky\_solve example becomes once you move the assembly and post-processing onto the device too."* The batched solve alone is good; the batched solve embedded in a fully resident pipeline is the real win.

---

## 2. The CUDA execution model (background you need to read the rest)

Every kernel launch in this file is a line like `kernel<<<blocks, threads>>>(...)`, and you cannot reason about any of the stages without understanding what those two numbers mean.

### 2.1 Threads, warps, blocks, grids

CUDA executes under a **SIMT** model — Single Instruction, Multiple Thread. The hierarchy, from smallest to largest:

- **Thread** — the unit of work. Runs the kernel body once, identified by its indices.
- **Warp** — 32 threads that execute *in lockstep*. The hardware does not schedule threads individually; it schedules warps. This 32 is not a tunable; it is baked into the architecture, and it is not a coincidence that this program's submatrix dimension `N` is also 32 (more on that in §6.2).
- **Block** (a "thread block") — a group of threads (here 256 or 32) that are guaranteed to run on the same Streaming Multiprocessor (SM), can share fast on-chip *shared memory*, and can synchronize with each other via `__syncthreads()`. Threads in different blocks cannot synchronize cheaply and cannot assume anything about each other's timing.
- **Grid** — the full collection of blocks launched by one `<<<...>>>` call.

A thread finds its global position with the canonical idiom that appears in nearly every kernel here:

```cpp
int idx = blockIdx.x * blockDim.x + threadIdx.x;
```

`blockDim.x` is the threads-per-block, `blockIdx.x` is which block you are in, `threadIdx.x` is your lane within the block. Multiply and add and you get a unique global index. Because the grid is usually launched slightly larger than the data (you round *up* when dividing), every kernel guards with `if (idx >= n) return;` to kill the over-hang threads.

### 2.2 The launch-configuration arithmetic

The recurring pattern

```cpp
int threads = 256;
int blocks  = (NVALS + threads - 1) / threads;
```

is integer **ceiling division**: it computes ⌈NVALS / 256⌉. With `NVALS = 2,097,152` and 256 threads this is exactly 8,192 blocks, and 8192 × 256 = 2,097,152, so it divides evenly and no threads are wasted — but the `+ threads - 1` and the in-kernel guard make the code correct even when it does not divide evenly. 256 is a conventional, safe block size: a multiple of the warp size (8 warps per block), large enough to hide memory latency through occupancy, small enough to allow many blocks to be resident per SM.

### 2.3 Launches are asynchronous — this is why there are two kinds of timer

A kernel launch **returns to the host immediately**. The CPU queues the work into a *stream* and keeps going; the GPU executes whenever it gets to it. This single fact has two consequences that the code handles explicitly:

1. You cannot time a kernel by wrapping a host clock around the launch — you would be timing the *enqueue*, which takes microseconds, not the *execution*. This is why the program uses **CUDA events** (§5.2) for per-stage timing: events are markers placed *into the stream* and timed on the device itself.
2. Before you read any result on the host, you must `cudaDeviceSynchronize()` to wait for the queued work to actually finish. The program does this before reading `d_info` after factorization, and again at the very end before reading the wall-clock time.

Everything here runs on the **default stream (stream 0)**, which serializes: each kernel waits for the previous one to complete. That is why the stages can hand data to each other through device buffers without explicit synchronization between them — the stream ordering guarantees stage *N* finishes before stage *N+1* starts.

---

## 3. The memory model and the device budget

### 3.1 Global memory and coalescing

All the big buffers (`d_raw`, `d_trans`, `d_A`, `d_B`, `d_result`, `d_counts`) live in **global memory** — the multi-gigabyte HBM2 pool. Global memory is fast in aggregate (~900 GB/s) but only if accesses are **coalesced**: when the 32 threads of a warp touch 32 *consecutive* addresses, the hardware fuses them into a single wide transaction. Scattered or strided access throws that bandwidth away. The element-wise kernels here (transform, scatter, normalise) are perfectly coalesced — thread `idx` touches element `idx` — which is why they will run at near-peak bandwidth.

### 3.2 The column-major requirement

cuSOLVER inherits LAPACK's Fortran heritage and expects matrices in **column-major** order: consecutive elements in memory walk *down* a column, not across a row. That is why the matrix-assembly kernel writes

```cpp
Ak[row + col * n] = aij;     // column-major: row varies fastest
```

The index `row + col*n` places the row index as the fast-varying coordinate, which is exactly column-major. (Because the matrices here are symmetric, getting this wrong would not corrupt the *values*, but `lda`/fill-mode semantics in the solver assume the layout, so it matters in general and is correct here.)

### 3.3 The budget — and the one number that dominates

The program prints its own memory budget. The instructive line is the matrix store:

| Buffer | Size | Bytes |
|---|---|---|
| raw values | 16 MB | NVALS × 8 |
| transformed | 16 MB | NVALS × 8 |
| **submatrices** | **537 MB** | **BATCH × 32 × 32 × 8** |
| RHS / solution | 2 MB | BATCH × 32 × 8 |
| result + counts | 24 MB | NVALS × (8 + 4) |
| pointer arrays | 0.5 MB | BATCH × 8 × 2 |

The 65,536 dense 32×32 matrices consume **537 MB** — by far the largest allocation, an order of magnitude more than everything else combined. This is the price of *batching everything simultaneously*: every matrix is materialized in memory at the same time so the batched solver can chew through all of them in one call. It is a deliberate space-for-throughput trade. If memory were tight you would tile the batch (process, say, 8,192 systems at a time, reusing the matrix buffer), at the cost of more solver invocations. Worth knowing where the memory actually goes.

---

## 4. Configuration constants, decoded

```cpp
constexpr int N      = 32;          // submatrix dimension
constexpr int BATCH  = 65536;       // number of independent systems
constexpr int NVALS  = BATCH * N;   // 2,097,152 raw values
constexpr int N2     = N * N;       // 1024 elements per submatrix
constexpr int NRHS   = 1;           // one right-hand side per system

constexpr double RBF_GAMMA = 0.5;   // RBF bandwidth
constexpr double REG_DIAG  = 1.0e-3;// Tikhonov regularisation
```

- **`N = 32`** is chosen to match the warp size. When the assembly kernel launches 32 threads per block (one per matrix row), the block is exactly one warp — no partial warps, no wasted lanes, clean lockstep execution. (See §6.2.)
- **`BATCH = 65536` (= 2¹⁶)** is large enough to saturate the GPU: tens of thousands of independent systems give the scheduler an enormous amount of parallel work to hide latency behind. The batched solver's efficiency depends on having many problems to amortize its overhead over.
- **`NRHS = 1`** is not just a modeling choice — cuSOLVER's `potrsBatched` *only supports a single right-hand side*. The code respects that constraint.
- **`RBF_GAMMA` and `REG_DIAG`** are the two knobs of the matrix-assembly step and are explained with their math in §6.

`constexpr` means all of these are compile-time constants, so the compiler can fold them into the kernels (the loop bound `n` in the assembly kernel, for example, becomes a compile-time `32` and the loop can be unrolled).

---

## 5. Infrastructure: error handling and timing

### 5.1 Why every CUDA call is wrapped

```cpp
#define CUDA_CHECK(call) do { cudaError_t err = (call); if (err != cudaSuccess) throw ...; } while (0)
```

CUDA's C API reports errors through return codes, not exceptions, and — crucially — *asynchronous* errors (a kernel that faulted) may not surface until the next synchronizing call. Silently ignoring a return code is how you end up debugging garbage results three stages downstream. The macro converts every failure into a thrown `std::runtime_error` carrying file, line, and the decoded error string, so a failure stops the program at the exact call site. The `do { ... } while(0)` wrapper is the standard C idiom that lets a multi-statement macro behave like a single statement (so it is safe inside an unbraced `if`). `CUSOLVER_CHECK` does the same for the solver's own status enum.

### 5.2 Two timers, two jobs

```cpp
struct Timer    { ... std::chrono::high_resolution_clock ... };  // host wall clock
struct GpuTimer { cudaEvent_t start_ev, stop_ev; ... };          // device events
```

- **`Timer`** is an ordinary host stopwatch. It is used once, around the *whole* pipeline, and is only meaningful because the program calls `cudaDeviceSynchronize()` before reading it — at which point all device work is genuinely complete.
- **`GpuTimer`** records CUDA **events** into the stream. `cudaEventRecord(start_ev)` and `cudaEventRecord(stop_ev)` mark points in the device's execution timeline; `cudaEventElapsedTime` then measures the *device* time between them. `elapsed_ms()` calls `cudaEventSynchronize(stop_ev)` first, blocking the host until the stop event has actually been reached on the GPU. This is the only correct way to time an asynchronous kernel, for the reason given in §2.3.

The program times each stage with its own `GpuTimer`, which lets the final report attribute milliseconds to transform vs. assembly vs. factorization vs. solve vs. scatter — the breakdown that proves the thesis.

---

## 6. The pipeline, stage by stage

### Stage 1 — H2D: the first and only inbound crossing

```cpp
CUDA_CHECK(cudaMemcpy(d_raw, h_raw.data(), raw_bytes, cudaMemcpyHostToDevice));
```

The host generates 2M values deterministically (a ramp over [−5, 5] with a small sinusoidal wobble, so the data is reproducible and the verification is meaningful), then ships them up in one contiguous 16 MB transfer. One `cudaMemcpy`, one bus crossing. Everything after this is device-resident until Stage 7.

---

### Stage 2 — Transcendental transform (the *map* pattern)

```cpp
__global__ void transcendental_transform(const double* __restrict__ raw,
                                          double* __restrict__ out, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    double x  = raw[idx];
    double ax = fabs(x);
    out[idx]  = sin(x) * exp(-ax * 0.1) + sqrt(ax + 1.0);
}
```

**The pattern.** This is a *map*: one input element → one output element, every thread fully independent, no communication, no synchronization. It is the canonical "embarrassingly parallel" GPU workload and runs at essentially memory-bandwidth speed scaled by the cost of the arithmetic. Launched as 8,192 blocks of 256 threads, one thread per element.

**`__restrict__`.** The `__restrict__` qualifier on both pointers is a promise to the compiler that `raw` and `out` do not alias (do not point into overlapping memory). With that guarantee the compiler is free to keep loads in registers and reorder memory operations without worrying that a write through `out` might change a value reachable through `raw`. On the GPU this routinely enables wider load/store scheduling and better instruction-level parallelism. It is a correctness *promise* — the programmer is asserting it; the compiler trusts it.

**The transcendentals — a precision subtlety worth stating.** The function computes three transcendental operations per element: `sin`, `exp`, `sqrt`. The header comment calls this "high arithmetic intensity," and that is true in the sense that matters: the kernel reads 8 bytes and writes 8 bytes (16 bytes of traffic) while performing many tens of floating-point operations, so its arithmetic intensity (FLOPs ÷ bytes) sits well on the *compute-bound* side of the roofline — good GPU work, not bandwidth-starved.

But one nuance the comment glosses: these are **double-precision** transcendentals. The GPU's fast Special Function Units (SFUs) that make `__sinf`, `__expf` etc. nearly free are **single-precision only**. Double-precision `sin`/`exp` are evaluated by software polynomial routines — many instructions each. That is *why* this kernel is genuinely compute-heavy rather than instantly memory-bound, and it is also a lever: if the application could tolerate single precision, switching to intrinsics here would be dramatically faster. The choice of `double` throughout is a deliberate accuracy decision (this pipeline ends in a linear solve where precision compounds), not an oversight.

---

### Stage 3 — Forming the SPD submatrices (the mathematical heart)

```cpp
__global__ void form_submatrices(const double* __restrict__ vals,
                                 double* __restrict__ A, double* __restrict__ B,
                                 int n, int batch, double gamma, double reg)
{
    int k   = blockIdx.x;            // which system  (one block per system)
    int row = threadIdx.x;           // which row     (one thread per row)
    const double* v = vals + k * n;  // this system's 32-element window
    double* Ak = A + (size_t)k * n * n;
    double* bk = B + (size_t)k * n;

    double vi = v[row], row_sum = 0.0;
    for (int col = 0; col < n; ++col) {
        double diff = vi - v[col];
        double aij  = exp(-gamma * diff * diff);
        if (row == col) aij += reg;
        Ak[row + col * n] = aij;     // column-major
        row_sum += aij;
    }
    bk[row] = row_sum;               // b = A · [1,...,1]
}
```

#### 6.1 What matrix is being built — the RBF / Gaussian kernel

Each system `k` owns a contiguous, *non-overlapping* window of 32 transformed values, $v_0,\dots,v_{31}$ (system `k` uses transformed elements $[32k,\,32k+31]$). From those it builds the **Radial Basis Function** (a.k.a. Gaussian, a.k.a. squared-exponential) kernel matrix:

$$
A_{ij} \;=\; \exp\!\big(-\gamma\,(v_i - v_j)^2\big) \;+\; \varepsilon\,\delta_{ij}
$$

with bandwidth $\gamma = 0.5$ and regularizer $\varepsilon = 10^{-3}$. This is one of the most important objects in numerical analysis and machine learning: it is the Gram matrix of the points $\{v_i\}$ under the Gaussian kernel.

#### 6.2 Why it is symmetric positive definite — and why we care

The Gaussian kernel $K(u,w) = \exp(-\gamma\|u-w\|^2)$ is a **Mercer kernel**: it is *positive definite* in the functional sense, which (by Mercer's theorem / Bochner's theorem) guarantees that for *any* set of distinct points the resulting Gram matrix is **symmetric positive semidefinite**. Symmetry is obvious — $A_{ij}=A_{ji}$ because $(v_i-v_j)^2$ is symmetric. The positive-*semi*definiteness is the deep part: it is a property of the kernel itself, not of the data.

"Semi" becomes "strictly positive definite" once two things hold: the points are distinct (so no two rows are identical), and — to be safe regardless of the data — we add the **diagonal shift** $\varepsilon I$. That shift is **Tikhonov regularization**: it lifts every eigenvalue by $\varepsilon$, pushing the smallest eigenvalue safely away from zero and bounding the condition number. The line `if (row == col) aij += reg;` is doing exactly this.

Why does any of this matter? Because the next stage is **Cholesky factorization**, and Cholesky *only works on SPD matrices*. It has no pivoting and no fallback; feed it an indefinite or singular matrix and it fails. So the assembly stage is not just building a matrix — it is *manufacturing the guarantee* that the solver downstream cannot fail. The SPD property is constructed on purpose. (And `N = 32` matching the warp size means each system's block is exactly one warp: 32 rows, 32 threads, perfect lockstep, no divergence.)

#### 6.3 The block/thread decomposition

The launch is `<<<BATCH, N>>>` — **one block per system, one thread per row**. Thread `row` in block `k` computes the entire row `row` of matrix `k`: it loops over all 32 columns, evaluates the RBF value against each $v_{\text{col}}$, writes it column-major, and accumulates the row sum. 65,536 blocks give the scheduler tens of thousands of independent units; the GPU keeps as many resident per SM as registers and occupancy allow and streams through the rest.

*An optimization left on the table (worth noting for completeness):* every one of the 32 threads in a block reads the **same** 32-element window `v` from global memory — so the window is read 32 times over. Staging `v` into **shared memory** once at the top of the block (a `__shared__ double sv[32]`, cooperatively loaded, then `__syncthreads()`) would cut those redundant global reads to a single coalesced load and let all 32 threads read from on-chip memory. At this matrix size the assembly is unlikely to be the bottleneck, so the simpler code is a reasonable choice — but it is the first thing to try if profiling ever points here.

#### 6.4 The `b = A·1` trick — a self-verifying problem

The right-hand side is built as the **row sums**:

$$
b_i \;=\; \sum_{j} A_{ij}
$$

This is precisely $b = A\mathbf{1}$ where $\mathbf{1} = [1,1,\dots,1]^\top$. Therefore the *exact* solution of $Ax = b$ is, by construction, $x = \mathbf{1}$ — the all-ones vector. This is an elegant verification harness: it costs almost nothing to build (you are summing the row you just computed anyway), it requires no stored reference solution, and it lets the program measure the end-to-end numerical error of the *entire* pipeline at the end simply by checking how far each output is from 1.0. Any error introduced by the transform, the assembly, the factorization, or the solve shows up as a deviation from one. (Because $A$ is symmetric, the row sums equal the column sums, so the per-row accumulation is exactly right.)

---

### Interlude — Building the device pointer arrays

```cpp
__global__ void build_ptr_array(double** ptrs, double* base, int stride, int batch)
{
    int k = blockIdx.x * blockDim.x + threadIdx.x;
    if (k >= batch) return;
    ptrs[k] = base + (size_t)k * stride;
}
```

cuSOLVER's *batched* routines do not take one big contiguous buffer and a stride; they take an **array of device pointers**, one per problem — `double** Aarray` where `Aarray[k]` points at matrix `k`. The matrices already sit contiguously inside `d_A` (matrix `k` begins at offset `k·N2`), so all that is needed is an array of pointers into that block.

The naïve approach is to compute those pointers on the host and `cudaMemcpy` the small pointer array up — but that is *another bus crossing*, exactly the thing the whole design is avoiding. Instead this trivial kernel fills the pointer array **on the device**: thread `k` writes `base + k*stride`. It is launched twice, once for the matrices (`stride = N2 = 1024`) and once for the right-hand sides (`stride = N = 32`). Tiny, but ideologically consistent: even the bookkeeping stays resident.

Note the explicit `(size_t)` cast before the multiply — `k * stride` for the last matrix is `65535 * 1024 ≈ 6.7×10⁷`, which still fits in 32-bit `int`, but the cast is defensive against overflow for larger batches and is good practice whenever indexing into multi-hundred-megabyte buffers.

---

### Stage 4 — Batched Cholesky factorization (the numerical core)

```cpp
CUSOLVER_CHECK(cusolverDnDpotrfBatched(
    solver, CUBLAS_FILL_MODE_LOWER, N, d_Aptr, N, d_info, BATCH));
```

#### 6.5 What Cholesky factorization is

For any symmetric positive-definite matrix $A$, there exists a **unique** lower-triangular matrix $L$ with positive diagonal such that

$$
A \;=\; L\,L^{\top}.
$$

This is the Cholesky factorization. You can think of it as the "matrix square root for SPD matrices." The entries are computed by a direct recurrence — for column $j$:

$$
L_{jj} = \sqrt{\,A_{jj} - \sum_{m<j} L_{jm}^2\,}, \qquad
L_{ij} = \frac{1}{L_{jj}}\Big(A_{ij} - \sum_{m<j} L_{im}L_{jm}\Big)\ \ (i>j).
$$

The square root in the diagonal formula is the entire reason SPD is required: if $A$ is positive definite, the quantity under the root is provably positive at every step; if $A$ is not, you eventually take the square root of a non-positive number and the factorization fails. **That failure is the diagnostic** the program checks (below).

#### 6.6 Why Cholesky and not LU

For an SPD system, Cholesky is the right tool on every axis:

- **Half the work.** Cholesky costs about $\tfrac{1}{3}n^3$ floating-point operations; general LU costs about $\tfrac{2}{3}n^3$. Exploiting symmetry halves the arithmetic.
- **No pivoting.** General LU needs row pivoting for numerical stability, which means data-dependent row swaps — branchy, memory-shuffling, and awkward to batch on a GPU. Cholesky on an SPD matrix is **unconditionally backward stable without any pivoting**: the growth factor is bounded by the matrix itself. No pivoting means no divergence, which is exactly what you want across 65,536 parallel problems.
- **A built-in definiteness test.** The factorization succeeds *if and only if* the matrix is (numerically) positive definite. So running Cholesky simultaneously solves the system and certifies the matrix.

#### 6.7 The batched call and `CUBLAS_FILL_MODE_LOWER`

`cusolverDnDpotrfBatched` factorizes all `BATCH` matrices in a single call. The arguments:

- `CUBLAS_FILL_MODE_LOWER` — tells the routine to read (and overwrite) the **lower triangle** of each matrix and produce $L$. Because the matrices are symmetric, lower vs. upper is a free choice; lower is conventional.
- `N` (twice) — the matrix dimension and the leading dimension `lda` (column stride). They are equal here because the matrices are stored densely with no padding.
- `d_Aptr` — the array-of-pointers from the interlude.
- `d_info` — an array of `BATCH` integers, **one status per system** (this is genuinely per-problem for `potrf`).
- The factorization is written **in place**: each $L$ overwrites the lower triangle of its $A$.

#### 6.8 The status check — why it is there and what it catches

```cpp
cudaDeviceSynchronize();
cudaMemcpy(h_info.data(), d_info, info_bytes, cudaMemcpyDeviceToHost);
for (int k = 0; k < BATCH; ++k) if (h_info[k] != 0) ++bad;
if (bad > 0) throw std::runtime_error("Cholesky factorisation failed: ...");
```

`info[k] == 0` means system `k` factorized cleanly. `info[k] == m > 0` means the leading minor of order `m` was not positive definite — Cholesky hit a non-positive pivot at step `m` and bailed. Because the assembly stage deliberately built SPD matrices (Mercer kernel + $\varepsilon I$ regularization), this check should always pass — and that is the point: it converts the *mathematical* guarantee from §6.2 into a *runtime* assertion. If the regularizer were ever too small for some pathological window, this is where you would find out, loudly, instead of getting silent garbage. It is also a small, cheap copy back to the host (256 KB of ints), the only mid-pipeline D2H — and notably it copies *status*, not *data*, so it does not violate the resident-data principle.

---

### Stage 5 — Batched triangular solve

```cpp
CUSOLVER_CHECK(cusolverDnDpotrsBatched(
    solver, CUBLAS_FILL_MODE_LOWER, N, NRHS, d_Aptr, N, d_Bptr, N, d_info, BATCH));
```

Once $A = LL^\top$, solving $Ax = b$ no longer needs the matrix — it needs two cheap **triangular solves**:

$$
LL^\top x = b \;\;\Longrightarrow\;\;
\underbrace{Ly = b}_{\text{forward substitution}},\qquad
\underbrace{L^\top x = y}_{\text{back substitution}}.
$$

Each triangular solve is $O(n^2)$ — vastly cheaper than the $O(n^3)$ factorization — because a triangular system unwinds one variable at a time by direct substitution. `potrsBatched` does both substitutions for all 65,536 systems in one launch, reading the factored $L$'s from `d_Aptr` and the right-hand sides from `d_Bptr`, and **overwriting `B` in place with the solutions**. After this call, `d_B` no longer holds the right-hand sides; it holds the $x_k$ vectors.

Two semantic details worth being precise about:

- **`NRHS = 1` is mandatory, not incidental.** cuSOLVER's `potrsBatched` only supports a single right-hand side per system. The configuration honors this.
- **`d_info` here is *not* per-system.** Unlike `potrfBatched`, the `potrsBatched` info argument is a *single* integer used only to flag an illegal parameter (e.g., `info = -k` ⇒ the *k*-th argument was bad). The code passes the same `BATCH`-sized buffer for convenience; only one slot is meaningfully written. Harmless, but good to know so you do not misread `d_info` as a per-system solve status after this stage.

---

### Stage 6 — Scatter-average back into the result array

```cpp
__global__ void scatter_average(const double* __restrict__ X, double* __restrict__ result,
                                int* __restrict__ counts, int n, int batch)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= batch * n) return;
    int k = idx / n, i = idx % n, pos = k * n + i;
    atomicAdd(&result[pos], X[idx]);
    atomicAdd(&counts[pos], 1);
}

__global__ void normalise(double* __restrict__ result, const int* __restrict__ counts, int n)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= n) return;
    if (counts[idx] > 0) result[idx] /= (double)counts[idx];
}
```

#### 6.9 The scatter pattern and why atomics

Each solution $x_k$ has 32 entries that need to land back in the global result array at the window positions they came from. A *scatter* writes each computed value to a location determined at runtime. The danger of scatter is **collision**: if two threads can write to the same output address concurrently, an ordinary `+=` is a read-modify-write race and loses updates. The fix is `atomicAdd`, which performs the read-modify-write as one indivisible hardware operation — correct regardless of how the writes interleave. (`atomicAdd` on `double` requires compute capability ≥ 6.0; the `sm_70` build target satisfies this.)

#### 6.10 The honest note: here the atomics are unnecessary — and the comment says so

In *this* configuration the windows are **non-overlapping**, so the mapping is in fact the identity: `pos = k*n + i = idx`. Every output position is written **exactly once**, every `count` ends at 1, and the `normalise` divide is a no-op. So the atomics buy nothing but overhead right now. The code's own comment is candid about this: it is written *"for the general overlapping case."* The pattern it demonstrates — scatter with atomic accumulation plus a separate hit-count, then a normalize pass to turn sums into averages — is exactly what you need when windows *do* overlap (sliding stencils, overlap-add signal reconstruction, finite-element assembly), where many systems legitimately contribute to the same output cell and the final value should be their average. The example uses non-overlapping windows for a clean, verifiable demo while keeping the machinery that generalizes. That is a defensible pedagogical choice, but it is worth stating plainly that the atomic traffic is dead weight in the demo as written.

The two-pass "sum then divide by count" structure is the standard way to compute an average in parallel without knowing the multiplicities in advance: accumulate contributions and counts in the same scatter, then `normalise` divides them elementwise.

---

### Stage 7 — D2H: the second and final crossing

```cpp
std::vector<double> h_result(NVALS);
CUDA_CHECK(cudaMemcpy(h_result.data(), d_result, result_bytes, cudaMemcpyDeviceToHost));
cudaDeviceSynchronize();
```

The 16 MB result array comes home in one transfer. The trailing `cudaDeviceSynchronize()` guarantees every queued operation has completed before the host reads the wall-clock time — without it, the wall timer would stop while the GPU was potentially still working, and asynchronous errors might not yet have surfaced.

---

## 7. Verification and the performance report

### 7.1 The correctness check

```cpp
double worst = 0.0;
for (int i = 0; i < NVALS; ++i) {
    double err = std::fabs(h_result[i] - 1.0);
    if (err > worst) { worst = err; worst_idx = i; }
}
```

Because the right-hand side was built as $b = A\mathbf{1}$ (§6.4), the true solution is all ones; because the windows are non-overlapping and each count is 1, the scatter-average is the identity; therefore **every** result element should be exactly 1.0. The reported `Max |x − 1|` is the worst-case end-to-end error across all 2,097,152 outputs — it folds in the transform's rounding, the RBF assembly, the conditioning of each matrix, the Cholesky factorization, and the triangular solve. For well-regularized SPD systems of dimension 32 in double precision, this should land near machine epsilon (a few ×10⁻¹⁵ to 10⁻¹³ depending on conditioning). A large value here would immediately point a finger — most likely at the regularizer being too small for some window.

### 7.2 The transfer-vs-compute breakdown — the thesis, quantified

```cpp
float xfer    = t_h2d + t_d2h;
float compute = t_trans + t_form + t_fact + t_solve + t_scatter + t_norm;
```

The program sums the two transfer stages against the six compute stages and prints the ratio. This is the number the whole design exists to produce: in a resident pipeline the transfer slice should be a small single-digit percentage and the compute slice the overwhelming majority — the inversion of the transfer-bound anti-pattern from §1.2. The closing banner makes the comparison explicit: two host↔device trips total, versus the serial version's `65,536 × (H2D + solve + D2H)`.

A caveat on reading these timings: each stage is timed with its own pair of events on the default stream, and `elapsed_ms()` synchronizes on each. The per-stage numbers are sound. But note that because there is genuine work and the GPU pipelines instruction streams internally, summing isolated stage timings slightly over-counts relative to a single fused timing — the sum is an upper bound on the true overlapped cost. For the argument being made (transfer ≪ compute) this only strengthens the conclusion.

---

## 8. Where this could go further (for completeness)

Nothing below is a defect — the program is a clean, correct demonstration. These are the natural next moves if it graduated from demo to production:

1. **Kernel fusion.** The transform and the matrix assembly are separate launches with `d_trans` materialized between them. They could be fused so that each assembly block transforms its own 32-element window in shared memory and builds the matrix without ever writing `d_trans` to global memory — eliminating a 16 MB write and a 16 MB read.
2. **Shared-memory staging in assembly** (§6.3) — the cheapest single optimization if assembly ever shows up in a profile.
3. **The 537 MB matrix store** (§3.3) — tile the batch if memory-bound, or, for the RBF structure specifically, note that each matrix is generated from only 32 numbers, so it could in principle be regenerated on the fly inside a custom batched solver rather than stored — trading recompute for a 30× memory reduction.
4. **Single-precision or mixed-precision transcendentals** (§ Stage 2) if the accuracy budget allows, to put the SFUs to work.
5. **Pinned host memory** (`cudaHostAlloc`) for the two transfers, which roughly doubles achievable PCIe bandwidth and enables true async overlap — relevant if the two crossings ever become non-trivial at larger scale.
6. **A real overlapping-window workload** to make the scatter-average atomics earn their keep (§6.10), at which point the chosen pattern becomes exactly right rather than over-provisioned.

---

## 9. Build notes

```
nvcc -O2 -arch=sm_70 gpu_resident_pipeline.cu -lcusolver -lcublas -o gpu_pipeline
```

`-arch=sm_70` targets Volta (V100) — important here because double-precision `atomicAdd` needs compute capability ≥ 6.0 and the HBM2 bandwidth figures in §1 assume this class of device. `-lcusolver` provides the batched Cholesky routines; `-lcublas` is its required companion (cuSOLVER is layered on cuBLAS). The optional `-DWITH_CPU_REF` path (with `-llapack -lblas`) would build a host reference for cross-checking, consistent with a "trust but verify" methodology.

---

## 10. One-screen summary

| Stage | Technique | Why it's on the GPU |
|---|---|---|
| H2D | single bulk transfer | pay the bus tax exactly once |
| transcendental transform | embarrassingly-parallel map | thousands of independent FLOP-heavy threads |
| form submatrices | RBF/Mercer kernel + Tikhonov reg → SPD | constructs the factorization's success guarantee |
| build pointer arrays | device-side bookkeeping | avoids an extra H2D crossing |
| Cholesky factorize | $A=LL^\top$, $\tfrac13 n^3$, no pivot | stable + branch-free across 65,536 problems |
| triangular solve | forward/back substitution, $O(n^2)$ | reuses the factor, one launch for all systems |
| scatter + normalise | atomic scatter + count, then divide | generalizes to overlapping stencils |
| D2H | single bulk transfer | pay the bus tax exactly once |

The single sentence that captures it: **keep the data on the device and bring the work to the data, so the only thing the slow bus ever sees is the raw input going up and the final answer coming down.**
