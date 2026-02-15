# FatP Dedicated Benchmark Workflows — Plan (v3: Shared Cache + Vendored ThirdParty)

## Architecture: Shared Dependency Cache

One "build deps" workflow populates the cache; all benchmark workflows restore it read-only.

### Two Sources of Competitor Libraries

**1. `ThirdParty/` (in-repo, vendored, header-only)**

Already checked out with `actions/checkout`. Just needs `-I./ThirdParty` on the compile line. Currently **no workflow passes this flag**, so these competitors are silently disabled by `__has_include`.

| Vendored Lib    | Contents                                          | Used by              |
|-----------------|---------------------------------------------------|----------------------|
| `ankerl/`       | robin_hood.h, stl.h, svector.h, unordered_dense.h | FatPHashMap, SmallVector |
| `boost/sml/`    | sml.hpp + dispatch_table.hpp                      | StateMachine         |
| `etl/`          | Embedded Template Library (~500 headers)           | IntrusiveList        |
| `NamedType/`    | named_type.hpp + supporting headers               | StrongId             |
| `sg14/`         | slot_map.h, flat_map.h, flat_set.h, hive.h, inplace_vector.h, ring_span.h | SlotMap |
| `strong_type/`  | strong_type.hpp + modifier headers                | StrongId             |
| `tinyfsm/`      | tinyfsm.hpp                                       | StateMachine         |
| `tsl/`          | robin_map, robin_hash, sparse_map, sparse_set      | FatPHashMap          |

**2. `~/thirdparty/` (external, built from source or cloned, cached)**

Heavy compiled deps + header-only libs not vendored in repo. Built once, cached with GitHub Actions cache.

---

### Cache Layout

```
~/thirdparty-headers/           # Header-only external deps (universal)
  entt/                         # skypjack/entt
  concurrentqueue/              # cameron314/concurrentqueue
  readerwriterqueue/            # cameron314/readerwriterqueue
  plf_hive/                     # mattreecebentley/plf_hive

~/thirdparty-compiled/          # Compiled deps (per toolset: gcc or clang)
  boost/                        # boostorg/boost (b2 build)
    include/
    lib/
  abseil/                       # abseil/abseil-cpp (CMake)
    include/
    lib/
  folly/                        # facebook/folly (CMake)
    include/
    lib/
  eastl/                        # electronicarts/EASTL + EABase (CMake)
    include/
    lib/
  llvm/                         # distro llvm-dev, copied
    include/
    lib/
```

### Cache Keys (4 total, shared across ALL benchmark workflows)

| Cache Key                               | Contents                        | Shared by            |
|-----------------------------------------|---------------------------------|----------------------|
| `fatp-bench-deps-gcc-v1`               | Compiled deps (gcc-14 build)    | GCC 12/13/14 jobs    |
| `fatp-bench-deps-clang-v1`             | Compiled deps (clang-17 build)  | Clang 16/17 jobs     |
| `fatp-bench-deps-headeronly-v1`         | Header-only external deps       | All Linux jobs       |
| `fatp-bench-deps-vcpkg-v1`             | vcpkg x64-windows-release       | All MSVC jobs        |

GCC 12/13/14 share one compiled cache (static libs are ABI-compatible across gcc versions on same ubuntu). Same for Clang 16/17.

---

## `build-benchmark-deps.yml` — The Cache Builder

Triggered manually. Builds everything and saves all 4 caches.

### Header-only job (runs once, universal):
```
git clone --depth 1 skypjack/entt           → ~/thirdparty-headers/entt
git clone --depth 1 cameron314/concurrentqueue → ~/thirdparty-headers/concurrentqueue
git clone --depth 1 cameron314/readerwriterqueue → ~/thirdparty-headers/readerwriterqueue
git clone --depth 1 mattreecebentley/plf_hive → ~/thirdparty-headers/plf_hive
```

### GCC compiled job (gcc-14, ubuntu-24.04):
```
Boost from source    → ~/thirdparty-compiled/boost
  b2 --with-context --with-filesystem --with-program_options
     --with-regex --with-system --with-thread
Abseil from source   → ~/thirdparty-compiled/abseil
Folly from source    → ~/thirdparty-compiled/folly (CMAKE_PREFIX_PATH=boost)
EASTL from source    → ~/thirdparty-compiled/eastl
LLVM from distro     → ~/thirdparty-compiled/llvm
```

### Clang compiled job (clang-17, ubuntu-22.04):
```
Same as GCC but with toolset=clang / CMAKE_CXX_COMPILER=clang++-17
```

### MSVC vcpkg job (windows-latest):
```
vcpkg install (x64-windows-release):
  abseil boost-unordered boost-container boost-intrusive boost-lockfree
  boost-pool boost-asio folly llvm fmt entt tbb
```

---

## Dependency Matrix (complete)

| Component            | In-repo (`-I./ThirdParty`) | External cached | System pkg |
|----------------------|---------------------------|-----------------|------------|
| **FatPHashMap**      | ankerl, tsl               | boost, abseil, folly, llvm | fmt,glog,gflags,etc |
| **SmallVector**      | ankerl                    | boost, abseil, folly, llvm, eastl | fmt,glog,gflags,etc |
| **FlatMapSet**       |                           | boost, abseil, folly | fmt,glog,gflags,etc |
| **SparseSet**        |                           | abseil, llvm, entt |  |
| **IntrusiveList**    | etl                       | boost, llvm, eastl |  |
| **BitSet**           |                           | boost, llvm |  |
| **SlotMap**          | sg14                      | boost, entt, plf_hive |  |
| **ObjectPool**       |                           | boost, eastl |  |
| **StateMachine**     | boost/sml, tinyfsm        | boost |  |
| **CircularBuffer**   |                           | boost, moodycamel(rwq) |  |
| **LockFreeContainers** |                         | boost, moodycamel(cq) |  |
| **WorkQueue**        |                           | boost, moodycamel(cq) |  |
| **ThreadPool**       |                           | boost | tbb |
| **PolicyIterator**   |                           | boost |  |
| **StrongId**         | NamedType, strong_type    | boost |  |
| **ServiceLocator**   |                           | entt |  |
| **AlignedVector**    |                           | boost |  |
| **AllocationStrategies** |                       | boost |  |

---

## Benchmark Workflow Template

Every benchmark workflow follows this pattern:

```yaml
name: {Component} Benchmarks
on:
  workflow_dispatch:
    inputs:
      batches: { default: '20', type: string }
      target_work: { default: '100000', type: string }

env:
  BENCH_SRC: components/{Component}/benchmarks/benchmark_{X}.cpp

jobs:
  benchmarks-gcc:
    name: GCC-${{ matrix.version }} C++20
    runs-on: ubuntu-24.04
    strategy: { matrix: { version: [12, 13, 14] } }
    steps:
      - uses: actions/checkout@v4          # gets ThirdParty/ for free

      - name: Install system deps
        run: sudo apt-get update && sudo apt-get install -y g++-${{ matrix.version }} ...

      - name: Restore header-only deps
        uses: actions/cache/restore@v4
        with: { path: ~/thirdparty-headers, key: fatp-bench-deps-headeronly-v1 }

      - name: Restore compiled deps
        uses: actions/cache/restore@v4
        with: { path: ~/thirdparty-compiled, key: fatp-bench-deps-gcc-v1 }

      - name: Build benchmark
        run: |
          g++-${{ matrix.version }} -std=c++20 -O3 -DNDEBUG -march=native \
            -I./include/fat_p \
            -I./ThirdParty \
            -I$HOME/thirdparty-headers/entt/single_include \
            -I$HOME/thirdparty-compiled/boost/include \
            -I$HOME/thirdparty-compiled/abseil/include \
            ...
            ${{ env.BENCH_SRC }} -o bench_bin -pthread \
            $BOOST_LIBS $ABSEIL_LIBS ...

      # Run + Upload same as before
```

Key point: **`-I./ThirdParty`** enables the vendored competitors that are currently invisible.

---

## Implementation Plan

### Phase 0 — Foundation
1. **Create `build-benchmark-deps.yml`** — the cache builder
2. **Refactor `fatp-hash-map-benchmarks.yml`** — switch to shared cache + add `-I./ThirdParty`

### Phase 1 — Heavy competitors
3. `smallvector-benchmarks.yml` — boost, abseil, folly, llvm, eastl, ankerl (6+ competitors)
4. `flatmapset-benchmarks.yml` — boost, abseil, folly
5. `sparseset-benchmarks.yml` — abseil, llvm, entt

### Phase 2 — Medium
6. `intrusivelist-benchmarks.yml` — boost, llvm, eastl, etl
7. `statemachine-benchmarks.yml` — boost, sml, tinyfsm
8. `objectpool-benchmarks.yml` — boost, eastl
9. `bitset-benchmarks.yml` — boost, llvm
10. `slotmap-benchmarks.yml` — boost, entt, sg14, plf_hive

### Phase 3 — Concurrent structures
11. `threadpool-benchmarks.yml` — boost, tbb
12. `workqueue-benchmarks.yml` — boost, moodycamel
13. `lockfreecontainers-benchmarks.yml` — boost, moodycamel
14. `circularbuffer-benchmarks.yml` — boost, moodycamel(rwq)

### Phase 4 — Lightweight
15. `policyiterator-benchmarks.yml` — boost headers
16. `servicelocator-benchmarks.yml` — entt
17. `strongid-benchmarks.yml` — boost, NamedType, strong_type (all from ThirdParty/)
18. `alignedvector-benchmarks.yml` — boost headers
19. `allocationstrategies-benchmarks.yml` — boost headers

### Cleanup
- Update `run-all-benchmarks.yml`: add `-I./ThirdParty` to all builds, add missing components
- Remove embedded benchmark jobs from `thread-pool.yml` and `work-queue.yml`

---

## Savings Summary

| Metric                      | Per-workflow deps           | Shared cache          |
|-----------------------------|-----------------------------|-----------------------|
| Cold build for 1 workflow   | ~15 min                     | ~15 min (once)        |
| Cold build for 19 workflows | ~15 min × 19 = 4.75 hours  | ~15 min (once)        |
| Warm run per job            | ~1-2 min                    | ~1-2 min (same)       |
| Cache storage               | ~500 MB × 19 × 5 compilers | ~500 MB × 2 toolsets  |
| Adding new benchmark        | Write workflow + cold build  | Write workflow, instant |
