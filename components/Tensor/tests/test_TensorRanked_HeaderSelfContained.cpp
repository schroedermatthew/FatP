/**
 * @file test_TensorRanked_HeaderSelfContained.cpp
 * @brief Compile-only self-containment test for TensorRanked.h.
 */

/*
FATP_META:
  meta_version: 1
  component: TensorRanked
  file_role: test
  path: components/Tensor/tests/test_TensorRanked_HeaderSelfContained.cpp
  namespace: fat_p::testing
  layer: Testing
  summary: "Compile-only self-containment check for TensorRanked.h."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/TensorRanked.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include "TensorRanked.h"
#include "TensorRanked.h"

template <typename Source>
concept TemporaryRankedDescriptor = requires(Source&& source) {
    fat_p::describeTensor(static_cast<Source&&>(source));
};

template <typename Descriptor>
concept TemporaryRankedDescriptorBorrow = requires(Descriptor&& descriptor) {
    static_cast<Descriptor&&>(descriptor).borrow();
};

static_assert(!TemporaryRankedDescriptor<fat_p::RankedTensor<int, 2>>);
static_assert(!TemporaryRankedDescriptor<fat_p::RankedTensorView<int, 2>>);
static_assert(!TemporaryRankedDescriptor<fat_p::SharedRankedTensorView<int, 2>>);
static_assert(!TemporaryRankedDescriptorBorrow<fat_p::RankedStridedTensorDescriptor<int, 2>>);

int main()
{
    return 0;
}
