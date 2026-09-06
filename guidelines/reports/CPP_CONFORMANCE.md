# C++ conformance inventory

Conformance observation: 2026-09-06. This report records existing code gaps and tool-coverage limits. The inventory checks did not rewrite product source or embedded metadata.

**Status: formatter and lexical checks completed for all 347 supported authored C++ files; all 347 AST attempts have completed, with naming available for 249 files and unavailable for 98. Metadata checked all 383 files in its finalized declared surface. These are findings and coverage limits, not a claim that FatP satisfies every new rule.**

The machine-readable [per-file inventory](cpp-conformance-inventory.json) records source hashes, mappings, diagnostic counts and examples, metadata results, tool versions, and full AST completion counts. This dated report is supporting evidence; [CURRENT_VERIFICATION](../CURRENT_VERIFICATION.md) owns current gate status.

## Scope and method

The source baseline is `9ff6d855f5ce5d949387586fad9e74cc5e121349` with the configured guideline tools. The profile's positive globs identify 146 files below `include/fat_p`, 184 below `components`, and 17 below `Teaching`: 347 supported C++ files. Vendor-owned `ThirdParty` is outside this authored inventory. Five additional authored specialist files, four CUDA `.cu` and one C `.c`, are listed below and remain explicit tool-coverage gaps.

The new root formatter and clang-format 22.1.8 passed the checker's effective-configuration validation. The formatter comparison and lexical pass use the adopted checker functions without rewriting files. The AST pass uses Clang 22.1.8, Windows x86-64 GNU/UCRT with libstdc++, C++20, `-Iinclude -iquote include/fat_p`, `-Wall -Wextra -Wpedantic -Werror`, and the existing Clang zero-variadic-macro warning exception. Individual test files use `ENABLE_TEST_APPLICATION`; the materialization regression additionally uses `-fno-elide-constructors`. Benchmark files expose the declared local `ThirdParty` include path. Frozen hash-map teaching files use their own `src/include` before the current public include paths.

This is syntax-only naming analysis of individually selected files. It does not link or run the product. A compiler failure, missing dependency, timeout, or intentionally invalid fixture leaves AST naming unavailable for that file; it is never counted as a naming pass. Formatting and lexical checks still complete independently. Inactive preprocessor paths, actual protocol/ABI exceptions, meaningful names, include-layer order, macro lifetimes, and architectural invariants require review beyond these tools.

## Completed observations

| Automated observation | Result | Interpretation |
|---|---:|---|
| Authored C++ formatter/lexical coverage | 347 / 347 | Full declared supported-extension inventory |
| Effective formatter configuration | Pass | Compatible adopted LLVM/Allman configuration |
| Files differing from the formatter | 250 | Existing formatting drift; no automatic reformat applied |
| Files with lexical diagnostics | 153 | Distinct from formatting and AST counts |
| Component-header-first diagnostics | 126 | Across 175 explicitly mapped source/test files |
| Macro-spelling diagnostics | 251 | Includes mandated external controls; requires boundary review |
| Hard-width diagnostics | 27 | Non-macro lines over 120 columns |
| Header pragma-once / using-directive diagnostics | 0 / 0 | Only the implemented lexical checks; no full header-conformance claim |
| AST attempts completed | 347 / 347 | Every supported authored file has an explicit result |
| AST naming available / unavailable | 249 / 98 | Syntax/timeout failures remain unavailable, not passes |
| Named declarations checked | 44,881 | Selected-file declarations in the measured configuration |
| AST naming diagnostics / affected files | 15,299 / 191 | Raw observations, including repeated macro expansions |
| Metadata files checked | 383 | Supported C++, scripts/build files, and declared specialist formats |
| Metadata files with first diagnostic | 377 | Checker stops at the first metadata error in each file |
| Metadata files passing these checks | 6 | Does not establish their indexed component's correctness |

Counts overlap across categories. AST diagnostics may repeat names produced by shared FatPTest assertion macros at many expansion sites; those are repeated observations whose repair belongs to the harness definition, not independent authored defects in every test. Counts are snapshots derived from the recorded inventory, not independently maintained quotas. The final metadata CLI independently reproduced the full-surface failure with exit status 1 after checking 383 files; all six newly adopted Python tool/test files pass the selected metadata gate. All 347 AST attempts are now complete; every source hash was checked again against the final working tree. Fresh review found that the supplied AST checker confused function/namespace static variables with class static data members; every prior naming-available file was rerun with the corrected classifier. Only unchanged compiler/parse-failure observations are reused, so old false-positive naming totals are excluded from the final results.

## AST availability and limits

The completed inventory has 98 files without a usable naming AST in the selected strict Windows GNU Clang configuration. Categories below describe recorded outcomes, not independently confirmed product defects:

| Outcome category | Files |
|---|---:|
| Missing Kokkos/ULIB teaching prerequisites | 2 |
| Other compiler errors in the selected configuration | 42 |
| Compiler output includes warnings promoted to errors | 21 |
| Compiler/JSON-dump deadline exceeded | 14 |
| Designated negative fixtures | 19 |

Negative fixtures were not paired with positive controls and diagnostic-matched in this style inventory, so their failure is not reported as a successful contract test. A timeout is a limit of the bounded AST collection, not evidence that the product cannot build. Other failures include platform-selection and test/harness syntax issues in this particular compiler configuration. The per-file evidence retains the actual diagnostic.

## Confirmed examples and interpretation boundaries

- [test_CheckedArithmetic.cpp](../../components/CheckedArithmetic/tests/test_CheckedArithmetic.cpp), lines 40–47, starts its includes with standard headers before its identified `CheckedArithmetic.h`. This violates the adopted component-header-first rule for an ordinary component test. The required header is identified from the public component contract and metadata, not inferred from whichever include happens to appear first.

- [ScopeGuard.h](../../include/fat_p/ScopeGuard.h), lines 368–370, defines names such as `FATP_GET_NOEXCEPT_ScopeGuardNothrowPolicy`; those project-owned macro spellings are not SCREAMING_SNAKE. Rename and update their uses coherently in a future code change. In contrast, names such as `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, and `UNW_LOCAL_ONLY` are external controls and need boundary-specific review, not blind prefix replacement. The 251 raw macro diagnostics are therefore not 251 confirmed project-macro defects.

- [TensorMaterialization metadata](../../components/Tensor/tests/test_TensorMaterialization.cpp) uses `includes_windows_h` and a `generated.mode` of `manual`. The supplied checker cannot recompute that project-specific hygiene key and accepts only genuine autogen provenance when `generated` exists. Its first reported metadata diagnostic does not mean later fields were validated.

- Existing malformed links, empty search hints, invalid namespace values, stale hygiene counts, and canonical-key-order drift remain visible in the per-file metadata record. Adoption did not normalize source comments or discard their old fields to produce a passing result.

## Metadata adoption gap

The largest first-diagnostic categories in the recorded run are key ordering (152 files), the uncomputed `includes_windows_h` hygiene field (136), missing metadata (17), invalid or empty search hints (14), and namespace values failing the declared type (8). Because validation stops at the first error, further schema, placement, path, generated-provenance, and hygiene findings can be hidden behind those first errors.

The registered metadata taxonomy includes `Foundation`, `Containers`, `Concurrency`, `Domain`, `Testing`, `Infrastructure`, and `Examples`, with roles including public/internal headers, tests, benchmarks, examples, port modules, build scripts, tooling, and header-self-containment tests. Those are observed values registered in the profile, not an invented replacement taxonomy. Existing extra fields such as `language`, `contract_tested`, `expected_error`, `backlog`, and `api_stability_notes` need an explicit preservation or schema-extension decision when their source blocks are migrated. A schema extension must have executable checks and relevant regression cases.

The declared metadata surface also contains eight files whose comment wrapper the supplied checker does not support: the five specialist C/CUDA files below, plus `tools/build.bat`, `tools/rebuild.bat`, and `tools/run_all_benchmarks.bat`. These are unavailable coverage, not passing metadata. Tooling must pass its applicable supported metadata checks; existing source drift remains an explicit backlog.

## Reviewed component-header mappings

There are 186 authored `.cpp` files in the supported inventory. Of these, 175 have an identified corresponding header. The map includes both frozen hash-map teaching test files against their own corresponding headers. The remaining 11 source files were reviewed explicitly:

| Files | Why no single-header mapping is asserted |
|---|---|
| `components/FatPHashMap/benchmarks/benchmark_FatPHashMap.cpp` | Compares FastHashMap and StableHashMap |
| `components/FlatMapSet/benchmarks/benchmark_FlatMapSet.cpp` | Compares FlatMap and FlatSet |
| `components/Tensor/tests/test_TensorMaterialization.cpp` | Exercises publication across multiple tensor operation interfaces |
| `components/FatPTest/tests/test_FatP.cpp` | Legacy aggregate runner across components; excluded from ordinary CMake target discovery |
| `components/StateMachine/tests/test_StateMachine_HeaderIncludeOrder.cpp` | Intentional reversed include-order fixture; making StateMachine first would erase the test's distinguishing condition |
| Four `Teaching/StableHashMap_Optimization/bench` and `variants` comparison `.cpp` files | Compare multiple hash-map implementations, including frozen variants |
| Two `Teaching/GPU/batched_gpu_offload_training/ar_spectral_*.cpp` files | Standalone pipelines with external dependencies, without a corresponding project public header |

This is an account of each file's role, not an exception allowing ordinary component tests to choose arbitrary include order. Multi-component comparison files still require production dependencies to be reviewed before test support. The StateMachine ordinary and standalone-header tests remain mapped; the reversed composition test preserves a separate required property.

## Specialist and semantic coverage left open

The style checker does not accept these five authored files:

- `Teaching/GPU/batched_gpu_offload_training/ar_spectral_legacy.cu`

- `Teaching/GPU/batched_gpu_offload_training/ar_spectral_original.c`

- `Teaching/GPU/batched_gpu_offload_training/ar_spectral_pipeline.cu`

- `Teaching/GPU/batched_gpu_offload_training/batched_cholesky_solve.cu`

- `Teaching/GPU/batched_gpu_offload_training/gpu_resident_pipeline.cu`

CUDA build/runtime behavior, GPU availability, real Kokkos or ULIB dependencies, and C-specific diagnostics require their actual toolchains and configurations. Header composition, ODR and binary identity, supported compiler matrices, installed consumption, semantic naming, namespace ownership, protocol spelling, runtime invariants, and test/consumer isolation are not established by this report. In particular, `FatPTest.h` remains inside the public include surface; adopting the template's test-only harness rule does not make that existing exposure disappear. The [benchmark dispatcher](../../.github/workflows/run-all-benchmarks.yml) retains push/PR triggers that dispatch timing workflows, contrary to the preserved manual-only benchmark policy; [FatP workflow ownership](../project/FATP_RULES.md#workflow-ownership) records that existing drift. Those workflow triggers were not changed by this code-gap inventory.

## Existing verification gaps discovered during inventory

These findings concern the existing component verification setup:

- [Skeleton workflow](../../.github/workflows/skeleton.yml), line 365, names `compile_fail_Skeleton_EnumValueExceedsByte.cpp`; that file does not exist. The present fixture is [compile_fail_Skeleton_EnumValueExceedsLevelWidth.cpp](../../components/Skeleton/tests/compile_fail/compile_fail_Skeleton_EnumValueExceedsLevelWidth.cpp). The shell branch treats a failed compiler invocation as a passing contract rejection, so the missing input can be reported as success. Correcting the filename alone would still leave the intended-diagnostic and valid-control requirements unmet.

- The compile-fail jobs in [Jet](../../.github/workflows/jet.yml) and [StateMachine](../../.github/workflows/state-machine.yml) treat missing/empty fixture discovery as success and accept any compiler failure while discarding diagnostic output. They do not establish that the intended contract caused rejection. The [StateMachine](../../components/StateMachine/tests/compile_fail/README_StateMachine.md) and [Stringify](../../components/Stringify/tests/compile_fail/README_Stringify.md) fixture READMEs additionally contain old repository paths. The adopted [negative-test procedure](../cpp/TESTING.md#compile-time-and-expected-failure-tests) requires valid controls and diagnostic matching.

- [Stringify's workflow](../../.github/workflows/stringify.yml) has no compile-fail job for its four existing fixtures; the current component CI aggregate therefore does not establish those negative contracts.

The AST inventory records negative fixtures as syntax-unavailable; it does not reproduce these invalid success claims. Counts or an old green workflow must not be cited as evidence that the negative contracts were correctly verified.

The supplied naming checker also omits the Clang `VarTemplateSpecializationDecl` node kind. Reviewing the authored declaring header can detect the declaration in the supplied reproducer, but selected-source instantiation coverage is not equivalent to a complete naming check. This remains an explicit tool-coverage limit, alongside inactive preprocessing paths and semantic protocol exceptions.

AST diagnostic line coordinates from the inventory's loaded checker are unverified where Clang omitted an explicit line and the source used CRLF or a UTF-8 BOM. The current tool fixes the fallback to use original source bytes; that correction does not change predicate results or counts. Published naming examples omit those line numbers; complete raw local naming diagnostics retain them as unverified. Clang's own compiler-error coordinates are unaffected and remain in the evidence. Each actionable example above is checked against current source independently.

## Follow-up work boundaries

The adopted guidance applies to subsequent changes. Future remediation should group code changes by component or owning mechanism: fix corresponding-header placement and formatting without losing include-order fixtures; classify external macros before renaming project-owned macros; migrate metadata with generated provenance and recomputed values; then address AST naming and compiler-diagnostic findings with compile/test checks and coherent pre-release caller updates. Changes that affect public names, aggregate status, ODR, ownership, or test discovery require semantic verification rather than a formatter-only pass.

Remediation follows the authorized scope of each task. The full conformance gate remains failing until its actual findings and coverage limits are resolved.
