# Fat-P CI Workflow Remediation Plan

**Date:** February 4, 2026  
**Scope:** 46 of 50 workflow files require fixes  
**Priority:** High - MSVC builds will fail on most components

---

## Executive Summary

A survey of the `.github/workflows/` directory reveals that **92% of workflow files are broken** and will fail CI. The root cause is outdated templates that reference the old directory structure (`FAT_P/FAT_P/...`) and lack recent requirements (C++23 testing, MSVC warning suppressions, ThreadSanitizer).

**Only 4 files are correct:**
- `aligned-vector.yml`
- `fatp-hash-map.yml`
- `object-pool.yml`
- `state-machine.yml`

---

## Issues Identified

### Issue 1: Wrong Directory Paths (39 files)

**Symptom:** `file not found` errors on all platforms

**Root Cause:** Workflows use deprecated `FAT_P/FAT_P/...` paths

**Fix Required:**
```yaml
# OLD (broken)
/I.\FAT_P\FAT_P\fat_p FAT_P\FAT_P\tests\test_Component.cpp

# NEW (correct)
/I.\include\fat_p components\Component\tests\test_Component.cpp
```

**Affected Files (39):**
```
fatp-binary.yml          fatp-cbor-stream.yml     fatp-cbor.yml
fatp-json-stream.yml     fatp-json.yml            fatp-type-traits.yml
feature-manager.yml      flat-map.yml             flat-set.yml
floating-point-comparison.yml  hpc-vector.yml     id-generator.yml
intrusive-list.yml       json-lite.yml            json-stream-lite.yml
lock-free-queue.yml      lock-free-ring-buffer.yml  pipe-operator.yml
policy-iterator.yml      rate-limiter.yml         reflection.yml
scope-guard-expected.yml scope-guard.yml          service-locator.yml
signal.yml               simd-vector.yml          slot-map.yml
small-vector.yml         sorted-container.yml     sparse-set.yml
stacktrace.yml           string-pool.yml          stringify.yml
strong-id.yml            tensor.yml               thread-pool.yml
type-traits.yml          value-guard.yml          view-lifetime-tracking.yml
```

---

### Issue 2: Missing MSVC `/wd4324` Warning Suppression (41 files)

**Symptom:** MSVC build fails with `C4324: structure was padded due to alignment specifier`

**Root Cause:** `ConcurrencyPolicies.h` uses `alignas(64)` for cache-line alignment. MSVC warns about intentional padding, and `/WX` treats warnings as errors.

**Fix Required:**
```yaml
# OLD (broken)
cl /std:c++20 /W4 /WX /EHsc ...

# NEW (correct)
cl /std:c++20 /W4 /WX /wd4324 /EHsc ...
```

**Affected Files (41):**
```
allocation-strategies.yml  fatp-binary.yml          fatp-cbor-stream.yml
fatp-cbor.yml             fatp-json-stream.yml     fatp-json.yml
fatp-type-traits.yml      feature-manager.yml      flat-map.yml
flat-set.yml              floating-point-comparison.yml  header-hygiene.yml
hpc-vector.yml            id-generator.yml         intrusive-list.yml
json-lite.yml             json-stream-lite.yml     lock-free-queue.yml
lock-free-ring-buffer.yml pipe-operator.yml        rate-limiter.yml
reflection.yml            scope-guard-expected.yml scope-guard.yml
service-locator.yml       signal.yml               simd-vector.yml
slot-map.yml              small-vector.yml         sorted-container.yml
sparse-set.yml            stacktrace.yml           state-machine.yml
string-pool.yml           stringify.yml            strong-id.yml
tensor.yml                thread-pool.yml          type-traits.yml
value-guard.yml           view-lifetime-tracking.yml
```

---

### Issue 3: Missing C++23 Testing (42 files)

**Symptom:** No forward-compatibility testing; C++23 regressions go undetected

**Root Cause:** Workflows only test C++20

**Fix Required:**
```yaml
# OLD (C++20 only)
matrix:
  include:
    - version: 13
      std: 20

# NEW (C++20 + C++23)
matrix:
  include:
    - version: 13
      std: 20
    - version: 14
      std: 23
```

**Compiler Matrix:**
| Compiler | C++20 | C++23 |
|----------|-------|-------|
| GCC | 13 | 14 |
| Clang | 16 | 17 |
| MSVC | `/std:c++20` | `/std:c++latest` |

**Affected Files (42):**
```
allocation-strategies.yml  ci_verify.yml            fatp-binary.yml
fatp-cbor-stream.yml      fatp-cbor.yml            fatp-json-stream.yml
fatp-json.yml             fatp-type-traits.yml     fatp_meta_compliance.yml
feature-manager.yml       flat-map.yml             flat-set.yml
floating-point-comparison.yml  header-hygiene.yml  hpc-vector.yml
id-generator.yml          intrusive-list.yml       json-lite.yml
json-stream-lite.yml      lock-free-queue.yml      lock-free-ring-buffer.yml
memory-mapped-file.yml    numa-allocator.yml       pipe-operator.yml
policy-iterator.yml       rate-limiter.yml         reflection.yml
scope-guard-expected.yml  scope-guard.yml          service-locator.yml
signal.yml                simd-vector.yml          sliding-file-window.yml
slot-map.yml              sorted-container.yml     sparse-set.yml
stacktrace.yml            string-pool.yml          stringify.yml
strong-id.yml             tensor.yml               thread-pool.yml
type-traits.yml           value-guard.yml          view-lifetime-tracking.yml
```

---

### Issue 4: Obsolete C++17 Testing (3 files)

**Symptom:** Wasted CI time; C++17 is no longer supported

**Root Cause:** Old matrix includes C++17 standard

**Fix Required:** Remove all `std: 17` entries and `/std:c++17` flags

**Affected Files (3):**
```
allocation-strategies.yml
policy-iterator.yml
small-vector.yml
```

---

### Issue 5: Missing ThreadSanitizer (40 files)

**Symptom:** Data races and thread-safety bugs go undetected

**Root Cause:** TSan job not included in workflow

**Fix Required:** Add `sanitizer-tsan` job and include in `ci-success` needs

**Affected Files (40):**
```
fatp-binary.yml          fatp-cbor-stream.yml     fatp-cbor.yml
fatp-json-stream.yml     fatp-json.yml            fatp-type-traits.yml
feature-manager.yml      flat-map.yml             flat-set.yml
floating-point-comparison.yml  hpc-vector.yml     id-generator.yml
intrusive-list.yml       json-lite.yml            json-stream-lite.yml
lock-free-queue.yml      lock-free-ring-buffer.yml  memory-mapped-file.yml
numa-allocator.yml       pipe-operator.yml        rate-limiter.yml
reflection.yml           scope-guard-expected.yml scope-guard.yml
service-locator.yml      signal.yml               simd-vector.yml
sliding-file-window.yml  slot-map.yml             small-vector.yml
sorted-container.yml     sparse-set.yml           stacktrace.yml
string-pool.yml          strong-id.yml            tensor.yml
thread-pool.yml          type-traits.yml          value-guard.yml
view-lifetime-tracking.yml
```

**Note:** Some non-concurrent components (e.g., `stringify`, `type-traits`) may not strictly require TSan, but including it uniformly ensures consistency and catches unexpected thread-safety issues.

---

## Remediation Plan

### Phase 1: Critical Path Fixes (Immediate)

**Goal:** Make all workflows pass CI

**Priority Order:**
1. Fix paths (`FAT_P/FAT_P` → `include/fat_p` + `components/...`)
2. Add `/wd4324` to all MSVC builds
3. Ensure `advapi32.lib` is linked in MSVC builds

**Estimated Files:** 39 files with path issues (highest priority)

### Phase 2: Standard Compliance (Week 1)

**Goal:** Align with C++20/C++23 policy

**Tasks:**
1. Remove C++17 from 3 files
2. Add C++23 testing matrix to 42 files
3. Update GCC to 13/14 matrix
4. Update Clang to 16/17 matrix
5. Update MSVC to use `$stdFlag` pattern with C++23 support

### Phase 3: Sanitizer Coverage (Week 1-2)

**Goal:** Full sanitizer coverage on all components

**Tasks:**
1. Add `sanitizer-tsan` job to 40 files
2. Verify `sanitizer-tsan` is in `ci-success` needs list
3. Verify ASan and UBSan jobs exist and are in gate

### Phase 4: Validation (Week 2)

**Goal:** Verify all workflows pass

**Tasks:**
1. Run each workflow manually via `workflow_dispatch`
2. Fix any remaining issues
3. Document any component-specific exceptions

---

## Implementation Approach

### Option A: Batch Script (Recommended)

Create a script that programmatically fixes all files:

```bash
#!/bin/bash
for f in .github/workflows/*.yml; do
  # Fix paths
  sed -i 's|FAT_P/FAT_P/fat_p|include/fat_p|g' "$f"
  sed -i 's|FAT_P/FAT_P/tests|components/.../tests|g' "$f"
  sed -i 's|FAT_P/FAT_P/benchmarks|components/.../benchmarks|g' "$f"
  
  # Fix MSVC warnings
  sed -i 's|/W4 /WX /EHsc|/W4 /WX /wd4324 /EHsc|g' "$f"
  
  # ... etc
done
```

**Pros:** Fast, consistent  
**Cons:** Requires careful regex; component names vary

### Option B: Template Regeneration

1. Create a canonical template from `object-pool.yml`
2. Generate each workflow from template with component-specific substitutions
3. Review and commit

**Pros:** Guaranteed consistency  
**Cons:** More upfront work; loses any component-specific customizations

### Option C: Manual Fix Per File

Fix each file individually, using the CI Workflow Style Guide v3.0 as reference.

**Pros:** Can handle edge cases  
**Cons:** Time-consuming; 46 files × ~15 min = ~11 hours

---

## Recommended Approach

**Use Option A (Batch Script) for Phase 1, then Option B (Template) for Phases 2-3.**

1. Write a sed/awk script to fix the critical path issues (paths, `/wd4324`)
2. Validate with a sampling of 5-10 workflows
3. Create a canonical template for the full fix
4. Regenerate all workflows from template
5. Diff against originals to catch any lost customizations
6. Test via `workflow_dispatch`

---

## Files by Fix Complexity

### Simple Fixes (sed replacement only) - 30 files

These files just need path fixes and MSVC flag additions:

```
fatp-binary, fatp-cbor, fatp-cbor-stream, fatp-json, fatp-json-stream,
fatp-type-traits, flat-set, floating-point-comparison, hpc-vector,
id-generator, intrusive-list, json-lite, json-stream-lite, pipe-operator,
rate-limiter, reflection, scope-guard, scope-guard-expected, service-locator,
signal, slot-map, sorted-container, sparse-set, stacktrace, string-pool,
stringify, strong-id, type-traits, value-guard, view-lifetime-tracking
```

### Moderate Fixes (matrix changes needed) - 12 files

These files need path fixes + C++23 matrix + possibly benchmark updates:

```
feature-manager, flat-map, lock-free-queue, lock-free-ring-buffer,
numa-allocator, memory-mapped-file, simd-vector, sliding-file-window,
small-vector, tensor, thread-pool, policy-iterator
```

### Complex Fixes (multiple issues + C++17 removal) - 4 files

These files have multiple overlapping issues:

```
allocation-strategies.yml  - paths, C++17 removal, C++23 add, /wd4324
policy-iterator.yml        - paths, C++17 removal, C++23 add, TSan
small-vector.yml           - paths, C++17 removal, C++23 add, TSan
state-machine.yml          - /wd4324 only (otherwise correct)
```

---

## Success Criteria

- [ ] All 50 workflows pass when triggered via `workflow_dispatch`
- [ ] No `FAT_P/FAT_P` paths remain in any workflow
- [ ] All workflows test C++20 (GCC-13, Clang-16, MSVC)
- [ ] All workflows test C++23 (GCC-14, Clang-17, MSVC `/std:c++latest`)
- [ ] No C++17 testing in any workflow
- [ ] All MSVC builds include `/wd4324` and link `advapi32.lib`
- [ ] All workflows include ASan, UBSan, TSan jobs
- [ ] All sanitizer jobs are in `ci-success` needs list
- [ ] CI Workflow Style Guide v3.0 is the authoritative reference

---

## Appendix: Correct File Reference

Use these files as templates:

| File | Why It's Correct |
|------|------------------|
| `object-pool.yml` | Full template: paths, C++20/23, all sanitizers, benchmarks |
| `fatp-hash-map.yml` | Multi-component example (FastHashMap + StableHashMap) |
| `aligned-vector.yml` | Simple component with benchmarks |
| `state-machine.yml` | Correct except missing `/wd4324` |

---

*Fat-P CI Workflow Remediation Plan — February 2026*
