# ServiceLocator.h Dependency Tree

**Date:** 2026-02-03  
**File:** `include/fat_p/ServiceLocator.h`  
**Layer:** Domain

---

## Visual Dependency Tree

```
ServiceLocator.h
├── ConcurrencyPolicies.h
│   └── CppFeatureDetection.h ─────────────────────────────┐
│                                                          │
├── enforce.h                                              │
│   ├── enforce_enforcers.h                                │
│   │   ├── CppFeatureDetection.h ◄────────────────────────┤
│   │   ├── FatPConfig.h (leaf)                            │
│   │   ├── ContractException.h (leaf)                     │
│   │   ├── enforce_raisers.h ◄────────────────────────────┤
│   │   └── Stringify.h                                    │
│   │       ├── CppFeatureDetection.h ◄────────────────────┤
│   │       └── Concepts.h                                 │
│   │           └── CppFeatureDetection.h ◄────────────────┤
│   │                                                      │
│   ├── enforce_predicates.h                               │
│   │   ├── Concepts.h                                     │
│   │   │   └── CppFeatureDetection.h ◄────────────────────┤
│   │   └── ConstexprUtilities.h                           │
│   │       └── CppFeatureDetection.h ◄────────────────────┤
│   │                                                      │
│   ├── enforce_raiser_selector.h                          │
│   │   ├── enforce_contextual_policies.h (leaf)           │
│   │   ├── enforce_predicates.h (see above)               │
│   │   └── enforce_raisers.h (see below)                  │
│   │                                                      │
│   └── enforce_raisers.h                                  │
│       ├── ContractException.h (leaf)                     │
│       └── Expected.h ◄───────────────────────────────────┤
│                                                          │
├── Expected.h                                             │
│   └── CppFeatureDetection.h ◄────────────────────────────┘
│
└── StableHashMap.h
    ├── AllocationStrategies.h (leaf)
    └── SimdDetection.h
        └── PlatformDetection.h (leaf)
```

---

## Dependency Summary

### Direct Dependencies (4)

| Header | Layer | Purpose |
|--------|-------|---------|
| ConcurrencyPolicies.h | Foundation | `SharedMutexPolicy`, `NullMutexPolicy` |
| enforce.h | Foundation | `FATP_ENFORCE` contract assertions |
| Expected.h | Foundation | `Expected<T,E>` error handling |
| StableHashMap.h | Containers | Registry storage with pointer stability |

### Full Transitive Closure (16 unique headers)

| # | Header | Layer | Leaf? |
|---|--------|-------|-------|
| 1 | ServiceLocator.h | Domain | — |
| 2 | ConcurrencyPolicies.h | Foundation | |
| 3 | enforce.h | Foundation | |
| 4 | Expected.h | Foundation | |
| 5 | StableHashMap.h | Containers | |
| 6 | CppFeatureDetection.h | Detection | ✓ |
| 7 | enforce_enforcers.h | Foundation | |
| 8 | enforce_predicates.h | Foundation | |
| 9 | enforce_raiser_selector.h | Foundation | |
| 10 | enforce_raisers.h | Foundation | |
| 11 | enforce_contextual_policies.h | Foundation | ✓ |
| 12 | FatPConfig.h | Detection | ✓ |
| 13 | ContractException.h | Foundation | ✓ |
| 14 | Stringify.h | Foundation | |
| 15 | Concepts.h | Foundation | |
| 16 | ConstexprUtilities.h | Foundation | |
| 17 | AllocationStrategies.h | Foundation | ✓ |
| 18 | SimdDetection.h | Detection | |
| 19 | PlatformDetection.h | Detection | ✓ |

### Leaf Headers (No Fat-P Dependencies)

```
CppFeatureDetection.h    (Detection layer - root)
PlatformDetection.h      (Detection layer)
FatPConfig.h             (Detection layer)
ContractException.h      (Foundation layer)
AllocationStrategies.h   (Foundation layer)
enforce_contextual_policies.h (Foundation layer)
```

---

## Layer Breakdown

```
Detection Layer (4):
  └── CppFeatureDetection.h
  └── PlatformDetection.h
  └── SimdDetection.h
  └── FatPConfig.h

Foundation Layer (11):
  ├── ConcurrencyPolicies.h
  ├── enforce.h
  │   ├── enforce_enforcers.h
  │   ├── enforce_predicates.h
  │   ├── enforce_raiser_selector.h
  │   ├── enforce_raisers.h
  │   └── enforce_contextual_policies.h
  ├── Expected.h
  ├── ContractException.h
  ├── Stringify.h
  ├── Concepts.h
  ├── ConstexprUtilities.h
  └── AllocationStrategies.h

Containers Layer (1):
  └── StableHashMap.h

Domain Layer (1):
  └── ServiceLocator.h
```

---

## Standard Library Dependencies

### ServiceLocator.h Direct

```cpp
#include <atomic>              // std::atomic (statistics)
#include <condition_variable>  // Singleton factory sync
#include <cstddef>             // size_t
#include <cstdint>             // uint64_t, uint8_t
#include <exception>           // std::exception_ptr
#include <functional>          // std::function, std::reference_wrapper
#include <memory>              // std::shared_ptr, std::unique_ptr
#include <mutex>               // std::mutex, std::unique_lock
#include <ostream>             // operator<< overloads
#include <string>              // std::string
#include <string_view>         // std::string_view
#include <thread>              // std::this_thread::get_id()
#include <type_traits>         // Type traits
#include <utility>             // std::forward, std::move
```

### Full Standard Library Set (via transitives)

```
<algorithm>          (StableHashMap)
<array>              (SimdDetection)
<atomic>             (ServiceLocator, ConcurrencyPolicies)
<bit>                (StableHashMap)
<cassert>            (Expected)
<charconv>           (Stringify)
<cmath>              (ConstexprUtilities)
<compare>            (Expected)
<concepts>           (Concepts)
<condition_variable> (ServiceLocator)
<cstddef>            (multiple)
<cstdint>            (multiple)
<cstdlib>            (Stringify)
<exception>          (Expected, ContractException)
<expected>           (Expected, conditional C++23)
<functional>         (ServiceLocator)
<initializer_list>   (StableHashMap)
<iosfwd>             (ConstexprUtilities)
<iterator>           (StableHashMap)
<limits>             (ConstexprUtilities)
<memory>             (multiple)
<mutex>              (ServiceLocator)
<new>                (AllocationStrategies)
<optional>           (Stringify)
<ostream>            (ServiceLocator, enforce)
<shared_mutex>       (ConcurrencyPolicies)
<sstream>            (enforce_raisers)
<stdexcept>          (Expected, ContractException)
<string>             (multiple)
<string_view>        (multiple)
<thread>             (ServiceLocator)
<tuple>              (Stringify)
<type_traits>        (multiple)
<utility>            (multiple)
<variant>            (Expected, conditional)
<vector>             (StableHashMap)
```

---

## Dependency Graph Statistics

| Metric | Value |
|--------|-------|
| Direct Fat-P dependencies | 4 |
| Total unique Fat-P headers | 19 |
| Leaf headers (no Fat-P deps) | 6 |
| Maximum depth | 5 |
| Circular dependencies | 0 |

### Depth Analysis

```
Depth 0: ServiceLocator.h
Depth 1: ConcurrencyPolicies, enforce, Expected, StableHashMap
Depth 2: CppFeatureDetection, enforce_*, AllocationStrategies, SimdDetection
Depth 3: FatPConfig, ContractException, Stringify, Concepts, ConstexprUtilities, PlatformDetection
Depth 4: (Concepts, Stringify → CppFeatureDetection)
Depth 5: (ConstexprUtilities → CppFeatureDetection)
```

---

## Include Order (Topologically Sorted)

For manual inclusion or build verification:

```cpp
// Detection layer (no deps)
#include "CppFeatureDetection.h"
#include "PlatformDetection.h"
#include "FatPConfig.h"

// Detection layer (with deps)
#include "SimdDetection.h"          // → PlatformDetection

// Foundation layer (leaves)
#include "AllocationStrategies.h"
#include "ContractException.h"
#include "enforce_contextual_policies.h"

// Foundation layer (with deps)
#include "Concepts.h"               // → CppFeatureDetection
#include "ConstexprUtilities.h"     // → CppFeatureDetection
#include "Stringify.h"              // → CppFeatureDetection, Concepts
#include "Expected.h"               // → CppFeatureDetection
#include "enforce_predicates.h"     // → Concepts, ConstexprUtilities
#include "enforce_raisers.h"        // → ContractException, Expected
#include "enforce_raiser_selector.h"// → enforce_contextual_policies, enforce_predicates, enforce_raisers
#include "enforce_enforcers.h"      // → CppFeatureDetection, FatPConfig, ContractException, enforce_raisers, Stringify
#include "enforce.h"                // → enforce_*
#include "ConcurrencyPolicies.h"    // → CppFeatureDetection

// Containers layer
#include "StableHashMap.h"          // → AllocationStrategies, SimdDetection

// Domain layer
#include "ServiceLocator.h"         // → ConcurrencyPolicies, enforce, Expected, StableHashMap
```

---

*End of Document*
