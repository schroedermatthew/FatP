/**
 * @file benchmark_StrongId.cpp
 * @brief FAT-P StrongId benchmarks vs industry competitors.
 *
 * Architecture: Round-robin execution with randomized order per run.
 * This ensures all libraries observe the same distribution of machine states,
 * eliminating drift-induced unfairness.
 *
 * Design Invariants:
 *   1. Each measured run executes exactly one timed iteration per library.
 *   2. Library execution order is randomized per run.
 *   3. Setup and teardown occur outside timed regions.
 *   4. All libraries observe the same distribution of machine states.
 *   5. Medians are the primary reported statistic.
 *
 * Fat-P Libraries:
 *   - fat_p::StrongId (Checked): DefaultOpPolicy with overflow detection
 *   - fat_p::StrongId (Unchecked): UncheckedOpPolicy, zero overhead
 *
 * Competitor Libraries (conditioned on availability):
 *   TIER 1 - Direct competitors (strong typedef libraries):
 *     - fluent::NamedType - Popular header-only strong typedef
 *     - ts::strong_typedef - foonathan/type_safe library
 *     - strong::type - rollbear/strong_type
 *   TIER 2 - Standard library alternatives:
 *     - enum class - Built-in C++ strong enum
 *     - Manual wrapper struct - Hand-written ID class
 *   TIER 3 - Baseline:
 *     - Raw int - Performance baseline
 *
 * Build (minimal):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_StrongId.cpp -o bench_strongid
 *
 * Build (MSVC):
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_StrongId.cpp /Fe:bench_strongid.exe
 *
 * Build (with competitors):
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native \
 *       -I/path/to/NamedType/include \
 *       -I/path/to/type_safe/include \
 *       -I/path/to/strong_type/include \
 *       benchmark_StrongId.cpp -o bench_strongid
 *
 * Environment Variables (all optional):
 *   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
 *   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
 *   FATP_BENCH_SEED          - RNG seed (default: 12345)
 *   FATP_BENCH_TARGET_WORK   - Operations per library iteration (default: 1000000)
 *   FATP_BENCH_MIN_BATCH_MS  - Min batch duration (default: 50)
 *   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
 *   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
 *   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
 *   FATP_BENCH_NO_SCOPE      - Disable Windows priority/affinity changes
 *   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
 *   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
 *
 * Run:
 *   ./bench_strongid
 *   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_strongid
 */

/*
FATP_META:
  meta_version: 1
  component: StrongId
  file_role: benchmark
  path: components/StrongId/benchmarks/benchmark_StrongId.cpp
  layer: Testing
  namespace: fat_p
  summary: Comprehensive benchmarks for StrongId vs industry competitors.
  api_stability: candidate
  related:
    docs:
      - Documentation/IN WORK/Overview - StrongId.md
      - Documentation/IN WORK/User Manual - StrongId.md
      - Documentation/IN WORK/Companion Guide - StrongId.md
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
      - include/fat_p/StrongId.h
    tests:
      - components/StrongId/tests/test_StrongId.cpp
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 5
    defines_unprefixed: 5
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: Claude
    mode: manual
*/

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

// Vendored strong_type before FatPBenchmarkRunner (pulls windows.h). MSVC cannot
// parse several strong_type noexcept/friend templates once Win32 macros are live.
#if __has_include(<strong_type/strong_type.hpp>)
#include <strong_type/strong_type.hpp>
#endif

#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"

// ============================================================================
// Library Detection
// ============================================================================

// Fat-P StrongId (should always be available)
#if __has_include("StrongId.h")
#include "StrongId.h"
#define HAS_FATP_STRONGID 1
#else
#define HAS_FATP_STRONGID 0
#endif

// fluent::NamedType - Popular header-only strong typedef
// Install: git clone https://github.com/joboccara/NamedType
// Or: vcpkg install named-type
#if __has_include(<NamedType/named_type.hpp>)
#include <NamedType/named_type.hpp>
#define HAS_NAMED_TYPE 1
#else
#define HAS_NAMED_TYPE 0
#endif

// foonathan::type_safe - Comprehensive type safety library
// Install: vcpkg install type-safe
// Or: git clone https://github.com/foonathan/type_safe
#if __has_include(<type_safe/strong_typedef.hpp>)
#include <type_safe/strong_typedef.hpp>
#define HAS_TYPE_SAFE 1
#else
#define HAS_TYPE_SAFE 0
#endif

// rollbear::strong_type - Modern C++ strong typedef
// Install: git clone https://github.com/rollbear/strong_type
#if __has_include(<strong_type/strong_type.hpp>)
#include <strong_type/strong_type.hpp>
#define HAS_ROLLBEAR_STRONG_TYPE 1
#else
#define HAS_ROLLBEAR_STRONG_TYPE 0
#endif

// Boost.StrongTypedef - Classic Boost approach
// Install: vcpkg install boost-serialization (includes strong_typedef)
#if __has_include(<boost/serialization/strong_typedef.hpp>)
#include <boost/serialization/strong_typedef.hpp>
#define HAS_BOOST_STRONG_TYPEDEF 1
#else
#define HAS_BOOST_STRONG_TYPEDEF 0
#endif

// ============================================================================
// Global Configuration
// ============================================================================

static fat_p::bench::BenchConfig g_config;

static size_t TARGET_WORK()
{
    return fat_p::bench::detail::getEnvSizeT("FATP_BENCH_TARGET_WORK", 1'000'000);
}

static size_t WARMUP_RUNS()
{
    return g_config.warmupRuns;
}
static size_t MEASURED_RUNS()
{
    return g_config.measuredRuns;
}

// ============================================================================
// Type Definitions - Fat-P
// ============================================================================

#if HAS_FATP_STRONGID
struct FatPCheckedTag
{
};
struct FatPUncheckedTag
{
};

using FatPCheckedId = fat_p::StrongId<int, FatPCheckedTag>;
using FatPUncheckedId =
    fat_p::StrongId<int, FatPUncheckedTag, fat_p::strong_id::NoCheckPolicy, fat_p::strong_id::UncheckedOpPolicy>;
#endif

// ============================================================================
// Type Definitions - fluent::NamedType
// ============================================================================

#if HAS_NAMED_TYPE
using NamedTypeId =
    fluent::NamedType<int, struct NamedTypeIdTag, fluent::Comparable, fluent::Hashable, fluent::Addable>;
#endif

// ============================================================================
// Type Definitions - type_safe
// ============================================================================

#if HAS_TYPE_SAFE
struct TypeSafeIdTag : type_safe::strong_typedef<TypeSafeIdTag, int>,
                       type_safe::strong_typedef_op::equality_comparison<TypeSafeIdTag>,
                       type_safe::strong_typedef_op::relational_comparison<TypeSafeIdTag>,
                       type_safe::strong_typedef_op::integer_arithmetic<TypeSafeIdTag>
{
    using strong_typedef::strong_typedef;
};
using TypeSafeId = TypeSafeIdTag;
#endif

// ============================================================================
// Type Definitions - rollbear::strong_type
// ============================================================================

#if HAS_ROLLBEAR_STRONG_TYPE
using RollbearId = strong::type<int, struct RollbearIdTag_, strong::ordered, strong::equality, strong::hashable>;
#endif

// ============================================================================
// Type Definitions - Boost.StrongTypedef
// ============================================================================

#if HAS_BOOST_STRONG_TYPEDEF
BOOST_STRONG_TYPEDEF(int, BoostStrongId)
#endif

// ============================================================================
// Type Definitions - enum class (built-in)
// ============================================================================

enum class EnumClassId : int
{
};

// Helper to construct enum class from int
inline EnumClassId make_enum_id(int v)
{
    return static_cast<EnumClassId>(v);
}
inline int get_enum_id(EnumClassId id)
{
    return static_cast<int>(id);
}

// ============================================================================
// Type Definitions - Manual wrapper (baseline)
// ============================================================================

struct ManualId
{
    int value;

    ManualId() : value(0) {}
    explicit ManualId(int v) : value(v) {}

    int get() const { return value; }

    bool operator==(ManualId o) const { return value == o.value; }
    bool operator!=(ManualId o) const { return value != o.value; }
    bool operator<(ManualId o) const { return value < o.value; }
    bool operator<=(ManualId o) const { return value <= o.value; }
    bool operator>(ManualId o) const { return value > o.value; }
    bool operator>=(ManualId o) const { return value >= o.value; }

    ManualId operator+(ManualId o) const { return ManualId(value + o.value); }
    ManualId operator+(int v) const { return ManualId(value + v); }
    ManualId& operator+=(int v)
    {
        value += v;
        return *this;
    }
    ManualId& operator++()
    {
        ++value;
        return *this;
    }
    ManualId operator++(int)
    {
        ManualId tmp = *this;
        ++value;
        return tmp;
    }
};

namespace std
{
template <>
struct hash<ManualId>
{
    std::size_t operator()(ManualId id) const noexcept { return std::hash<int>{}(id.value); }
};

template <>
struct hash<EnumClassId>
{
    std::size_t operator()(EnumClassId id) const noexcept { return std::hash<int>{}(static_cast<int>(id)); }
};

#if HAS_BOOST_STRONG_TYPEDEF
template <>
struct hash<BoostStrongId>
{
    std::size_t operator()(BoostStrongId id) const noexcept { return std::hash<int>{}(id.t); }
};
#endif
} // namespace std

// ============================================================================
// Adapter Interface
// ============================================================================

struct IStrongIdAdapter
{
    virtual ~IStrongIdAdapter() = default;
    virtual const char* name() const = 0;

    // Benchmark operations - return accumulated value to prevent optimization
    virtual int64_t bench_construction(const std::vector<int>& values) = 0;
    virtual int64_t bench_comparison(const std::vector<int>& values) = 0;
    virtual int64_t bench_addition(const std::vector<int>& values) = 0;
    virtual int64_t bench_increment(size_t n) = 0;
    virtual int64_t bench_hash(const std::vector<int>& values) = 0;
};

// ============================================================================
// Fat-P Checked Adapter
// ============================================================================

#if HAS_FATP_STRONGID
class FatPCheckedAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "fat_p::StrongId (Checked)"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            FatPCheckedId id(v);
            sum += id.get();
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            FatPCheckedId a(values[i - 1]);
            FatPCheckedId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        // Use modular arithmetic to avoid overflow while still testing += performance
        int64_t total = 0;
        FatPCheckedId sum(0);
        for (size_t i = 0; i < values.size(); ++i)
        {
            // Reset periodically to avoid overflow
            if (i % 1000 == 0)
            {
                total += sum.get();
                sum = FatPCheckedId(0);
            }
            sum += (values[i] % 100); // Small values to avoid overflow
        }
        return total + sum.get();
    }

    int64_t bench_increment(size_t n) override
    {
        FatPCheckedId id(0);
        for (size_t i = 0; i < n; ++i)
        {
            ++id;
        }
        return id.get();
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<FatPCheckedId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            FatPCheckedId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};

// ============================================================================
// Fat-P Unchecked Adapter
// ============================================================================

class FatPUncheckedAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "fat_p::StrongId (Unchecked)"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            FatPUncheckedId id(v);
            sum += id.get();
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            FatPUncheckedId a(values[i - 1]);
            FatPUncheckedId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        FatPUncheckedId sum(0);
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum.get();
                sum = FatPUncheckedId(0);
            }
            sum += (values[i] % 100);
        }
        return total + sum.get();
    }

    int64_t bench_increment(size_t n) override
    {
        FatPUncheckedId id(0);
        for (size_t i = 0; i < n; ++i)
        {
            ++id;
        }
        return id.get();
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<FatPUncheckedId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            FatPUncheckedId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};
#endif

// ============================================================================
// fluent::NamedType Adapter
// ============================================================================

#if HAS_NAMED_TYPE
class NamedTypeAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "fluent::NamedType"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            NamedTypeId id(v);
            sum += id.get();
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            NamedTypeId a(values[i - 1]);
            NamedTypeId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        NamedTypeId sum(0);
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum.get();
                sum = NamedTypeId(0);
            }
            sum = sum + NamedTypeId(values[i] % 100);
        }
        return total + sum.get();
    }

    int64_t bench_increment(size_t n) override
    {
        NamedTypeId id(0);
        for (size_t i = 0; i < n; ++i)
        {
            id = id + NamedTypeId(1);
        }
        return id.get();
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<NamedTypeId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            NamedTypeId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};
#endif

// ============================================================================
// type_safe Adapter
// ============================================================================

#if HAS_TYPE_SAFE
class TypeSafeAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "ts::strong_typedef"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            TypeSafeId id(v);
            sum += static_cast<int>(id);
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            TypeSafeId a(values[i - 1]);
            TypeSafeId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        TypeSafeId sum(0);
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += static_cast<int>(sum);
                sum = TypeSafeId(0);
            }
            sum += TypeSafeId(values[i] % 100);
        }
        return total + static_cast<int>(sum);
    }

    int64_t bench_increment(size_t n) override
    {
        TypeSafeId id(0);
        for (size_t i = 0; i < n; ++i)
        {
            ++id;
        }
        return static_cast<int>(id);
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<int> hasher; // type_safe uses underlying type hash
        int64_t sum = 0;
        for (int v : values)
        {
            TypeSafeId id(v);
            sum += static_cast<int64_t>(hasher(static_cast<int>(id)));
        }
        return sum;
    }
};
#endif

// ============================================================================
// rollbear::strong_type Adapter
// ============================================================================

#if HAS_ROLLBEAR_STRONG_TYPE
class RollbearAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "strong::type (rollbear)"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            RollbearId id(v);
            sum += value_of(id);
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            RollbearId a(values[i - 1]);
            RollbearId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        // rollbear doesn't have += so we construct and extract (same work as others)
        int64_t total = 0;
        int sum = 0;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum;
                sum = 0;
            }
            RollbearId id(values[i] % 100);
            sum += value_of(id);
        }
        return total + sum;
    }

    int64_t bench_increment(size_t n) override
    {
        // rollbear doesn't have ++ so we construct each iteration
        int val = 0;
        for (size_t i = 0; i < n; ++i)
        {
            RollbearId id(val);
            val = value_of(id) + 1;
        }
        return val;
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<RollbearId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            RollbearId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};
#endif

// ============================================================================
// Boost.StrongTypedef Adapter
// ============================================================================

#if HAS_BOOST_STRONG_TYPEDEF
class BoostAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "boost::strong_typedef"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            BoostStrongId id(v);
            sum += id.t;
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            BoostStrongId a(values[i - 1]);
            BoostStrongId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        // boost::strong_typedef doesn't have += so we construct and extract
        int64_t total = 0;
        int sum = 0;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum;
                sum = 0;
            }
            BoostStrongId id(values[i] % 100);
            sum += id.t;
        }
        return total + sum;
    }

    int64_t bench_increment(size_t n) override
    {
        // boost::strong_typedef doesn't have ++ so we construct each iteration
        int val = 0;
        for (size_t i = 0; i < n; ++i)
        {
            BoostStrongId id(val);
            val = id.t + 1;
        }
        return val;
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<BoostStrongId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            BoostStrongId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};
#endif

// ============================================================================
// enum class Adapter
// ============================================================================

class EnumClassAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "enum class"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            EnumClassId id = make_enum_id(v);
            sum += get_enum_id(id);
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            EnumClassId a = make_enum_id(values[i - 1]);
            EnumClassId b = make_enum_id(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        int sum = 0;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum;
                sum = 0;
            }
            sum += (values[i] % 100); // enum class has no arithmetic
        }
        return total + sum;
    }

    int64_t bench_increment(size_t n) override
    {
        int id = 0;
        for (size_t i = 0; i < n; ++i)
        {
            ++id; // enum class has no ++
        }
        return id;
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<EnumClassId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            EnumClassId id = make_enum_id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};

// ============================================================================
// Manual Wrapper Adapter
// ============================================================================

class ManualAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "Manual wrapper struct"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            ManualId id(v);
            sum += id.get();
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            ManualId a(values[i - 1]);
            ManualId b(values[i]);
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        ManualId sum(0);
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum.get();
                sum = ManualId(0);
            }
            sum += (values[i] % 100);
        }
        return total + sum.get();
    }

    int64_t bench_increment(size_t n) override
    {
        ManualId id(0);
        for (size_t i = 0; i < n; ++i)
        {
            ++id;
        }
        return id.get();
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<ManualId> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            ManualId id(v);
            sum += static_cast<int64_t>(hasher(id));
        }
        return sum;
    }
};

// ============================================================================
// Raw int Adapter (Baseline)
// ============================================================================

class RawIntAdapter final : public IStrongIdAdapter
{
public:
    const char* name() const override { return "Raw int (baseline)"; }

    int64_t bench_construction(const std::vector<int>& values) override
    {
        int64_t sum = 0;
        for (int v : values)
        {
            int id = v;
            sum += id;
        }
        return sum;
    }

    int64_t bench_comparison(const std::vector<int>& values) override
    {
        int64_t count = 0;
        for (size_t i = 1; i < values.size(); ++i)
        {
            int a = values[i - 1];
            int b = values[i];
            if (a < b)
                ++count;
        }
        return count;
    }

    int64_t bench_addition(const std::vector<int>& values) override
    {
        int64_t total = 0;
        int sum = 0;
        for (size_t i = 0; i < values.size(); ++i)
        {
            if (i % 1000 == 0)
            {
                total += sum;
                sum = 0;
            }
            sum += (values[i] % 100);
        }
        return total + sum;
    }

    int64_t bench_increment(size_t n) override
    {
        int id = 0;
        for (size_t i = 0; i < n; ++i)
        {
            ++id;
        }
        return id;
    }

    int64_t bench_hash(const std::vector<int>& values) override
    {
        std::hash<int> hasher;
        int64_t sum = 0;
        for (int v : values)
        {
            sum += static_cast<int64_t>(hasher(v));
        }
        return sum;
    }
};

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

using fat_p::bench::DoNotOptimize;
using fat_p::bench::preventOpt;
using fat_p::bench::Statistics;
using fat_p::bench::Timer;

struct BenchResult
{
    std::string name;
    Statistics stats;
};

void printResults(const std::vector<BenchResult>& results, const char* benchName)
{
    std::cout << "\n" << std::string(72, '-') << "\n";
    std::cout << "  " << benchName << "\n";
    std::cout << std::string(72, '-') << "\n\n";

    // Find baseline (raw int)
    double baseline = 0;
    for (const auto& r : results)
    {
        if (r.name == "Raw int (baseline)")
        {
            baseline = r.stats.median;
            break;
        }
    }

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results)
    {
        double ratio = (baseline > 0) ? (r.stats.median / baseline) : 0;
        std::cout << "  " << std::setw(30) << std::left << r.name << std::setw(10) << r.stats.median << " ns"
                  << "  (stddev: " << std::setw(6) << r.stats.stddev << ")"
                  << "  " << std::setw(5) << ratio << "x\n";
    }
}

struct CaseSamples
{
    std::string name;
    std::vector<double> samples;
};

template <typename Func>
std::vector<BenchResult> runComparisonCase(const char* benchName,
                                           std::vector<std::unique_ptr<IStrongIdAdapter>>& adapters,
                                           std::mt19937_64& rng,
                                           std::size_t opsPerIter,
                                           Func&& runOnce)
{
    std::vector<std::vector<double>> all_samples(adapters.size());
    for (auto& v : all_samples)
    {
        v.reserve(MEASURED_RUNS());
    }

    std::vector<std::size_t> order(adapters.size());
    std::iota(order.begin(), order.end(), 0);

    // Warmup (unreported)
    for (std::size_t run = 0; run < WARMUP_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);
        for (std::size_t idx : order)
        {
            int64_t result = runOnce(*adapters[idx], benchName);
            preventOpt(result);
        }
    }

    // Measured
    for (std::size_t run = 0; run < MEASURED_RUNS(); ++run)
    {
        std::shuffle(order.begin(), order.end(), rng);

        for (std::size_t idx : order)
        {
            Timer t;
            t.start();
            int64_t result = runOnce(*adapters[idx], benchName);
            double elapsed = t.elapsedNs();
            preventOpt(result);

            all_samples[idx].push_back(elapsed / static_cast<double>(opsPerIter));
        }
    }

    std::vector<BenchResult> results;
    results.reserve(adapters.size());
    for (std::size_t i = 0; i < adapters.size(); ++i)
    {
        BenchResult r;
        r.name = adapters[i]->name();
        r.stats = Statistics::compute(std::move(all_samples[i]));
        results.push_back(std::move(r));
    }

    return results;
}

// ============================================================================
// Benchmark Suite
// ============================================================================

void runBenchmarkSuite(std::vector<std::unique_ptr<IStrongIdAdapter>>& adapters, const std::vector<int>& values)
{
    const size_t N = values.size();

    std::mt19937_64 rng(g_config.seed ^ 0x9e3779b97f4a7c15ULL);

    // Construction benchmark
    {
        auto results = runComparisonCase(
            "construction",
            adapters,
            rng,
            N,
            [&](IStrongIdAdapter& adapter, const char*) { return adapter.bench_construction(values); });
        printResults(results, "CONSTRUCTION (from int)");
    }

    // Comparison benchmark
    {
        auto results = runComparisonCase(
            "comparison",
            adapters,
            rng,
            (N > 0 ? (N - 1) : 0),
            [&](IStrongIdAdapter& adapter, const char*) { return adapter.bench_comparison(values); });
        printResults(results, "COMPARISON (operator<)");
    }

    // Addition benchmark
    {
        auto results = runComparisonCase(
            "addition",
            adapters,
            rng,
            N,
            [&](IStrongIdAdapter& adapter, const char*) { return adapter.bench_addition(values); });
        printResults(results, "ADDITION (compound +=)");
    }

    // Increment benchmark
    {
        auto results = runComparisonCase(
            "increment",
            adapters,
            rng,
            N,
            [&](IStrongIdAdapter& adapter, const char*) { return adapter.bench_increment(N); });
        printResults(results, "INCREMENT (prefix ++)");
    }

    // Hash benchmark
    {
        auto results = runComparisonCase(
            "hash",
            adapters,
            rng,
            N,
            [&](IStrongIdAdapter& adapter, const char*) { return adapter.bench_hash(values); });
        printResults(results, "HASH (std::hash)");
    }
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    using namespace fat_p::bench;

    // Load configuration from environment
    g_config = BenchConfig::fromEnv();

    // Apply benchmark scope (Windows priority/affinity)
    BenchmarkScope scope(!g_config.noScope);

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "StrongId";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = g_config.seed;
    
    // Competitors
#if HAS_FATP_STRONGID
    hdr.competitors.push_back({"fat_p::StrongId<Checked>", true, "primary"});
    hdr.competitors.push_back({"fat_p::StrongId<Unchecked>", true, "primary"});
#else
    hdr.competitors.push_back({"fat_p::StrongId", false, "include StrongId.h"});
#endif
#if HAS_NAMED_TYPE
    hdr.competitors.push_back({"fluent::NamedType", true, ""});
#else
    hdr.competitors.push_back({"fluent::NamedType", false, "github.com/joboccara/NamedType"});
#endif
#if HAS_TYPE_SAFE
    hdr.competitors.push_back({"ts::strong_typedef", true, "type_safe"});
#else
    hdr.competitors.push_back({"ts::strong_typedef", false, "vcpkg install type-safe"});
#endif
#if HAS_ROLLBEAR_STRONG_TYPE
    hdr.competitors.push_back({"strong::type", true, "rollbear"});
#else
    hdr.competitors.push_back({"strong::type", false, "github.com/rollbear/strong_type"});
#endif
#if HAS_BOOST_STRONG_TYPEDEF
    hdr.competitors.push_back({"boost::strong_typedef", true, ""});
#else
    hdr.competitors.push_back({"boost::strong_typedef", false, "vcpkg install boost-serialization"});
#endif
    hdr.competitors.push_back({"enum class", true, "built-in"});
    hdr.competitors.push_back({"Manual wrapper struct", true, "baseline"});
    hdr.competitors.push_back({"Raw int", true, "baseline"});
    
    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = false;
    hdr.has_stabilization = !g_config.noStabilize;
    
    fat_p::bench::print_standard_header(hdr);

    // Generate test data
    const size_t N = TARGET_WORK();
    std::cout << "Operations per run: " << N << "\n\n";
    
    std::vector<int> values;
    values.reserve(N);
    std::mt19937_64 rng(g_config.seed);
    std::uniform_int_distribution<int> dist(1, 10000);
    for (size_t i = 0; i < N; ++i)
    {
        values.push_back(dist(rng));
    }

    // Build adapters
    std::vector<std::unique_ptr<IStrongIdAdapter>> adapters;
#if HAS_FATP_STRONGID
    adapters.push_back(std::make_unique<FatPCheckedAdapter>());
    adapters.push_back(std::make_unique<FatPUncheckedAdapter>());
#endif
#if HAS_NAMED_TYPE
    adapters.push_back(std::make_unique<NamedTypeAdapter>());
#endif
#if HAS_TYPE_SAFE
    adapters.push_back(std::make_unique<TypeSafeAdapter>());
#endif
#if HAS_ROLLBEAR_STRONG_TYPE
    adapters.push_back(std::make_unique<RollbearAdapter>());
#endif
#if HAS_BOOST_STRONG_TYPEDEF
    adapters.push_back(std::make_unique<BoostAdapter>());
#endif
    adapters.push_back(std::make_unique<EnumClassAdapter>());
    adapters.push_back(std::make_unique<ManualAdapter>());
    adapters.push_back(std::make_unique<RawIntAdapter>());

    // Run benchmarks
    runBenchmarkSuite(adapters, values);

    // Summary
    std::cout << "\n" << std::string(72, '=') << "\n";
    std::cout << "  INTERPRETATION GUIDE\n";
    std::cout << std::string(72, '=') << "\n\n";

    std::cout << "  1.00x = Zero overhead (identical to raw int)\n";
    std::cout << "  <1.5x = Negligible overhead for most applications\n";
    std::cout << "  >2.0x = Measurable overhead, consider for hot paths\n\n";

    std::cout << "  Key findings:\n";
    std::cout << "  - fat_p::StrongId (Unchecked) should match raw int exactly\n";
    std::cout << "  - fat_p::StrongId (Checked) has overflow detection cost\n";
    std::cout << "  - enum class lacks arithmetic/increment operators\n";
    std::cout << "  - Manual wrapper validates zero-overhead design pattern\n\n";

    std::cout << std::string(72, '=') << "\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << std::string(72, '=') << "\n";

    return 0;
}
