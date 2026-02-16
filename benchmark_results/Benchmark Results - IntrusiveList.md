---
doc_id: BR-IntrusiveList-001
doc_type: "Benchmark Results"
title: "IntrusiveList"
fatp_components: ["IntrusiveList"]
topics: ["performance", "benchmarking"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "active"
---

# Benchmark Results - IntrusiveList

**Source:** `benchmark_IntrusiveList.cpp`
**Date:** February 2026
**Methodology:** Round-robin, randomized order, CPU-stabilized (local) / unstabilized (CI), median-primary

---

## Test Environments

| Property | Local (MSVC) | GCC CI | Clang CI | MSVC CI |
|----------|-------------|--------|----------|---------|
| OS | Windows 11 Pro | Ubuntu (Azure) | Ubuntu (Azure) | Windows (Azure) |
| Compiler | Windows-x64 MSVC-1950 | Linux-x64 GCC-14.2 | Linux-x64 Clang-17.0 | Windows-x64 MSVC-1944 |
| CPU | Intel Core Ultra 9 285K | Azure (shared) | Azure (shared) | Azure (shared) |
| RAM | 64 GB DDR5 | Shared tenancy | Shared tenancy | Shared tenancy |
| Measured runs | 15 | 20 | 20 | 20 |
| CPU stabilization | Yes | No | No | No |

**Competitors detected:**

| Library | Local | GCC | Clang | MSVC CI |
|---------|-------|-----|-------|---------|
| fat_p::IntrusiveList<FastPolicy> | x | x | x | x |
| fat_p::IntrusiveList<SafePolicy> | x | x | x | x |
| std::list<T*> | x | x | x | x |
| boost::intrusive::list | x | x | x | x |
| eastl::intrusive_list | x | x | x | x |
| llvm::simple_ilist | x | x | x | — |
| etl::intrusive_list | x | x | x | x |
| llvm::simple_ilist (apt install llvm-dev) | — | — | — | — |

---

## Local

**Platform:** Windows-x64 MSVC-1950 | measured=15 | seed=12345

```
================================================================================
  PUSH_BACK PERFORMANCE (N=10000)
================================================================================

[2026-02-15 19:36:03] CPU: 2469 MHz (base: 3686)
Contract: Zero allocation for IntrusiveList vs heap allocation for std::list

Measuring time to push_back 10000 pre-existing nodes.
IntrusiveList: no allocation (nodes pre-exist)
std::list: allocates node wrapper for each push

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           2.01        2.01      0.03  [2.00, 2.03] ns/op
fat_p::IntrusiveList (safe)           2.44        2.45      0.04  [2.43, 2.47] ns/op
std::list<T*>                        19.94       20.12      2.46  [18.88, 21.36] ns/op
boost::intrusive::list                1.95        2.00      0.15  [1.92, 2.07] ns/op
eastl::intrusive_list                 1.91        1.91      0.03  [1.89, 1.92] ns/op
etl::intrusive_list [!]               1.45        1.49      0.11  [1.44, 1.55] ns/op
llvm::simple_ilist                    1.99        2.20      0.72  [1.84, 2.57] ns/op

Speedup (IntrusiveList vs std::list): 8.2x

================================================================================
  REMOVE PERFORMANCE (N=10000)
================================================================================

[2026-02-15 19:36:03] CPU: 2137 MHz (base: 3686)
Contract: O(1) removal with known node reference - except ETL which is O(N)

Measuring time to remove 10000 nodes in random order.
Most intrusive lists: O(1) removal via node reference or iterator_to()
ETL: O(N) removal - searches the list (API limitation)

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           5.33        5.32      0.16  [5.23, 5.40] ns/op
fat_p::IntrusiveList (safe)           6.42        6.35      0.28  [6.20, 6.49] ns/op
std::list<T*>                        28.21       30.85      7.59  [27.01, 34.69] ns/op
boost::intrusive::list                5.20        5.27      0.22  [5.16, 5.38] ns/op
eastl::intrusive_list                 4.56        4.58      0.16  [4.49, 4.66] ns/op
etl::intrusive_list [!]           15129.69    15113.14    165.43  [15029.43, 15196.86] ns/op
llvm::simple_ilist                    5.15        5.17      0.16  [5.09, 5.25] ns/op

Speedup (IntrusiveList vs std::list): 4.4x

================================================================================
  ITERATION PERFORMANCE (N=10000)
================================================================================

[2026-02-15 19:36:06] CPU: 2248 MHz (base: 3686)
Contract: Sequential traversal with equivalent memory layout (std::deque storage)

Measuring time to iterate and sum 10000 elements.
All competitors use std::deque for node storage.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       78500.00    73313.33   9234.47  [68640.04, 77986.62] ns/iter
fat_p::IntrusiveList (safe)       82100.00    77440.00  10652.02  [72049.34, 82830.66] ns/iter
std::list<T*>                     39500.00    35860.00   6199.52  [32722.61, 38997.39] ns/iter
boost::intrusive::list            71000.00    69713.33   7999.10  [65665.23, 73761.43] ns/iter
eastl::intrusive_list             78400.00    69560.00  11457.30  [63761.81, 75358.19] ns/iter
etl::intrusive_list [!]           76600.00    70000.00  14579.49  [62621.76, 77378.24] ns/iter
llvm::simple_ilist                78300.00    85233.33  78440.51  [45536.96, 124929.71] ns/iter

Per-element: 7.85 ns/element

================================================================================
  SPLICE PERFORMANCE (N=10000)
================================================================================

[2026-02-15 19:36:06] CPU: 2211 MHz (base: 3686)
Contract: Build source list + splice to dest (measures total transfer cost)

Measuring time to build source list and splice 10000 elements.
std::list: N allocating push_backs + O(1) splice
Intrusive: N non-allocating links + O(1) splice
fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       50000.00    50893.33   5545.20  [48087.08, 53699.59] ns/splice
fat_p::IntrusiveList (safe)       82900.00    84426.67   2200.15  [83313.24, 85540.10] ns/splice
std::list<T*>                    250600.00   252833.33  11896.50  [246812.87, 258853.79] ns/splice
boost::intrusive::list            49400.00    50886.67   2647.60  [49546.79, 52226.54] ns/splice
eastl::intrusive_list             18700.00    18693.33    409.65  [18486.02, 18900.64] ns/splice
etl::intrusive_list [!]           47100.00    46506.67   2279.24  [45353.21, 47660.12] ns/splice
llvm::simple_ilist                18800.00    18993.33    587.33  [18696.11, 19290.56] ns/splice

Note: Results include cost of building source list (N push_backs).
std::list is slowest due to N allocations during setup.

================================================================================
  MEMORY OVERHEAD COMPARISON
================================================================================

Per-node memory overhead for different list implementations:

Structure                               sizeof (bytes)
-------------------------------------------------------
Raw user data (int64_t + 7 padding)                  64
fat_p::IntrusiveList (fast)                          80
fat_p::IntrusiveList (safe)                          88
std::list<T*>                                        88
boost::intrusive::list                               80
eastl::intrusive_list                                80
etl::intrusive_list [!]                              80
llvm::simple_ilist                                   80

Analysis:
- IntrusiveList adds 24 bytes per node (prev + next + owner)
- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)
- Other intrusive lists add 16 bytes (prev + next only)
- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list

================================================================================
  FREE LIST PATTERN (Pool=1000, Ops=100000)
================================================================================

[2026-02-15 19:36:06] CPU: 2322 MHz (base: 3686)
Contract: Allocate/deallocate pattern using list as free list

Simulating object pool: allocate (pop_front) and deallocate (push_back)
Pattern: 70% allocate, 30% deallocate

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           2.91        3.16      0.83  [2.74, 3.58] ns/op
fat_p::IntrusiveList (safe)           3.05        3.19      0.74  [2.82, 3.57] ns/op
std::list<T*>                         8.33        8.57      0.80  [8.16, 8.97] ns/op
boost::intrusive::list                3.06        3.07      0.11  [3.01, 3.12] ns/op
eastl::intrusive_list                 2.97        3.03      0.18  [2.94, 3.12] ns/op
etl::intrusive_list [!]               3.25        3.26      0.08  [3.22, 3.30] ns/op
llvm::simple_ilist                    3.13        3.11      0.18  [3.02, 3.20] ns/op

Speedup: 2.7x

================================================================================
  IS_LINKED CHECK PERFORMANCE (N=1000)
================================================================================

[2026-02-15 19:36:06] CPU: 2211 MHz (base: 3686)
Contract: Check if each node is in a list - O(1) vs O(N) depending on library

Measuring time to check membership for 1000 nodes (half linked).
fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers
LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API
Note: N kept small because O(N) search per node = O(N^2) total.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           0.50        0.52      0.06  [0.49, 0.55] ns/op
fat_p::IntrusiveList (safe)           0.50        0.52      0.07  [0.49, 0.55] ns/op
std::list<T*>                       271.00      275.50      7.34  [271.79, 279.21] ns/op
boost::intrusive::list                0.70        0.71      0.06  [0.68, 0.74] ns/op
eastl::intrusive_list                 0.50        0.47      0.05  [0.45, 0.50] ns/op
etl::intrusive_list [!]               0.50        0.47      0.05  [0.45, 0.50] ns/op
llvm::simple_ilist                  272.80      294.79     73.80  [257.44, 332.13] ns/op

Note: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.
Libraries requiring O(N) search: LLVM, std::list.
At N=100000, O(N) search would be ~10000x slower than O(1).
fat_p safe policy: owner pointer can identify WHICH list owns the node.

================================================================================
```

---

## GCC

**Platform:** Linux-x64 GCC-14.2 | measured=20 | seed=12345

```
================================================================================
  PUSH_BACK PERFORMANCE (N=10000)
================================================================================

[2026-02-16 03:37:45] CPU: 3248 MHz (~base: 3248)
Contract: Zero allocation for IntrusiveList vs heap allocation for std::list

Measuring time to push_back 10000 pre-existing nodes.
IntrusiveList: no allocation (nodes pre-exist)
std::list: allocates node wrapper for each push

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           1.56        1.58      0.09  [1.54, 1.62] ns/op
fat_p::IntrusiveList (safe)           1.88        1.96      0.23  [1.86, 2.06] ns/op
std::list<T*>                        18.18       18.74      1.93  [17.89, 19.59] ns/op
boost::intrusive::list                1.59        1.62      0.09  [1.58, 1.65] ns/op
eastl::intrusive_list                 1.57        1.57      0.01  [1.57, 1.58] ns/op
etl::intrusive_list [!]               3.47        3.47      0.01  [3.46, 3.47] ns/op
llvm::simple_ilist                    1.56        1.56      0.01  [1.56, 1.57] ns/op

Speedup (IntrusiveList vs std::list): 9.7x

================================================================================
  REMOVE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 03:37:45] CPU: 3159 MHz (~base: 3159)
Contract: O(1) removal with known node reference - except ETL which is O(N)

Measuring time to remove 10000 nodes in random order.
Most intrusive lists: O(1) removal via node reference or iterator_to()
ETL: O(N) removal - searches the list (API limitation)

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           6.30        6.50      0.64  [6.21, 6.78] ns/op
fat_p::IntrusiveList (safe)           7.66        7.70      0.85  [7.33, 8.07] ns/op
std::list<T*>                        38.15       38.44      2.56  [37.32, 39.56] ns/op
boost::intrusive::list                6.44        6.48      0.70  [6.18, 6.79] ns/op
eastl::intrusive_list                 8.31        8.23      0.69  [7.93, 8.53] ns/op
etl::intrusive_list [!]           17999.57    18003.57    275.70  [17882.74, 18124.40] ns/op
llvm::simple_ilist                    5.99        6.04      0.40  [5.86, 6.22] ns/op

Speedup (IntrusiveList vs std::list): 5.0x

================================================================================
  ITERATION PERFORMANCE (N=10000)
================================================================================

[2026-02-16 03:37:50] CPU: 2445 MHz (~base: 2445)
Contract: Sequential traversal with equivalent memory layout (std::deque storage)

Measuring time to iterate and sum 10000 elements.
All competitors use std::deque for node storage.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       17748.50    18030.70    840.26  [17662.44, 18398.96] ns/iter
fat_p::IntrusiveList (safe)       19336.00    20320.40   2607.95  [19177.42, 21463.38] ns/iter
std::list<T*>                     19221.00    20148.45   2300.05  [19140.41, 21156.49] ns/iter
boost::intrusive::list            16956.00    17120.90    528.80  [16889.14, 17352.66] ns/iter
eastl::intrusive_list             17042.00    18218.85   3187.77  [16821.75, 19615.95] ns/iter
etl::intrusive_list [!]           16846.50    18436.50   6179.53  [15728.20, 21144.80] ns/iter
llvm::simple_ilist                16932.00    17039.80    483.98  [16827.69, 17251.91] ns/iter

Per-element: 1.77 ns/element

================================================================================
  SPLICE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 03:37:50] CPU: 2644 MHz (~base: 2644)
Contract: Build source list + splice to dest (measures total transfer cost)

Measuring time to build source list and splice 10000 elements.
std::list: N allocating push_backs + O(1) splice
Intrusive: N non-allocating links + O(1) splice
fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       35852.00    36309.75   2083.64  [35396.55, 37222.95] ns/splice
fat_p::IntrusiveList (safe)       60252.00    60401.35   2069.04  [59494.55, 61308.15] ns/splice
std::list<T*>                    257501.00   256636.50   8154.41  [253062.67, 260210.33] ns/splice
boost::intrusive::list            34995.00    36742.00   6143.40  [34049.53, 39434.47] ns/splice
eastl::intrusive_list             15714.50    15808.50    354.39  [15653.18, 15963.82] ns/splice
etl::intrusive_list [!]           54136.50    54445.30   2008.92  [53564.85, 55325.75] ns/splice
llvm::simple_ilist                15624.50    15634.75    116.14  [15583.85, 15685.65] ns/splice

Note: Results include cost of building source list (N push_backs).
std::list is slowest due to N allocations during setup.

================================================================================
  MEMORY OVERHEAD COMPARISON
================================================================================

Per-node memory overhead for different list implementations:

Structure                               sizeof (bytes)
-------------------------------------------------------
Raw user data (int64_t + 7 padding)                  64
fat_p::IntrusiveList (fast)                          80
fat_p::IntrusiveList (safe)                          88
std::list<T*>                                        88
boost::intrusive::list                               80
eastl::intrusive_list                                80
etl::intrusive_list [!]                              80
llvm::simple_ilist                                   80

Analysis:
- IntrusiveList adds 24 bytes per node (prev + next + owner)
- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)
- Other intrusive lists add 16 bytes (prev + next only)
- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list

================================================================================
  FREE LIST PATTERN (Pool=1000, Ops=100000)
================================================================================

[2026-02-16 03:37:50] CPU: 2445 MHz (~base: 2445)
Contract: Allocate/deallocate pattern using list as free list

Simulating object pool: allocate (pop_front) and deallocate (push_back)
Pattern: 70% allocate, 30% deallocate

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           3.91        3.95      0.33  [3.81, 4.09] ns/op
fat_p::IntrusiveList (safe)           4.06        4.35      0.68  [4.05, 4.64] ns/op
std::list<T*>                         9.59       10.15      1.77  [9.37, 10.93] ns/op
boost::intrusive::list                4.45        4.57      0.35  [4.41, 4.72] ns/op
eastl::intrusive_list                 4.47        4.57      0.33  [4.42, 4.71] ns/op
etl::intrusive_list [!]               4.94        5.00      0.32  [4.85, 5.14] ns/op
llvm::simple_ilist                    4.37        4.42      0.28  [4.30, 4.54] ns/op

Speedup: 2.4x

================================================================================
  IS_LINKED CHECK PERFORMANCE (N=1000)
================================================================================

[2026-02-16 03:37:50] CPU: 2445 MHz (~base: 2445)
Contract: Check if each node is in a list - O(1) vs O(N) depending on library

Measuring time to check membership for 1000 nodes (half linked).
fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers
LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API
Note: N kept small because O(N) search per node = O(N^2) total.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           1.30        1.47      0.64  [1.19, 1.75] ns/op
fat_p::IntrusiveList (safe)           1.35        1.34      0.23  [1.24, 1.44] ns/op
std::list<T*>                       473.56      476.66      4.52  [474.68, 478.64] ns/op
boost::intrusive::list                1.12        1.15      0.15  [1.08, 1.21] ns/op
eastl::intrusive_list                 1.17        1.16      0.20  [1.07, 1.24] ns/op
etl::intrusive_list [!]               1.11        1.15      0.17  [1.08, 1.22] ns/op
llvm::simple_ilist                  654.31      654.64      8.49  [650.92, 658.36] ns/op

Note: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.
Libraries requiring O(N) search: LLVM, std::list.
At N=100000, O(N) search would be ~10000x slower than O(1).
fat_p safe policy: owner pointer can identify WHICH list owns the node.

================================================================================
```

---

## Clang

**Platform:** Linux-x64 Clang-17.0 | measured=20 | seed=12345

```
================================================================================
  PUSH_BACK PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:11:27] CPU: 3239 MHz (~base: 3239)
Contract: Zero allocation for IntrusiveList vs heap allocation for std::list

Measuring time to push_back 10000 pre-existing nodes.
IntrusiveList: no allocation (nodes pre-exist)
std::list: allocates node wrapper for each push

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           1.43        1.47      0.09  [1.43, 1.51] ns/op
fat_p::IntrusiveList (safe)           1.72        1.81      0.15  [1.74, 1.87] ns/op
std::list<T*>                        18.32       19.76      4.11  [17.96, 21.56] ns/op
boost::intrusive::list                1.58        1.80      0.38  [1.63, 1.97] ns/op
eastl::intrusive_list                 1.64        1.76      0.25  [1.65, 1.87] ns/op
etl::intrusive_list [!]               2.90        3.03      0.53  [2.79, 3.26] ns/op
llvm::simple_ilist                    1.52        1.70      0.31  [1.57, 1.84] ns/op

Speedup (IntrusiveList vs std::list): 10.6x

================================================================================
  REMOVE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:11:28] CPU: 3925 MHz (~base: 3925)
Contract: O(1) removal with known node reference - except ETL which is O(N)

Measuring time to remove 10000 nodes in random order.
Most intrusive lists: O(1) removal via node reference or iterator_to()
ETL: O(N) removal - searches the list (API limitation)

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)          11.17       11.54      1.05  [11.08, 11.99] ns/op
fat_p::IntrusiveList (safe)          11.05       11.35      0.97  [10.93, 11.78] ns/op
std::list<T*>                        39.27       39.49      3.32  [38.04, 40.95] ns/op
boost::intrusive::list               10.03       10.24      0.94  [9.83, 10.66] ns/op
eastl::intrusive_list                 8.84        9.21      0.87  [8.83, 9.59] ns/op
etl::intrusive_list [!]           18244.02    18271.60    313.00  [18134.42, 18408.77] ns/op
llvm::simple_ilist                    9.02        9.26      0.78  [8.92, 9.60] ns/op

Speedup (IntrusiveList vs std::list): 3.6x

================================================================================
  ITERATION PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:11:32] CPU: 2445 MHz (~base: 2445)
Contract: Sequential traversal with equivalent memory layout (std::deque storage)

Measuring time to iterate and sum 10000 elements.
All competitors use std::deque for node storage.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       18965.00    19008.50   2159.80  [18061.93, 19955.07] ns/iter
fat_p::IntrusiveList (safe)       20298.00    19968.25    494.01  [19751.74, 20184.76] ns/iter
std::list<T*>                     19351.00    19361.10    593.49  [19100.99, 19621.21] ns/iter
boost::intrusive::list            18094.00    18271.60   1935.95  [17423.13, 19120.07] ns/iter
eastl::intrusive_list             18028.50    17827.80    969.20  [17403.03, 18252.57] ns/iter
etl::intrusive_list [!]           18044.00    18291.25   2729.61  [17094.95, 19487.55] ns/iter
llvm::simple_ilist                18063.50    17771.60    553.97  [17528.81, 18014.39] ns/iter

Per-element: 1.90 ns/element

================================================================================
  SPLICE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:11:32] CPU: 2593 MHz (~base: 2593)
Contract: Build source list + splice to dest (measures total transfer cost)

Measuring time to build source list and splice 10000 elements.
std::list: N allocating push_backs + O(1) splice
Intrusive: N non-allocating links + O(1) splice
fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       34404.00    35043.45   2523.46  [33937.50, 36149.40] ns/splice
fat_p::IntrusiveList (safe)       58995.00    61468.15   5051.15  [59254.39, 63681.91] ns/splice
std::list<T*>                    245763.00   248487.75   4688.27  [246433.02, 250542.48] ns/splice
boost::intrusive::list            34805.00    36346.70   5531.50  [33922.41, 38770.99] ns/splice
eastl::intrusive_list             16170.50    16228.40    322.26  [16087.16, 16369.64] ns/splice
etl::intrusive_list [!]           48415.00    48914.90   2965.60  [47615.17, 50214.63] ns/splice
llvm::simple_ilist                15078.50    15858.75   2235.72  [14878.90, 16838.60] ns/splice

Note: Results include cost of building source list (N push_backs).
std::list is slowest due to N allocations during setup.

================================================================================
  MEMORY OVERHEAD COMPARISON
================================================================================

Per-node memory overhead for different list implementations:

Structure                               sizeof (bytes)
-------------------------------------------------------
Raw user data (int64_t + 7 padding)                  64
fat_p::IntrusiveList (fast)                          80
fat_p::IntrusiveList (safe)                          88
std::list<T*>                                        88
boost::intrusive::list                               80
eastl::intrusive_list                                80
etl::intrusive_list [!]                              80
llvm::simple_ilist                                   80

Analysis:
- IntrusiveList adds 24 bytes per node (prev + next + owner)
- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)
- Other intrusive lists add 16 bytes (prev + next only)
- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list

================================================================================
  FREE LIST PATTERN (Pool=1000, Ops=100000)
================================================================================

[2026-02-16 04:11:32] CPU: 2445 MHz (~base: 2445)
Contract: Allocate/deallocate pattern using list as free list

Simulating object pool: allocate (pop_front) and deallocate (push_back)
Pattern: 70% allocate, 30% deallocate

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           4.48        4.48      0.14  [4.42, 4.54] ns/op
fat_p::IntrusiveList (safe)           4.50        4.51      0.14  [4.45, 4.57] ns/op
std::list<T*>                         9.66        9.66      0.05  [9.64, 9.69] ns/op
boost::intrusive::list                4.87        4.87      0.07  [4.83, 4.90] ns/op
eastl::intrusive_list                 4.95        4.96      0.12  [4.91, 5.01] ns/op
etl::intrusive_list [!]               5.40        5.35      0.13  [5.30, 5.41] ns/op
llvm::simple_ilist                    4.83        4.81      0.13  [4.75, 4.87] ns/op

Speedup: 2.1x

================================================================================
  IS_LINKED CHECK PERFORMANCE (N=1000)
================================================================================

[2026-02-16 04:11:32] CPU: 2445 MHz (~base: 2445)
Contract: Check if each node is in a list - O(1) vs O(N) depending on library

Measuring time to check membership for 1000 nodes (half linked).
fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers
LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API
Note: N kept small because O(N) search per node = O(N^2) total.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           1.26        1.30      0.30  [1.17, 1.43] ns/op
fat_p::IntrusiveList (safe)           1.47        1.38      0.33  [1.24, 1.53] ns/op
std::list<T*>                       481.54      480.37      6.74  [477.42, 483.32] ns/op
boost::intrusive::list                1.43        1.46      0.24  [1.36, 1.56] ns/op
eastl::intrusive_list                 1.37        1.34      0.20  [1.25, 1.43] ns/op
etl::intrusive_list [!]               1.49        1.45      0.28  [1.33, 1.58] ns/op
llvm::simple_ilist                  660.53      658.03      6.24  [655.30, 660.77] ns/op

Note: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.
Libraries requiring O(N) search: LLVM, std::list.
At N=100000, O(N) search would be ~10000x slower than O(1).
fat_p safe policy: owner pointer can identify WHICH list owns the node.

================================================================================
```

---

## MSVC CI

**Platform:** Windows-x64 MSVC-1944 | measured=20 | seed=12345

```
================================================================================
  PUSH_BACK PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:55:32] CPU: 2445 MHz (base: 2445)
Contract: Zero allocation for IntrusiveList vs heap allocation for std::list

Measuring time to push_back 10000 pre-existing nodes.
IntrusiveList: no allocation (nodes pre-exist)
std::list: allocates node wrapper for each push

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           2.21        2.46      0.51  [2.24, 2.69] ns/op
fat_p::IntrusiveList (safe)           2.70        2.92      0.40  [2.75, 3.10] ns/op
std::list<T*>                        35.95       35.49      2.30  [34.48, 36.50] ns/op
boost::intrusive::list                2.37        2.42      0.12  [2.36, 2.47] ns/op
eastl::intrusive_list                 5.86        5.86      0.06  [5.84, 5.89] ns/op
etl::intrusive_list [!]               4.18        4.35      0.39  [4.18, 4.52] ns/op

Speedup (IntrusiveList vs std::list): 13.3x

================================================================================
  REMOVE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:55:32] CPU: 2445 MHz (base: 2445)
Contract: O(1) removal with known node reference - except ETL which is O(N)

Measuring time to remove 10000 nodes in random order.
Most intrusive lists: O(1) removal via node reference or iterator_to()
ETL: O(N) removal - searches the list (API limitation)

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)          13.93       14.57      1.59  [13.87, 15.26] ns/op
fat_p::IntrusiveList (safe)          13.18       14.22      2.80  [12.99, 15.44] ns/op
std::list<T*>                        61.11       63.42      6.20  [60.70, 66.14] ns/op
boost::intrusive::list               13.05       13.95      2.32  [12.93, 14.97] ns/op
eastl::intrusive_list                17.44       18.59      2.68  [17.41, 19.76] ns/op
etl::intrusive_list [!]           30999.88    31292.28   1004.78  [30851.92, 31732.64] ns/op

Speedup (IntrusiveList vs std::list): 4.6x

================================================================================
  ITERATION PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:55:40] CPU: 2445 MHz (base: 2445)
Contract: Sequential traversal with equivalent memory layout (std::deque storage)

Measuring time to iterate and sum 10000 elements.
All competitors use std::deque for node storage.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       60450.00    62165.00   4925.79  [60006.18, 64323.82] ns/iter
fat_p::IntrusiveList (safe)       55050.00    55455.00   1150.05  [54950.97, 55959.03] ns/iter
std::list<T*>                     29350.00    30005.00   3318.76  [28550.49, 31459.51] ns/iter
boost::intrusive::list            59750.00    60735.00   3253.22  [59309.21, 62160.79] ns/iter
eastl::intrusive_list             56750.00    57520.00   2946.29  [56228.73, 58811.27] ns/iter
etl::intrusive_list [!]           58250.00    59370.00   4430.77  [57428.13, 61311.87] ns/iter

Per-element: 6.04 ns/element

================================================================================
  SPLICE PERFORMANCE (N=10000)
================================================================================

[2026-02-16 04:55:40] CPU: 2445 MHz (base: 2445)
Contract: Build source list + splice to dest (measures total transfer cost)

Measuring time to build source list and splice 10000 elements.
std::list: N allocating push_backs + O(1) splice
Intrusive: N non-allocating links + O(1) splice
fat_p: fast policy splices in O(1); safe policy is O(N) due to owner updates.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)       78700.00    81030.00   7132.88  [77903.88, 84156.12] ns/splice
fat_p::IntrusiveList (safe)      145550.00   145835.00   3439.75  [144327.46, 147342.54] ns/splice
std::list<T*>                    483000.00   481930.00  15614.57  [475086.61, 488773.39] ns/splice
boost::intrusive::list            78800.00    79360.00   3021.22  [78035.89, 80684.11] ns/splice
eastl::intrusive_list             58400.00    58275.00   1946.62  [57421.85, 59128.15] ns/splice
etl::intrusive_list [!]           98700.00    98900.00   1795.02  [98113.30, 99686.70] ns/splice

Note: Results include cost of building source list (N push_backs).
std::list is slowest due to N allocations during setup.

================================================================================
  MEMORY OVERHEAD COMPARISON
================================================================================

Per-node memory overhead for different list implementations:

Structure                               sizeof (bytes)
-------------------------------------------------------
Raw user data (int64_t + 7 padding)                  64
fat_p::IntrusiveList (fast)                          80
fat_p::IntrusiveList (safe)                          88
std::list<T*>                                        88
boost::intrusive::list                               80
eastl::intrusive_list                                80
etl::intrusive_list [!]                              80

Analysis:
- IntrusiveList adds 24 bytes per node (prev + next + owner)
- std::list adds 16 bytes per node PLUS allocator overhead (~8-32 bytes)
- Other intrusive lists add 16 bytes (prev + next only)
- For 1M nodes, IntrusiveList saves ~24-48 MB vs std::list

================================================================================
  FREE LIST PATTERN (Pool=1000, Ops=100000)
================================================================================

[2026-02-16 04:55:40] CPU: 2445 MHz (base: 2445)
Contract: Allocate/deallocate pattern using list as free list

Simulating object pool: allocate (pop_front) and deallocate (push_back)
Pattern: 70% allocate, 30% deallocate

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           4.74        4.74      0.14  [4.68, 4.80] ns/op
fat_p::IntrusiveList (safe)           4.69        4.72      0.15  [4.66, 4.79] ns/op
std::list<T*>                        16.75       16.78      0.29  [16.65, 16.90] ns/op
boost::intrusive::list                4.99        4.99      0.14  [4.93, 5.05] ns/op
eastl::intrusive_list                 5.14        5.06      0.19  [4.98, 5.15] ns/op
etl::intrusive_list [!]               5.39        5.43      0.20  [5.34, 5.52] ns/op

Speedup: 3.6x

================================================================================
  IS_LINKED CHECK PERFORMANCE (N=1000)
================================================================================

[2026-02-16 04:55:40] CPU: 2445 MHz (base: 2445)
Contract: Check if each node is in a list - O(1) vs O(N) depending on library

Measuring time to check membership for 1000 nodes (half linked).
fat_p/Boost/EASTL/ETL: O(1) via link state or owner pointers
LLVM/std::list: O(N) search required - no public is_linked()/isLinked() API
Note: N kept small because O(N) search per node = O(N^2) total.

Library                             Median        Mean    Stddev  CI95
-------------------------------------------------------------------------------
fat_p::IntrusiveList (fast)           2.70        2.93      0.81  [2.58, 3.28] ns/op
fat_p::IntrusiveList (safe)           2.75        2.92      0.82  [2.56, 3.27] ns/op
std::list<T*>                       544.05      547.69     16.93  [540.27, 555.11] ns/op
boost::intrusive::list                3.10        2.96      0.90  [2.56, 3.35] ns/op
eastl::intrusive_list                 2.70        2.73      0.86  [2.35, 3.10] ns/op
etl::intrusive_list [!]               2.90        2.90      0.76  [2.56, 3.23] ns/op

Note: Libraries with O(1) membership: fat_p, Boost, EASTL, ETL.
Libraries requiring O(N) search: LLVM, std::list.
At N=100000, O(N) search would be ~10000x slower than O(1).
fat_p safe policy: owner pointer can identify WHICH list owns the node.

================================================================================
```

---

## Caveats

- Local MSVC CPU frequency was unstable during testing (65-77% of 3686 MHz base clock). Absolute ns/op values are approximate; relative rankings within each section are reliable.
- GCC and Clang CI runs are on shared-tenancy Azure runners with potential neighbor noise.
- CI benchmarks run without CPU stabilization (`FATP_BENCH_NO_STABILIZE=1`) for faster execution.
- llvm::simple_ilist (apt install llvm-dev) was not detected on MSVC CI.
