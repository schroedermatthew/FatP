#pragma once

/*
FATP_META:
  meta_version: 1
  component: Tensor
  file_role: internal_header
  path: include/fat_p/tensor/TensorIndex.h
  namespace: fat_p::tensor_detail
  layer: Domain
  summary: "Range-preserving conversion of integral Tensor element indices."
  api_stability: in_work
  related:
    headers:
      - include/fat_p/tensor/TensorExtents.h
      - include/fat_p/tensor/TensorStatic.h
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: codex
    mode: manual
*/

#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace fat_p::tensor_detail
{
template <std::integral Target, std::integral Index>
[[nodiscard]] constexpr Target checkedElementIndexCast(Index index)
{
    if constexpr (std::is_signed_v<Index>)
    {
        if (index < 0)
        {
            throw std::out_of_range("Tensor element index is negative");
        }
    }
    if constexpr (std::numeric_limits<Index>::digits > std::numeric_limits<Target>::digits)
    {
        if (index > static_cast<Index>(std::numeric_limits<Target>::max()))
        {
            throw std::out_of_range("Tensor element index is not representable");
        }
    }
    return static_cast<Target>(index);
}
} // namespace fat_p::tensor_detail
