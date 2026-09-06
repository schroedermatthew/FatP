# C++ build and CI

Extends [Workflow](../modules/WORKFLOW.md) for C++ compilation, linking, testing,
and distribution. Actual commands and supported configurations belong in the
[project profile](../PROJECT_PROFILE.md), not copied machine recipes.

## Define the compilation contract

- Record the language mode, compiler, standard-library implementation, target
  architecture, runtime library, and relevant build definitions. A compiler version
  alone does not describe the environment consuming a C++ interface.
- Separate language support from library support. A parser accepting a feature
  does not prove the selected standard library supplies the required facility.
- Check every compiler/configuration promised by the support policy. A successful
  GCC build does not establish MSVC compatibility, and two compiler frontends
  using one standard library do not cover all standard-library differences.
- Keep ABI-affecting settings consistent across binary boundaries: layout macros,
  packing/alignment, calling conventions, runtime-library selection, exception/RTTI
  settings where relevant, and library debug modes. Document allowed variation.
- Treat floating-point modes, architecture-specific instructions, and accelerator
  toolchains as observable configuration choices. Test the behavior actually
  promised under those choices, including backend availability.

## Make dependencies explicit

Use target-level declarations in the chosen build system. Headers used by public
interfaces must bring their actual consumer requirements; implementation-only
settings should not leak into every dependent target. Apply project diagnostic
policy without imposing unrelated warning flags on third-party consumers.

When the project uses CMake, express dependencies and configuration with its
target_* commands and imported targets. PUBLIC requirements affect the target
and consumers, PRIVATE affects the target, and INTERFACE describes consumers'
requirements. Set scopes from actual use, not convenience. Header-only libraries
still have usage requirements and need consumer compile checks.
[CMake build and usage requirements](https://cmake.org/cmake/help/latest/manual/cmake-buildsystem.7.html#build-specification-and-usage-requirements).

Declare required language features on the relevant targets; do not rely on a
compiler's changing default mode. If the support policy needs an exact mode or
extension policy, configure and test it explicitly rather than mistaking a minimum
feature requirement for an exact standard selection.
[CMake compile features](https://cmake.org/cmake/help/latest/command/target_compile_features.html).

This CMake guidance is conditional. It does not require replacing another build
system or adopting a particular preset, generator, or minimum CMake version.

## Diagnostics and analysis

Authored code is warning-clean with warnings treated as errors: GCC/Clang use
-Wall -Wextra -Wpedantic -Werror; MSVC uses /W4 /WX where supported.
Third-party consumer warning policy remains separate. Record justified exceptions. Investigate a warning
before suppressing it. Keep justified suppressions narrow, version-aware when
necessary, and tied to the code or dependency that requires them.

Keep formatter and static-analysis configuration executable in the real project.
Do not add an empty configuration file and call the policy enforced. When a
configuration references checks unsupported by a tool version, detect that mismatch
instead of silently claiming full analysis coverage.

Check an optimized configuration when optimization-sensitive behavior is in scope;
a debug-only run can miss undefined-behavior symptoms and differences in assertions
or checked-library modes. Do not call a build free of undefined behavior merely
because the compiler emitted no warnings.

## Verification stages

| Stage | What to verify; what it does not establish |
|---|---|
| Configure/generate | Dependencies, options, and targets are found; source has not necessarily compiled |
| Syntax or compile-only | Relevant source instantiates and passes diagnostics; final linkage and runtime behavior remain unchecked |
| Link | Required definitions and libraries resolve; ODR correctness and runtime behavior are not proven |
| Execute tests | Discovered tests ran in the stated configuration; unselected cases and platforms remain untested |
| Instrumented execution | Selected runtime checks ran with compatible instrumentation; absence of a report is not universal safety proof |
| Install/export and consume | A consumer outside the source tree can use the delivered interface under the stated conditions |

Include header self-containment and supported composition checks from
[Headers and linkage](HEADERS_AND_LINKAGE.md). Disable accidental PCH/unity support
for those checks. When C++ modules are used, build and consume their interfaces
using the project's supported module workflow instead of treating textual include
checks as complete coverage.

Compile or validate shipped examples and benchmark targets when their buildability
is part of the deliverable, even if lengthy benchmark execution is scheduled
separately. Derive discovery from the actual build/test registration; do not
maintain an unrelated list that can omit new targets.

## Sanitizers and runtime dependencies

Check sanitizer support for the actual compiler, runtime, architecture, and runner.
Instrumentation must reach the affected code and the final link/runtime setup.
For Clang AddressSanitizer, use the compiler driver for the final link so the
appropriate runtime can be supplied; follow its documented instrumentation steps.
[AddressSanitizer documentation](https://clang.llvm.org/docs/AddressSanitizer.html#usage).

Keep incompatible sanitizer configurations separate and do not imply every tool
is available on every platform. Record runtime-loader failures, unsupported
options, exclusions, and uninstrumented dependencies as limits, not clean reports.
Real dependency or hardware tests remain distinct from stubs and emulators.

## Packaging and acceptance

Build a small external consumer when distributing a library. Use the installed or
exported target and public includes rather than private source-tree paths. Check
transitive dependencies, generated headers, symbols, and runtime assets required
to use the delivered package.

Required CI jobs must cover the configurations and stages promised by the profile.
The aggregate gate follows the general workflow module; no skipped compiler or
failed setup step silently counts as a passing C++ test. Record the exact source
state and what was compiled, linked, run, or left unavailable.
