/** @file test_TensorAlgorithms_HeaderSelfContained.cpp @brief TensorAlgorithms header self-containment test. */

/*
FATP_META:
  meta_version: 1
  component: TensorAlgorithms
  file_role: test
  path: components/Tensor/tests/test_TensorAlgorithms_HeaderSelfContained.cpp
  namespace: fat_p::testing
  layer: Testing
  summary: "Compile-only self-containment check for TensorAlgorithms.h."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/TensorAlgorithms.h
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

#include "TensorAlgorithms.h"
#include "TensorAlgorithms.h"

int main()
{
    fat_p::Tensor<int> source({2, 2}, 3);
    auto packed = fat_p::clone(source);
    auto reshaped = fat_p::reshapeCopy(packed, fat_p::DynamicExtents{4});
    auto destination = source.reshapeView(fat_p::DynamicExtents{4});
    fat_p::copyFrom(destination, reshaped);
    const fat_p::Tensor<double> offsets({2, 2}, 0.5);
    auto added = source + offsets;
    auto subtracted = fat_p::subtract(source, offsets);
    auto multiplied = fat_p::multiply(source, offsets);
    static_assert(fat_p::TensorArithmeticCompatible<int, double>);
    static_assert(std::same_as<fat_p::TensorArithmeticType<int, double>, double>);
    const auto converted = fat_p::cast<double>(source);
    const auto scaled = converted * 2.0;
    const auto reverse = fat_p::subtract(10.0, source);
    const auto quotient = source / offsets;
    const auto scalarQuotient = fat_p::divide(source, 2);
    const auto reverseQuotient = 12.0 / source;
    source += 1;
    destination -= 1;
    auto shared = source.asSharedView();
    shared *= 2.0;
    fat_p::divideAssign(source, 2, std::allocator<int>{});
    fat_p::addAssign(source, packed);
    fat_p::subtractAssign(source, packed.asConstView());
    fat_p::multiplyAssign(shared, 2);
    source /= 2;
    const auto negative = fat_p::negate(source);
    const auto minus = -source.asConstView();
    const auto magnitude = fat_p::abs(negative);
    const auto allocatedNegative = fat_p::negate(source, std::allocator<int>{});
    const auto allocatedMagnitude = fat_p::abs(minus.asSharedView(), std::allocator<int>{});
    const fat_p::Tensor<double> floating({}, 1.0);
    const auto root = fat_p::sqrt(floating);
    const auto exponential = fat_p::exp(floating.asConstView());
    const auto logarithm = fat_p::log(floating.asSharedView());
    const auto allocatedRoot = fat_p::sqrt(floating.asConstView(), std::allocator<double>{});
    const auto allocatedExp = fat_p::exp(floating.asSharedView(), std::allocator<double>{});
    const auto allocatedLog = fat_p::log(floating, std::allocator<double>{});
    return source[0] == 3 && added[0] == 3.5 && subtracted[0] == 2.5 && multiplied[0] == 1.5 &&
           converted[0] == 3.0 && scaled[0] == 6.0 && reverse[0] == 7.0 &&
           quotient[0] == 6.0 && scalarQuotient[0] == 1 && reverseQuotient[0] == 4.0 &&
           negative[0] == -3 && minus[0] == -3 && magnitude[0] == 3 &&
           allocatedNegative[0] == -3 && allocatedMagnitude[0] == 3 && root[0] == 1.0 &&
           exponential[0] > 2.0 && exponential[0] < 3.0 && logarithm[0] == 0.0 &&
           allocatedRoot[0] == 1.0 && allocatedExp[0] == exponential[0] && allocatedLog[0] == 0.0 ? 0 : 1;
}
