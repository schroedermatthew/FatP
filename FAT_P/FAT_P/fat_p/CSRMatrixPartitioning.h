#pragma once

/*
FATP_META:
  meta_version: 1
  component: CSRMatrixPartitioning
  file_role: public_header
  path: fat_p/CSRMatrixPartitioning.h
  namespace: fat_p
  summary: "Public header for CSRMatrixPartitioning."
  api_stability: in_work
  related:
    docs_search: "CSRMatrixPartitioning"
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
/**
 * @file CSRMatrixPartitioning.h
 * @brief Shared work partitioning helpers for CSRMatrix parallel/HPC algorithms.
 *
 * @details
 * Systemic Hygiene Policy Rule E: if two headers need the same helper, define it
 * in exactly one shared header and include it from both.
 */

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace fat_p
{
namespace detail
{

/**
 * @brief Compute balanced row partitions based on non-zero count.
 *
 * @details Divides rows into partitions where each partition has approximately
 * equal work (measured by nnz). This is key for good parallel scaling with
 * irregular row lengths.
 *
 * @tparam PtrType Pointer type from the matrix (typically std::size_t)
 * @param row_ptrs Row pointer array from CSR matrix (length num_rows + 1)
 * @param num_rows Number of rows
 * @param num_partitions Desired number of partitions
 * @return Vector of (start_row, end_row) pairs (half-open ranges)
 */
template <typename PtrType>
inline std::vector<std::pair<std::size_t, std::size_t>> compute_balanced_partitions(
    const PtrType* row_ptrs,
    std::size_t num_rows,
    std::size_t num_partitions)
{
    if (num_rows == 0 || num_partitions == 0)
    {
        return {};
    }

    num_partitions = std::min(num_partitions, num_rows);

    std::vector<std::pair<std::size_t, std::size_t>> partitions;
    partitions.reserve(num_partitions);

    const std::size_t total_nnz = static_cast<std::size_t>(row_ptrs[num_rows]);
    std::size_t target_nnz_per_partition = (total_nnz + num_partitions - 1) / num_partitions;

    if (target_nnz_per_partition == 0)
    {
        target_nnz_per_partition = 1;
    }

    std::size_t current_start = 0;
    std::size_t current_nnz = 0;

    for (std::size_t i = 0; i < num_rows; ++i)
    {
        const std::size_t row_nnz = static_cast<std::size_t>(row_ptrs[i + 1] - row_ptrs[i]);
        current_nnz += row_nnz;

        if (current_nnz >= target_nnz_per_partition || i == num_rows - 1)
        {
            partitions.emplace_back(current_start, i + 1);
            current_start = i + 1;
            current_nnz = 0;

            // If we already produced the requested number of partitions, merge the
            // remaining rows into the last partition.
            if (partitions.size() >= num_partitions && i < num_rows - 1)
            {
                partitions.back().second = num_rows;
                break;
            }
        }
    }

    return partitions;
}

} // namespace detail
} // namespace fat_p
