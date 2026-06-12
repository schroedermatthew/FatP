# Moving-Window AR Spectral Estimation on GPU

## The Physical Problem

A servo-hydraulic test system drives a specimen through a load program while transducers record force, displacement, strain, and acceleration.  The spectral content of these signals is not stationary — resonant frequencies shift as the specimen degrades, swept-sine excitations move through frequency, and transient events (crack initiation, bearing seizure, valve transitions) excite and then damp different structural modes.

The Fourier transform assumes stationarity over its analysis window.  A single FFT of the full record produces a frequency-domain average that smears time-varying features into a flat spectral estimate.  You see *that* a 1200 Hz mode existed, but not *when* it appeared, how long it lasted, or whether it drifted.

The standard fix is the **short-time Fourier transform** (STFT): chop the signal into short overlapping windows, FFT each one, and stack the results into a time-frequency matrix.  The STFT works, but its resolution is bounded by the **Heisenberg-Gabor limit**: a short window gives good time resolution but poor frequency resolution, and vice versa.  For a window of *W* samples at sample rate *f_s*, the frequency resolution is *f_s / W*.  At *f_s* = 10 kHz and *W* = 128, that's 78 Hz — too coarse to resolve closely-spaced structural modes.

**Parametric spectral estimation** breaks this tradeoff by imposing a model on the data.  Instead of asking "what frequencies are present?" (the DFT question), it asks "what autoregressive process best explains this window?" (the AR model question).  The AR model has *p* free parameters regardless of window length, and its spectral resolution depends on model order, not window length.  An AR(32) model estimated from 128 samples can resolve features that an FFT of the same 128 samples cannot.

The cost is a linear solve per window.  With 65,536 overlapping windows, that's 65,536 independent 32×32 Cholesky solves — the exact workload the GPU batched pipeline is designed for.


## The AR Model

An **autoregressive process of order *p*** models the current sample as a linear combination of the *p* most recent samples plus white noise:

    x[n] = a₁·x[n−1] + a₂·x[n−2] + ⋯ + aₚ·x[n−p] + ε[n]

where **a** = [a₁, …, aₚ]ᵀ are the AR coefficients and ε[n] is white innovation noise with variance σ².

The transfer function is:

    H(z) = 1 / A(z),    A(z) = 1 − Σₖ aₖ z⁻ᵏ

and the power spectral density is:

    P(f) = σ² / |A(e^{j2πf/fₛ})|²

The AR model describes a signal whose spectrum is determined entirely by the pole locations of H(z).  Each complex-conjugate pole pair produces a spectral peak.  An AR(32) model can represent up to 16 spectral peaks — enough for most structural vibration scenarios.


## The Yule-Walker Equations

The AR coefficients satisfy the **Yule-Walker equations**, derived by multiplying both sides of the AR recursion by x[n−k] and taking expectations:

    R · a = r

where:

    R[i,j] = rₓₓ(|i − j|),    i,j = 0, …, p−1       (Toeplitz)
    r[i]   = rₓₓ(i + 1),       i = 0, …, p−1

and rₓₓ(k) is the **autocovariance at lag k**:

    rₓₓ(k) = E[x[n] · x[n−k]]

R is the *p × p* autocovariance matrix.  It is **symmetric positive definite** for any non-degenerate stationary process — this is what makes Cholesky the natural solver.

The innovation variance is:

    σ² = rₓₓ(0) − aᵀ · r


## Sample Autocovariance Estimation

For a finite window of *W* samples x[0], …, x[W−1], the biased autocovariance estimator is:

    r̂ₓₓ(k) = (1/W) · Σₙ₌ₖᵂ⁻¹ x[n] · x[n−k],    k = 0, …, p

The biased estimator (dividing by W, not W−k) is preferred because it guarantees the resulting Toeplitz matrix is positive semi-definite.  With a window function applied and W > 2p, the matrix is positive definite in practice.


## Why Batched Cholesky, Not Levinson-Durbin

The classical algorithm for Toeplitz Yule-Walker systems is **Levinson-Durbin recursion**, which solves the system in O(p²) operations by exploiting Toeplitz structure.  For a single system, this beats Cholesky's O(p³/3).

But Levinson-Durbin is **inherently serial** — order *k* depends on order *k−1*.  You cannot parallelise within one system.  On a CPU processing systems one at a time, this is fine.  On a GPU processing 65,536 systems simultaneously, the per-system cost matters less than the ability to dispatch all systems in one kernel launch.

The arithmetic comparison:

    Levinson-Durbin:  p² = 1,024 FLOPs/system × 65,536 systems = 67M FLOPs (serial)
    Batched Cholesky:  p³/3 ≈ 10,923 FLOPs/system × 65,536 systems = 716M FLOPs (parallel)

The GPU does 10× more arithmetic but finishes faster because all 65,536 systems execute simultaneously.  This is the fundamental GPU bargain: trade arithmetic efficiency for parallelism.


## The Windowing Function

Before estimating the autocovariance, each window is tapered with a **Hann window** to suppress spectral leakage from the window edges:

    w[n] = 0.5 · (1 − cos(2πn / (W−1))),    n = 0, …, W−1
    x̃[n] = x[n] · w[n]

The autocovariance is then estimated from the tapered signal x̃.  The Hann window's sidelobe level (−31 dB) is sufficient for most vibration analysis; for demanding dynamic range requirements, a Kaiser or Dolph-Chebyshev window can be substituted.

The cos evaluation in the Hann window is the "transcendental" step of the pipeline — it maps naturally to a GPU kernel where each thread computes one sample's window coefficient.


## The Pipeline

The full moving-window AR spectral estimation maps to six GPU stages:

    ┌─────────────────────────────────────────────────────────────┐
    │  Stage 1:  H2D transfer — raw signal to device              │
    │  Stage 2:  Windowing kernel — Hann taper (transcendental)   │
    │  Stage 3:  Autocovariance + matrix assembly kernel           │
    │  Stage 4:  Batched Cholesky solve (cuSOLVER)                │
    │  Stage 5:  PSD evaluation kernel — AR coefficients to P(f)  │
    │  Stage 6:  D2H transfer — time-frequency matrix to host     │
    └─────────────────────────────────────────────────────────────┘

Stages 2, 3, and 5 are custom CUDA kernels.  Stage 4 is a library call.  The data touches the host exactly twice — everything between is device-resident.


## PSD Evaluation

Given the AR coefficients **a** and innovation variance σ² for window *w*, the PSD at frequency bin *j* is:

    P_w(fⱼ) = σ² / |A_w(fⱼ)|²

where:

    A_w(fⱼ) = 1 − Σₖ₌₁ᵖ aₖ · e^{−j2πfⱼk/fₛ}

This is a complex polynomial evaluation at *p* points on the unit circle.  For each (window, frequency) pair, the computation is:

    Re(A) = 1 − Σₖ aₖ cos(2πfⱼk/fₛ)
    Im(A) =     Σₖ aₖ sin(2πfⱼk/fₛ)
    P = σ² / (Re² + Im²)

With BATCH × NFREQ evaluations (65,536 × 256 = 16.8M), this is another embarrassingly parallel kernel — one thread per (window, frequency) pair.

The output is a **time-frequency matrix** of dimension BATCH × NFREQ, which is the spectrogram.


## Verification Strategy

The test signal is constructed with known time-frequency content:

1. **Linear chirp**: frequency sweeps from f₀ to f₁ over the signal duration.
   The instantaneous frequency at time t is f(t) = f₀ + (f₁ − f₀)·t/T.
   The AR spectral peak at each window should track f(t).

2. **Transient burst**: a Gaussian-modulated sinusoid at frequency f_t,
   centered at time t₀.  The AR spectrum should show a transient peak
   at f_t near t₀.

3. **White noise floor**: the AR spectrum should show a flat noise floor
   away from the signal components.

The peak-tracking error (estimated peak frequency minus known chirp frequency) quantifies the accuracy of the AR spectral estimator and verifies the entire pipeline from signal generation through batched solve to PSD evaluation.


## Parameters Used in the Implementation

    AR model order:     p  = 32         (32×32 Cholesky systems)
    Window length:      W  = 128        (4× oversampling of AR order)
    Window stride:      S  = 32         (75% overlap)
    Number of windows:  65,536
    Signal length:      (65535 × 32) + 128 = 2,097,248 samples
    Sample rate:        fₛ = 10,000 Hz
    Signal duration:    ≈ 209.7 seconds
    Frequency bins:     256 (0 to fₛ/2)
    Chirp:              200 Hz → 2000 Hz
    Transient:          1200 Hz, centered at t = 105 s, σ_t = 2 s

    Device memory:      ≈ 791 MB total
                        (signal 17 MB + windowed 67 MB + matrices 537 MB
                         + RHS×2 34 MB + PSD 134 MB + misc 2 MB)
