# Kokkos Layout Selection for Batched Window-Local GPU Kernels

## What This Document Teaches

How to choose the correct Kokkos memory layout for 2D views on GPU.
Specifically, why the obvious default is wrong for this pipeline and
how to analyze your kernel's access pattern to get the right answer.

This is not a reference on Kokkos layouts in general.  It is a
worked example of a real mistake, the analysis that reveals it,
and the correction — using the AR spectral pipeline as the concrete case.


## The Mistake

The first version of the Kokkos pipeline used a single 2D view alias:

```cpp
using view_2d = Kokkos::View<double**, Kokkos::LayoutLeft, MemSpace>;
```

with the comment:

```cpp
// 2D device view (LayoutLeft on CUDA — column-major, natural for coalescing)
```

This alias was then applied to every 2D array in the pipeline:

```cpp
view_2d d_xw ("windowed", BATCH, WLEN);     // [window, sample]
view_2d d_psd("psd",      BATCH, NFREQ);    // [window, frequency]
```

The reasoning was: "LayoutLeft is the standard Kokkos layout for CUDA.
Column-major is natural for GPU coalescing.  Use it everywhere."

That reasoning is wrong.  Not because LayoutLeft is never correct on GPU,
but because "standard default" is not an analysis.  The correct layout
depends on three things that must be examined together:

```
memory layout  ×  execution mapping  ×  access pattern
```

Skipping any one of the three produces the wrong answer.


## What LayoutLeft and LayoutRight Mean

For a 2D Kokkos view `A(i, j)`:

**LayoutLeft** — the leftmost index varies fastest in memory:

```
A(0,0)  A(1,0)  A(2,0)  A(3,0)  ...  A(BATCH-1,0)
A(0,1)  A(1,1)  A(2,1)  A(3,1)  ...  A(BATCH-1,1)
A(0,2)  ...
```

For `A(i, j)`, elements with adjacent `i` and the same `j` are
contiguous.  Elements with the same `i` and adjacent `j` are
separated by a stride of `extent(0)`.

**LayoutRight** — the rightmost index varies fastest:

```
A(0,0)  A(0,1)  A(0,2)  ...  A(0,WLEN-1)
A(1,0)  A(1,1)  A(1,2)  ...  A(1,WLEN-1)
A(2,0)  ...
```

For `A(i, j)`, elements with the same `i` and adjacent `j` are
contiguous.  Elements with adjacent `i` and the same `j` are
separated by a stride of `extent(1)`.


## How Kokkos Maps Threads on GPU

The critical piece the original code ignored.

### RangePolicy

`Kokkos::RangePolicy<ExecSpace>(0, N)` maps one thread per index.
Adjacent threads get adjacent indices.  No layout issue — it's 1D.

### MDRangePolicy

`Kokkos::MDRangePolicy<Kokkos::Rank<2>>({0, 0}, {M, N})` maps a
2D index space to a 2D CUDA grid.

The `Rank` template accepts optional iteration-order parameters:

```cpp
Kokkos::Rank<2, OuterDirection, InnerDirection>
```

**The default iteration order is execution-space-dependent.  On CUDA,
the default aligns with LayoutLeft** — the leftmost rank varies fastest
across threads.  This means a default `Rank<2>` policy makes adjacent
threads vary the **first** index, not the last.

For window-local views stored as `LayoutRight`, the iteration order
must be set explicitly so adjacent threads vary the rightmost rank:

```cpp
using mdrange_right_2d = Kokkos::MDRangePolicy<
    Kokkos::Rank<2, Kokkos::Iterate::Right, Kokkos::Iterate::Right>>;
```

With `Iterate::Right`, for `mdrange_right_2d({0, 0}, {BATCH, NFREQ})`:

```
Thread 0 in warp:  (w, j)
Thread 1 in warp:  (w, j+1)
Thread 2 in warp:  (w, j+2)
...
Thread 31 in warp: (w, j+31)
```

All 32 threads in the warp share the same `w` and have consecutive `j`.

**Without explicit `Iterate::Right`, adjacent threads would vary `w`
instead, and LayoutRight views would be strided — the same problem
as before, just with the roles swapped.**  The layout and the
iteration order must agree.

### TeamPolicy

`Kokkos::TeamPolicy<ExecSpace>(league_size, team_size)` assigns one
team to each league rank.  Within a team, `team.team_rank()` gives
each thread its local index.

For `TeamPolicy(BATCH, ORDER+1)`:

```
Team w, Thread 0:  (w, 0)
Team w, Thread 1:  (w, 1)
...
Team w, Thread 31: (w, 31)
```

Adjacent threads within the team have the same `w` and consecutive
local indices.


## The Analysis: Three Kernels, One Mistake

### Kernel 1 — Hann Windowing (MDRangePolicy)

```cpp
Kokkos::parallel_for("hann_window",
    mdrange_right_2d({0, 0}, {BATCH, WLEN}),
    KOKKOS_LAMBDA(const int w, const int n) {
        d_xw(w, n) = d_sig(w * STRIDE + n) * hann;
    });
```

**Execution mapping:** with explicit `Iterate::Right`, the rightmost rank (n)
maps to threadIdx.x.  Adjacent threads have (w, n), (w, n+1), (w, n+2), ...

**Write access:** `d_xw(w, n)` where n varies across adjacent threads.

**With LayoutLeft:** `d_xw(w, n)` has n in the second position.
LayoutLeft makes the first position (w) contiguous.  So
`d_xw(w, n)` and `d_xw(w, n+1)` are separated by stride BATCH.

Adjacent threads write to addresses 512 KiB apart.  **Fully uncoalesced.**

**With LayoutRight:** `d_xw(w, n)` and `d_xw(w, n+1)` are contiguous.
Adjacent threads write to adjacent addresses.  **Coalesced.**


### Kernel 2 — Autocovariance (TeamPolicy)

```cpp
Kokkos::parallel_for("autocov",
    team_policy(BATCH, ORDER + 1).set_scratch_size(...),
    KOKKOS_LAMBDA(const team_member& team) {
        int w   = team.league_rank();
        int tid = team.team_rank();

        for (int n = tid; n < WLEN; ++n)
            rk += d_xw(w, n) * d_xw(w, n - tid);
    });
```

**Execution mapping:** one team per window.  thread_rank gives the
local index within the team.

**Read access:** `d_xw(w, n)` where w is fixed for the whole team
and n varies.  At any given loop iteration, adjacent threads within
the team access `d_xw(w, 0)`, `d_xw(w, 1)`, ..., `d_xw(w, 31)`.

**With LayoutLeft:** those reads are at:

```
base + w,  base + w + BATCH,  base + w + 2*BATCH,  ...
```

Stride of 65,536 doubles between adjacent threads.  **Fully uncoalesced.**

**With LayoutRight:** those reads are at:

```
base + w*WLEN,  base + w*WLEN + 1,  base + w*WLEN + 2,  ...
```

Contiguous.  **Coalesced.**

This kernel makes the mistake most visible because the entire
algorithmic structure is "one team owns one window, threads
iterate over that window's local samples."  LayoutLeft
stores different windows' samples interleaved, which is the
opposite of what this kernel needs.


### Kernel 3 — PSD Evaluation (MDRangePolicy)

```cpp
Kokkos::parallel_for("psd_eval",
    mdrange_right_2d({0, 0}, {BATCH, NFREQ}),
    KOKKOS_LAMBDA(const int w, const int j) {
        d_psd(w, j) = 10.0 * Kokkos::log10(...);
    });
```

Identical analysis to Kernel 1.  Adjacent threads have consecutive
j for the same w.  LayoutLeft makes j strided.  LayoutRight makes
j contiguous.

**The original ulib code had this comment:**

```cpp
//  The Kokkos::LayoutLeft device view gives coalesced access in
//  the PSD evaluation kernel (adjacent threads access adjacent
//  frequency bins within the same window).
```

This comment describes why LayoutRight is correct while asserting
that LayoutLeft achieves it.  It is internally contradictory.
"Adjacent frequency bins" are contiguous under LayoutRight, not
LayoutLeft.  The comment was written by reasoning about the access
pattern correctly and then applying the layout backwards.


## Why the Mistake Happens

The reasoning that produces this error has a specific structure:

1. "LayoutLeft is the Kokkos GPU default" — true but irrelevant.
   The default is a convention for when you don't specify, not a
   recommendation for when you do.

2. "Column-major is natural for GPU coalescing" — partially true
   and fatally misleading.  Column-major means the first index is
   contiguous.  GPU coalescing requires adjacent threads to access
   contiguous memory.  These are the same thing ONLY if adjacent
   threads vary the first index.

3. The missing step: asking "which index varies across adjacent
   threads in MY kernel?"  Without this step, the analysis maps
   a layout to a hardware property without going through the
   actual thread mapping.

The general failure mode is:

```
GPU wants coalescing → coalescing means contiguous → LayoutLeft
is contiguous in the first index → LayoutLeft is the GPU layout
```

Every step in that chain is individually correct.  The chain is
wrong because it never checks which index the threads are
actually varying.


## The Correct Layout for This Pipeline

### The Rule

For a 2D Kokkos view `V(a, b)` on GPU:

```
If adjacent threads vary a (leftmost):   use LayoutLeft
If adjacent threads vary b (rightmost):  use LayoutRight
```

For `MDRangePolicy` on CUDA with **explicit `Iterate::Right`**, the
rightmost rank maps to threadIdx.x.  The layout and iteration order
must agree:

```
mdrange_right_2d({0,0}, {M, N}) writing V(i, j)
  → Iterate::Right: adjacent threads vary j
  → j must be contiguous
  → LayoutRight

Default Rank<2> on CUDA (Iterate::Left): adjacent threads vary i
  → i must be contiguous
  → LayoutLeft
```

**You must choose a consistent pair.**  This pipeline uses LayoutRight
views + explicit `Iterate::Right` policies, because the TeamPolicy
kernel also needs the local index contiguous (see below).

For `TeamPolicy` where the team owns one "row" and threads index
within it:

```
TeamPolicy(num_rows, row_width) accessing V(row, col)
  → adjacent threads vary col
  → col must be contiguous
  → LayoutRight
```

### The Role-Specific Type Aliases

Do not use one generic `view_2d` for all 2D arrays.  Different
data roles have different layout requirements.

```cpp
// Window-local data: (window, local_index) → LayoutRight
// Adjacent threads vary local_index (MDRange rightmost rank,
// or TeamPolicy thread rank within a window-team).
using window_view_2d =
    Kokkos::View<double**, Kokkos::LayoutRight, MemSpace>;

// cuSOLVER matrix buffers: flat 1D, manual column-major indexing
// Each batch item is a contiguous block.  Column-major inside
// the block because cuSOLVER requires it.  Not a layout question —
// it is explicit index arithmetic.
using view_1d =
    Kokkos::View<double*, MemSpace>;
```

### The cuSOLVER Buffers Are a Separate Case

The solver matrices are stored as:

```cpp
view_1d d_A("matrices", BATCH * ORDER * ORDER);
```

with manual indexing:

```cpp
d_A(w * ORDER * ORDER + row + col * ORDER) = value;
```

This is batch-major outside, column-major inside each matrix
block.  It is correct for cuSOLVER and it is not a Kokkos layout
question — it is explicit pointer arithmetic on a flat buffer.

Do not convert this to `View<double***, LayoutRight>` or any other
multi-dimensional view.  The flat buffer with manual indexing is
the correct abstraction for vendor library interop where the
library dictates the memory layout.


## How to Check Your Own Kernels

For every 2D (or higher) Kokkos view accessed inside a parallel
dispatch:

1. **Identify the execution policy.**
   RangePolicy, MDRangePolicy, TeamPolicy, or TeamThreadRange.

2. **Determine which index adjacent threads vary.**
   - MDRangePolicy with default `Rank<N>`: depends on execution space.
     CUDA default is `Iterate::Left` (leftmost rank fastest).
     Use explicit `Iterate::Right` if you need rightmost rank fastest.
   - TeamPolicy: `team_rank()` within one team
   - Nested TeamThreadRange: the inner range index

3. **Match the layout AND iteration order to the thread index.**
   The index that varies across adjacent threads must be the
   contiguous index in memory, and the iteration order must make
   that index vary across threads.
   - If that's the leftmost index → LayoutLeft + default iteration
   - If that's the rightmost index → LayoutRight + `Iterate::Right`

4. **If different kernels disagree**, you need either:
   - Separate views with different layouts for different kernels
   - A single layout chosen to favor the most bandwidth-sensitive
     kernel (profile to determine which one)
   - A layout-agnostic view (`Kokkos::LayoutStride`) with a
     performance cost from indirect indexing

For the window-local 2D arrays in this pipeline (`d_xw` and `d_psd`),
all kernels agree: adjacent device work varies the rightmost local
index.  LayoutRight paired with explicit `Iterate::Right` is correct
for those views.  The cuSOLVER matrix buffers are a separate case —
flat 1D with column-major blocks, as documented above.

If the Hann kernel had been written with swapped dimensions:

```cpp
mdrange_right_2d({0, 0}, {WLEN, BATCH})
    KOKKOS_LAMBDA(int n, int w) { d_xw(n, w) = ...; }
```

then with `Iterate::Right`, adjacent threads would vary `w` (the
rightmost rank).  LayoutRight would still be the correct choice
because `w` is the rightmost index and must be contiguous for
coalescing.  LayoutLeft would make `n` contiguous instead, which
does not match that thread mapping.

The point is: **you cannot determine the correct layout from the
view dimensions alone.**  You must look at the kernel.


## Summary

The mistake:

```
"LayoutLeft is the GPU default" → use LayoutLeft everywhere
```

The correction:

```
1. Determine which index adjacent threads should vary
2. Choose the layout that makes that index contiguous
3. Set the MDRangePolicy iteration order to match
All three must agree.
```

For the AR spectral pipeline:

```
All kernels:  (window, local_index) where local_index varies
              across adjacent threads
Layout:       LayoutRight  (local_index contiguous)
MDRange:      Iterate::Right  (rightmost rank → threadIdx.x)
cuSOLVER:     flat view_1d, column-major blocks, unchanged
```

The performance difference is the difference between coalesced
and fully strided global memory access — typically 10–30× on
bandwidth-bound kernels.  The pipeline will produce correct
numerical results with either layout.  It will produce them
at very different speeds.
