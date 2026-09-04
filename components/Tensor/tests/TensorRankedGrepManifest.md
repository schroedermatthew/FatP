# TensorRanked architecture grep manifest

Run these checks from the repository root during the Phase 12 cutover. Empty
output is success for the first four commands.

```sh
# Concepts must not require the concrete dynamic metadata types.
rg -n 'same_as<.*(DynamicExtents|TensorLayout)' include/fat_p/tensor

# TensorAccess must not regain a permissive no-op validation fallback.
rg -n 'validate\([^)]*\).*\{\s*\}' include/fat_p/tensor/TensorView.h

# Shared kernels must accept the destination rank instead of naming only the
# two-argument dynamic Tensor specialization.
rg -n 'Tensor<[^,>]+,\s*[^,>]+>&\s+(result|destination)' include/fat_p/tensor

# No second ranked-only traversal loop or hidden dynamic materialization.
rg -n 'Ranked.*(forEach|while|offset)|Tensor<.*>\s+(temporary|intermediate|materialized)' include/fat_p/tensor

# The superseded dynamically boxed walker must not coexist with the unified
# rank-aware implementation.
rg -n '^class TensorIterationPlan$' include/fat_p/tensor/TensorIterationPlan.h
```

The public facade and aggregate registrations must all be present:

```sh
rg -n 'TensorRanked.h' \
  include/fat_p/TensorRanked.h \
  include/fat_p/tensor/TensorRanked.h \
  components/FatPTest/tests/IncludeAllFatPHeaders.h \
  include/fat_p/FatPTest.h

rg -n 'test_TensorRanked' \
  components/FatPTest/tests/test_FatP.h \
  components/FatPTest/tests/test_FatP.cpp
```

The inventory is deliberately syntactic. The ranked behavioral suite and peer
review remain responsible for detecting equivalent defects that use different
spellings.
