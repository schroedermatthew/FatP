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
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file ConstexprHash.h
 * @brief Compile-time FNV-1a hashing and hash-combine utilities.
 *
 * Provides constexpr string hashing (32-bit and 64-bit FNV-1a), a Boost-style
 * hash_combine function, and a variadic hash_values helper for combining
 * multiple string hashes.
 *
 * Every function is constexpr rather than consteval: one definition hashes
 * string literals during constant evaluation (case labels, static_assert,
 * template arguments) and runtime strings in ordinary code. A string switch
 * therefore uses the same function on both sides, so the label hashes and the
 * input hash cannot disagree.
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
 * @brief FNV-1a hash function for strings (32-bit), usable at compile time and runtime.
 *
 * FNV-1a is a non-cryptographic hash with good avalanche properties.
 * Useful for string switches and hash-based dispatch tables: hash the case
 * labels and the runtime input with this same function.
 *
 * @param s The string view to hash.
 * @return uint32_t The 32-bit hash value.
 *
 * @note Not suitable for cryptographic purposes.
 * @note Empty string returns the FNV offset basis (2166136261).
 * @note Each byte is hashed as unsigned char, so the result does not depend on
 *       the signedness of char.
 * @note Distinct strings can share a hash. A dispatch that must reject unknown
 *       input compares the matched string after the hash selects a case.
 */
[[nodiscard]] constexpr uint32_t constexpr_hash(std::string_view s) noexcept
{
    constexpr uint32_t kFnvPrime = 16777619U;
    constexpr uint32_t kFnvOffsetBasis = 2166136261U;

    uint32_t hash = kFnvOffsetBasis;
    for (char c : s)
    {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= kFnvPrime;
    }
    return hash;
}

/**
 * @brief FNV-1a hash function for strings (64-bit), usable at compile time and runtime.
 *
 * 64-bit variant provides better collision resistance for large hash tables.
 *
 * @param s The string view to hash.
 * @return uint64_t The 64-bit hash value.
 *
 * @note Empty string returns the FNV offset basis (14695981039346656037).
 */
[[nodiscard]] constexpr uint64_t constexpr_hash64(std::string_view s) noexcept
{
    constexpr uint64_t kFnvPrime = 1099511628211ULL;
    constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;

    uint64_t hash = kFnvOffsetBasis;
    for (char c : s)
    {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= kFnvPrime;
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
 * Folds constexpr_hash64 of each argument into a seed of 0 with hash_combine.
 * The result is order-sensitive, and a single argument does not reproduce
 * constexpr_hash64 of that argument.
 *
 * @param args Values convertible to std::string_view.
 * @return uint64_t The combined hash of all inputs.
 *
 * @example
 *   constexpr auto h = hash_values("namespace", "class", "method");
 */
template <typename... Args>
[[nodiscard]] constexpr uint64_t hash_values(const Args&... args) noexcept
{
    uint64_t seed = 0;
    ((seed = hash_combine(seed, constexpr_hash64(args))), ...);
    return seed;
}

} // namespace fat_p
