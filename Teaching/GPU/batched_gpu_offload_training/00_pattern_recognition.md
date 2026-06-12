# When This Packet Applies — Pattern Recognition for Batched Small-Matrix GPU Offload

## Connection to the Triage Course

The GPU Offload Triage Training course answers the first question: *should this
code go onto a GPU at all?*  It teaches loop-level profiling with Intel Advisor,
the distinction between automatic loop offload (Advisor's question) and operation
replacement (the engineering question), and the diagnostic categories that
determine whether a loop is a profitable offload target: dependency, launch tax,
and trip count.

This packet picks up where the triage course leaves off.  The course concludes
with the observation that Advisor rejects the literal source loops because of
serial dependencies, but the *operation-replacement* question — should the dense
linear algebra be replaced by a batched GPU library call? — lives one abstraction
level higher and cannot be answered by a loop profiler.

This document bridges the two: given that you have identified a candidate for
operation replacement, how do you recognize whether your problem has the specific
structure that this packet teaches?


## The Diagnostic Questions

Four questions determine whether your problem maps to this packet.  If all four
answers are yes, the migration path in Stages 0–6 applies directly.

### 1. Is the hot path a dense linear solve, factorization, or eigendecomposition?

Look for: Cholesky, LU, QR, SVD, least-squares, triangular solve, symmetric
eigenvalue.  The operation does not need to be a library call — it might be a
hand-written factorization (Numerical Recipes, Golub-Van Loan transcription,
or a bespoke implementation someone wrote in 2005).  What matters is that the
mathematical operation is one of the standard dense linear algebra kernels.

If the hot path is sparse linear algebra, FFT, convolution, or a custom
iterative solver, this packet does not apply directly.  Sparse and iterative
problems have different GPU migration patterns (cuSPARSE, cuFFT, or custom
kernels) with different data layout and batching considerations.

### 2. Are the matrices small?

"Small" means the individual matrix does not saturate the GPU on its own.
The boundary is fuzzy, but the practical threshold is roughly:

    N < 512    →  definitely small, batching is the only path to GPU utilization
    N = 512–2048  →  gray zone, single cuSOLVER calls may be viable
    N > 2048   →  single large solve can saturate the GPU, batching is unnecessary

For the problems this packet targets, N is typically 4–128.  The per-system
FLOP count (O(N³/3) for Cholesky) is so low that kernel launch overhead
exceeds useful compute unless many systems are batched into one dispatch.

If your matrices are large (N > 1024), you do not need this packet — a single
cuSOLVER or MAGMA call per matrix is the right approach.

### 3. Are the systems independent, or can they be made independent?

The systems are independent if the input data for system k does not depend on
the solution of system k−1.  This is the batching precondition: all matrices
must exist simultaneously so they can be submitted in one call.

Three cases:

**Already independent.**  The loop iterates over spatial locations, sensor
channels, frequency bins, or mesh elements.  Each system is built from
different input data with no cross-system dependency.  This is the easy case.

**Artificially serial.**  The loop assembles matrix k, solves it, uses the
solution, then assembles matrix k+1 — but the assembly of k+1 does not
actually depend on the solution of k.  The serial structure is an artifact of
how the code was written, not a fundamental constraint.  Decouple assembly
from solve, and the systems become independent.  This is the case the training
packet demonstrates.

**Genuinely serial.**  System k's matrix is constructed from system k−1's
solution (e.g., Newton iteration, time-stepping where the Jacobian depends on
the current state).  The systems cannot be batched because they do not all
exist at the same time.  This packet does not apply.  The GPU migration path
for genuinely serial solves is different: either batch across a different
dimension (multiple independent simulations, multiple right-hand sides) or
pipeline the assembly-solve sequence using CUDA graphs.

### 4. Is the matrix storage contiguous?

Batched cuSOLVER requires a contiguous array of matrices (one flat allocation
with pointer offsets) or at minimum an array of device pointers to contiguous
per-matrix blocks.  If the legacy code uses:

- **Contiguous column-major or row-major arrays** — ready for GPU.  Build the
  pointer array and call the batched routine.
- **double\*\* row-pointer storage** (Numerical Recipes idiom) — rows are
  individually malloc'd, scattered across the heap.  Must be restructured to
  contiguous storage before GPU migration.  This is a code-level obstacle, not
  an algorithmic one.
- **Struct-of-arrays with matrices embedded in larger structs** — the matrix
  data may be interleaved with non-matrix fields.  Extraction into a
  contiguous batch buffer is required.

If the answer to Question 4 is "no," the restructuring in Stage 0→2 of this
packet shows exactly how to make it contiguous.


## The Pipeline Skeleton

Every problem that passes the four diagnostic questions reduces to the same
computational skeleton:

    ┌──────────────────────────────────────────────────────┐
    │  1. Ingest       raw input data (host → device)      │
    │  2. Transform    element-wise nonlinear preprocessing │
    │  3. Assemble     form all matrices from local data    │
    │  4. Solve        batched dense linear algebra         │
    │  5. Postprocess  derive outputs from solutions        │
    │  6. Extract      results (device → host)              │
    └──────────────────────────────────────────────────────┘

Stages 1 and 6 are data movement (DeviceInput / DeviceOutput in ulib terms).
Stages 2, 3, and 5 are embarrassingly parallel map or gather/scatter kernels.
Stage 4 is a vendor library call (cuSOLVER, MAGMA, cuBLAS).

The entire pipeline stays device-resident between stages 1 and 6.  No
intermediate host transfers.  This is what distinguishes the restructured
pipeline from the "just add CUDA" anti-pattern, where each solve does its
own H2D/D2H round trip.

Not every problem uses all six stages.  A problem where the matrices arrive
pre-formed from a host buffer skips stages 2 and 3 and goes straight to
batched solve (Stage 2 of the training packet: `batched_cholesky_solve.cu`).
A problem where the postprocessing is trivial (just read the solution vector)
collapses stage 5 to a no-op.  But the skeleton is always the same.


## Domain Map

The following domains produce the batched-small-solve pattern.  For each,
the table shows what maps to each pipeline stage.

### Moving-Window Spectral Estimation

This is the worked example in the training packet.

| Stage | Maps to |
|-------|---------|
| Ingest | Time-domain signal from DAQ / acquisition |
| Transform | Hann window (cos taper per sample) |
| Assemble | Autocovariance → Toeplitz matrix (Yule-Walker) |
| Solve | Cholesky: R·a = r for AR coefficients |
| Postprocess | PSD evaluation: P(f) = σ²/\|A(f)\|² |
| Extract | Time-frequency spectrogram |

Matrix size: AR order (typically 8–64).  System count: number of overlapping
windows (thousands to hundreds of thousands).

### Finite Element Local Condensation

| Stage | Maps to |
|-------|---------|
| Ingest | Nodal coordinates and material properties |
| Transform | Shape function evaluation, Jacobian computation |
| Assemble | Element stiffness matrix from quadrature |
| Solve | Static condensation of internal DOFs (Schur complement) |
| Postprocess | Scatter element contributions to global system |
| Extract | Condensed global stiffness / load vector |

Matrix size: DOFs per element (typically 8–96 for 3D hex/tet elements).
System count: number of elements (thousands to millions).  The scatter
in stage 5 uses atomic adds — identical to the training packet's
`gpu_resident_pipeline.cu`.

### Adaptive Beamforming / Sensor Array Processing

| Stage | Maps to |
|-------|---------|
| Ingest | Raw sensor time series (multi-channel) |
| Transform | Bandpass filtering, analytic signal (Hilbert) |
| Assemble | Sample covariance matrix from short data blocks |
| Solve | Capon / MVDR: R⁻¹ · s for weight vector |
| Postprocess | Beampower: P(θ) = 1 / (sᴴ R⁻¹ s) per look direction |
| Extract | Angular power map or bearing estimates |

Matrix size: number of sensors (typically 4–64).  System count: number of
time snapshots × frequency bins (thousands to millions).

### Kernel Regression / Local Gaussian Process Prediction

| Stage | Maps to |
|-------|---------|
| Ingest | Observation locations and values |
| Transform | Feature expansion, distance computation |
| Assemble | Local kernel matrix K (RBF, Matérn, etc.) |
| Solve | K · α = y for local interpolation weights |
| Postprocess | Prediction: f(x*) = Σ αᵢ k(xᵢ, x*) |
| Extract | Predicted field values at query points |

Matrix size: neighborhood size (typically 16–128).  System count: number of
query points (thousands to millions).  The `gpu_resident_pipeline.cu` in this
packet is literally this problem with an RBF kernel.

### Mesh-Free Methods (SPH, RKPM, MLS)

| Stage | Maps to |
|-------|---------|
| Ingest | Particle positions and field values |
| Transform | Neighbor search, kernel evaluation |
| Assemble | Moment matrix from local particle neighborhood |
| Solve | Moment matrix inversion for shape function coefficients |
| Postprocess | Field approximation at evaluation points |
| Extract | Updated particle field values |

Matrix size: polynomial basis size × spatial dimension (typically 6–20 in 2D,
10–35 in 3D).  System count: number of particles (thousands to millions).

### Recursive Least Squares / Windowed System Identification

| Stage | Maps to |
|-------|---------|
| Ingest | Input-output time series from plant / test system |
| Transform | Regressor construction (delay embedding, basis expansion) |
| Assemble | Local information matrix XᵀX from windowed data |
| Solve | Normal equations: (XᵀX) θ = Xᵀy for local model parameters |
| Postprocess | Parameter trajectory, model residuals, confidence bounds |
| Extract | Time-varying model coefficient array |

Matrix size: number of regressors (typically 4–64).  System count: number of
sliding windows (thousands to hundreds of thousands).

This is the closest domain to the training packet's application context —
it is what you do when you want to track how a servo-hydraulic system's
transfer function changes during a test.  Replace "AR coefficients from
Yule-Walker" with "regression coefficients from normal equations" and the
pipeline is structurally identical.


## What to Read Next

If your problem passes the four diagnostic questions and maps to one of the
domains above:

1. Read `ar_spectral_estimation_theory.md` for the mathematical development
   of the worked example.

2. Read `ar_spectral_original.c` (Stage 0) to see the legacy code structure
   and identify which migration obstacles apply to your codebase.

3. Read `batched_cholesky_solve.cu` (Stage 2) for the minimal restructuring
   that achieves the first GPU win — contiguous storage + batched dispatch.

4. Read `ar_spectral_pipeline.cu` (Stage 4) for the full device-resident
   pipeline.  Map your domain's stages to the six-stage skeleton above.

5. Read `ar_spectral_kokkos.cpp` (Stage 5) and `ar_spectral_ulib.cpp`
   (Stage 6) for the Kokkos and ulib integration patterns.

The anti-pattern file `ar_spectral_legacy.cu` (Stage 1) exists so you can
recognize the "just add CUDA" mistake when you see it — or when you are
tempted to make it.
