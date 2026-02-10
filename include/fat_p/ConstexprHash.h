#pragma once

/*
FATP_META:
  meta_version: 1
  component: ConstexprHash
  file_role: public_header
  path: include/fat_p/ConstexprHash.h
  namespace: fat_p
  layer: Foundation
  summary: Compile-time FNV-1a hashing and hash-combine utilities.
  api_stability: candidate
  related:
    docs_search: "ConstexprHash"
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
 * @file ConstexprHash.h
 * @brief Compile-time FNV-1a hashing and hash-combine utilities.
 *
 * Provides consteval string hashing (32-bit and 64-bit FNV-1a), a Boost-style
 * hash_combine function, and a variadic hash_values helper for combining
 * multiple string hashes.
 *
 * @note Not suitable for cryptographic purposes.
 *
 * Previously part of ConstexprUtilities.h. Extracted for cohesion — hashing
 * is independent of the arithmetic and string conversion utilities that share
 * that header.
 */

#include <cstdint>
#include <string_view>

namespace fat_p
{

// =============================================================================
// Hashing Utilities
// =============================================================================

/**
 * @brief Compile-time FNV-1a hash function for strings (32-bit).
 *
 * FNV-1a is a non-cryptographic hash with good avalanche properties.
 * Useful for compile-time string switches and hash-based dispatch tables.
 *
 * @param s The string view to hash.
 * @return uint32_t The 32-bit hash value.
 *
 * @note Not suitable for cryptographic purposes.
 * @note Empty string returns the FNV offset basis (2166136261).
 */
[[nodiscard]] consteval uint32_t constexpr_hash(std::string_view s) noexcept
{
    constexpr uint32_t FNV_PRIME = 16777619U;
    constexpr uint32_t FNV_OFFSET_BASIS = 2166136261U;

    uint32_t hash = FNV_OFFSET_BASIS;
    for (char c : s)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Compile-time FNV-1a hash function for strings (64-bit).
 *
 * 64-bit variant provides better collision resistance for large hash tables.
 *
 * @param s The string view to hash.
 * @return uint64_t The 64-bit hash value.
 *
 * @note Empty string returns the FNV offset basis (14695981039346656037).
 */
[[nodiscard]] consteval uint64_t constexpr_hash64(std::string_view s) noexcept
{
    constexpr uint64_t FNV_PRIME = 1099511628211ULL;
    constexpr uint64_t FNV_OFFSET_BASIS = 14695981039346656037ULL;

    uint64_t hash = FNV_OFFSET_BASIS;
    for (char c : s)
    {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return hash;
}

/**
 * @brief Combine two hash values into one (for hashing composite types).
 *
 * Uses the Boost-style hash combine algorithm with the golden ratio constant.
 *
 * @param seed The existing hash seed.
 * @param value The new hash value to combine.
 * @return uint64_t The combined hash.
 *
 * @example
 *   uint64_t h = hash_combine(constexpr_hash64("key"), constexpr_hash64("value"));
 */
[[nodiscard]] constexpr uint64_t hash_combine(uint64_t seed, uint64_t value) noexcept
{
    return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

/**
 * @brief Hash multiple string values into a single hash.
 *
 * @param args String views to hash and combine.
 * @return uint64_t The combined hash of all inputs.
 *
 * @example
 *   constexpr auto h = hash_values("namespace", "class", "method");
 */
template <typename... Args>
[[nodiscard]] consteval uint64_t hash_values(const Args&... args) noexcept
{
    uint64_t seed = 0;
    ((seed = hash_combine(seed, constexpr_hash64(args))), ...);
    return seed;
}

} // namespace fat_p
