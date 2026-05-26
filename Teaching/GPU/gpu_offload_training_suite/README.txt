GPU Offload Triage Training Suite
=================================

This folder contains a teaching suite for learning how to decide whether a CPU
hot loop should move to the GPU, and what kind of implementation path makes
sense after profiling.

The README is only a folder map and orientation guide. It does not contain
benchmark numbers. Measurements belong in the training document, source result
files, or freshly generated run output, because they change when the code,
hardware, drivers, compiler, libraries, or build settings change.

Top-level teaching files
------------------------

GPU_Offload_Triage_Training.html
    Main training document. Start here. It explains the offload triage story:
    profile the CPU code, identify the real mathematical operation, compare CPU
    and GPU library paths, and only then consider graph capture or native CUDA
    orchestration techniques.

Roofline_Simulator.html
    Interactive companion app for exploring the performance model behind the
    training document. Use it to experiment with operation size, arithmetic
    intensity, transfer cost, launch cost, precision, and CPU/GPU selection.

Foundations - The GPU Programming Model and Host Orchestration.md
    Companion reading for the programming model. It explains host/device roles,
    kernel launches, streams, synchronization, library calls, and why device
    residency does not mean autonomous GPU execution.

Source and build packages
-------------------------

GPU_Triage_Source.zip
    Source bundle for the examples discussed by the training document. This is
    the canonical source package for inspecting the teaching examples.

GetWorkingOnWindows.zip
    Windows-oriented helper package for getting the Kokkos/CUDA environment
    working. It is an environment/bootstrap companion, not the main lesson.

GetWorkingOnLinux.zip
    Linux-oriented helper package for unpacking, building, and running the
    examples. It includes scripts, CMake helpers, and Linux notes.

Suggested reading order
-----------------------

1. Open GPU_Offload_Triage_Training.html.
2. Read the early orientation and CPU profiling sections.
3. Read Foundations - The GPU Programming Model and Host Orchestration.md when
   the training document first discusses launches, streams, graphs, or host
   orchestration.
4. Use Roofline_Simulator.html as a companion lab while reading the performance
   model sections.
5. Inspect GPU_Triage_Source.zip only after the conceptual path is clear.
6. Use the Windows or Linux helper package only when you are ready to build and
   run the examples locally.

Maintenance rule
----------------

Keep this README stable. Do not add timing tables, measured speedups, versioned
filenames, or temporary packaging notes here. If a filename changes, update this
folder map and update links in the training document at the same time.
