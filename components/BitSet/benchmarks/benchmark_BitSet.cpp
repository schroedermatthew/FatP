/*
FATP_META:
  meta_version: 1
  component: BitSet
  file_role: benchmark
  path: components/BitSet/benchmarks/benchmark_BitSet.cpp
  layer: Testing
  namespace: fat_p
  summary: "benchmark file for BitSet"
  api_stability: in_work
  related:
    docs_search: "BitSet"
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 11
    defines_unprefixed: 11
    undefs_total: 0
    includes_windows_h: true
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

// benchmark_BitSet.cpp - FAT-P BitSet comprehensive benchmark suite
// Competitors: std::bitset, boost::dynamic_bitset, llvm::BitVector, CRoaring, BitMagic

// Suppress conversion warnings from vendor libraries (LLVM, roaring, BitMagic, absl)
#ifdef _MSC_VER
#pragma warning(disable : 4244 4267) // conversion warnings in vendor headers
#endif

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <malloc.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include "BitSet.h"
#include "FatPBenchmarkHeader.h"
#include "FatPBenchmarkRunner.h"

#pragma warning(push, 0)
#if __has_include(<boost/dynamic_bitset.hpp>)
#include <boost/dynamic_bitset.hpp>
#define HAS_BOOST 1
#else
#define HAS_BOOST 0
#endif

#if __has_include(<llvm/ADT/BitVector.h>)
#include <llvm/ADT/BitVector.h>
#include <llvm/ADT/SmallBitVector.h>
#if defined(_WIN32) || defined(_WIN64)
#pragma comment(lib, "ws2_32.lib") // LLVMSupport.lib requires Windows Sockets
#endif
#define HAS_LLVM 1
#else
#define HAS_LLVM 0
#endif

#if __has_include(<roaring/roaring.hh>)
#include <roaring/roaring.hh>
#define HAS_CROARING 1
#elif __has_include("roaring.hh")
#include "roaring.hh"
#define HAS_CROARING 1
#else
#define HAS_CROARING 0
#endif

#if __has_include(<bitmagic/bm.h>)
#include <bitmagic/bm.h>
#define HAS_BITMAGIC 1
#elif __has_include("bm.h")
#include "bm.h"
#define HAS_BITMAGIC 1
#else
#define HAS_BITMAGIC 0
#endif

static fat_p::bench::BenchConfig g_config;
static size_t WARMUP_RUNS()
{
    return g_config.warmupRuns;
}
static size_t MEASURED_RUNS()
{
    return g_config.measuredRuns;
}

volatile size_t benchmark_sink = 0;

template <typename T>
inline void DoNotOptimize(T&& value)
{
#if defined(_MSC_VER)
    volatile auto sink = &value;
    (void)sink;
#else
    asm volatile("" : "+r"(value));
#endif
}

class Timer
{
    std::chrono::high_resolution_clock::time_point mStart;

public:
    void start()
    {
        mStart = std::chrono::high_resolution_clock::now();
    }
    double elapsed_ns() const
    {
        auto end = std::chrono::high_resolution_clock::now();
        return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - mStart).count());
    }
};

struct Stats
{
    double median = 0, mean = 0, stddev = 0;
};

Stats compute_stats(std::vector<double>& samples)
{
    Stats s;
    if (samples.empty())
    {
        return s;
    }
    std::sort(samples.begin(), samples.end());
    size_t n = samples.size();
    s.median = (n % 2 == 0) ? (samples[n / 2 - 1] + samples[n / 2]) / 2.0 : samples[n / 2];
    s.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / n;
    double variance = 0;
    for (double v : samples)
    {
        variance += (v - s.mean) * (v - s.mean);
    }
    s.stddev = (n > 1) ? std::sqrt(variance / (n - 1)) : 0;
    return s;
}

void print_header(const std::string& title)
{
    std::cout << "\n" << std::string(70, '=') << "\n  " << title << "\n" << std::string(70, '=') << "\n\n";
}

void print_result_header()
{
    std::cout << std::setw(28) << "Library" << std::setw(14) << "Median(ns)" << std::setw(14) << "Mean(ns)"
              << std::setw(12) << "Stddev\n";
    std::cout << std::string(68, '-') << "\n";
}

void print_result(const std::string& name, const Stats& s)
{
    std::cout << std::fixed << std::setprecision(2) << std::setw(28) << name << std::setw(14) << s.median
              << std::setw(14) << s.mean << std::setw(12) << s.stddev << "\n";
}

std::vector<size_t> generate_indices(size_t n, size_t max_index, uint64_t seed)
{
    std::vector<size_t> indices(n);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, max_index - 1);
    for (size_t i = 0; i < n; ++i)
    {
        indices[i] = dist(rng);
    }
    return indices;
}

std::vector<size_t> generate_sparse_indices(size_t k, size_t max_index, uint64_t seed)
{
    std::unordered_set<size_t> seen;
    std::vector<size_t> indices;
    indices.reserve(k);
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<size_t> dist(0, max_index - 1);
    while (indices.size() < k)
    {
        size_t idx = dist(rng);
        if (seen.insert(idx).second)
        {
            indices.push_back(idx);
        }
    }
    std::sort(indices.begin(), indices.end());
    return indices;
}

// ============================================================================
// Section 1: Single-Bit Operations
// ============================================================================

template <size_t N>
void benchmark_single_bit_operations(size_t iterations)
{
    print_header("Section 1: Single-Bit Operations (N=" + std::to_string(N) + ")");
    auto indices = generate_indices(iterations, N, g_config.seed);

    std::cout << "--- Set Operation ---\n";
    print_result_header();

    // Fat-P BitSet
    {
        std::vector<double> samples;
        for (size_t w = 0; w < WARMUP_RUNS(); ++w)
        {
            fat_p::BitSet<N> bits;
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.setUnchecked(indices[i]);
            }
            benchmark_sink += bits.count();
        }
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            fat_p::BitSet<N> bits;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.setUnchecked(indices[i]);
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.count();
        }
        print_result("fat_p::BitSet", compute_stats(samples));
    }

    // std::bitset
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            std::bitset<N> bits;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.set(indices[i]);
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.count();
        }
        print_result("std::bitset", compute_stats(samples));
    }

#if HAS_BOOST
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            boost::dynamic_bitset<> bits(N);
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.set(indices[i]);
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.count();
        }
        print_result("boost::dynamic_bitset", compute_stats(samples));
    }
#endif

#if HAS_LLVM
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            llvm::BitVector bits(static_cast<unsigned>(N));
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.set(static_cast<unsigned>(indices[i]));
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.count();
        }
        print_result("llvm::BitVector", compute_stats(samples));
    }
#endif

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            roaring::Roaring bits;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.add(static_cast<uint32_t>(indices[i]));
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.cardinality();
        }
        print_result("roaring::Roaring", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            bm::bvector<> bits;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bits.set(indices[i]);
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += bits.count();
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 2: Population Count
// ============================================================================

template <size_t N>
void benchmark_population_count(size_t iterations)
{
    print_header("Section 2: Population Count (N=" + std::to_string(N) + ")");
    auto sparse = generate_sparse_indices(N / 4, N, g_config.seed);

    fat_p::BitSet<N> fatp_bits;
    std::bitset<N> std_bits;
    for (size_t idx : sparse)
    {
        fatp_bits.setUnchecked(idx);
        std_bits.set(idx);
    }

#if HAS_BOOST
    boost::dynamic_bitset<> boost_bits(N);
    for (size_t idx : sparse)
    {
        boost_bits.set(idx);
    }
#endif
#if HAS_LLVM
    llvm::BitVector llvm_bits(static_cast<unsigned>(N));
    for (size_t idx : sparse)
    {
        llvm_bits.set(static_cast<unsigned>(idx));
    }
#endif
#if HAS_CROARING
    roaring::Roaring roaring_bits;
    for (size_t idx : sparse)
    {
        roaring_bits.add(static_cast<uint32_t>(idx));
    }
#endif
#if HAS_BITMAGIC
    bm::bvector<> bm_bits;
    for (size_t idx : sparse)
    {
        bm_bits.set(static_cast<bm::bvector<>::size_type>(idx));
    }
#endif

    print_result_header();

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += fatp_bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet", compute_stats(samples));
    }

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += std_bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("std::bitset", compute_stats(samples));
    }

#if HAS_BOOST
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += boost_bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("boost::dynamic_bitset", compute_stats(samples));
    }
#endif

#if HAS_LLVM
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += llvm_bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("llvm::BitVector", compute_stats(samples));
    }
#endif

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += roaring_bits.cardinality();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring (cached)", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                sum += bm_bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 3: Find Operations (KEY DIFFERENTIATOR)
// ============================================================================

template <size_t N>
void benchmark_find_operations(size_t num_set_bits)
{
    print_header("Section 3: Find Operations (N=" + std::to_string(N) + ", k=" + std::to_string(num_set_bits) + ")");
    std::cout << "std::bitset has NO find_first/find_next - requires O(N) scan.\n\n";

    auto sparse = generate_sparse_indices(num_set_bits, N, g_config.seed);
    fat_p::BitSet<N> fatp_bits;
    std::bitset<N> std_bits;
    for (size_t idx : sparse)
    {
        fatp_bits.setUnchecked(idx);
        std_bits.set(idx);
    }

#if HAS_BOOST
    boost::dynamic_bitset<> boost_bits(N);
    for (size_t idx : sparse)
    {
        boost_bits.set(idx);
    }
#endif
#if HAS_LLVM
    llvm::BitVector llvm_bits(static_cast<unsigned>(N));
    for (size_t idx : sparse)
    {
        llvm_bits.set(static_cast<unsigned>(idx));
    }
#endif
#if HAS_CROARING
    roaring::Roaring roaring_bits;
    for (size_t idx : sparse)
    {
        roaring_bits.add(static_cast<uint32_t>(idx));
    }
#endif
#if HAS_BITMAGIC
    bm::bvector<> bm_bits;
    for (size_t idx : sparse)
    {
        bm_bits.set(static_cast<bm::bvector<>::size_type>(idx));
    }
#endif

    const size_t iters = 100000;
    std::cout << "--- find_first ---\n";
    print_result_header();

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                sum += fatp_bits.find_first();
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet", compute_stats(samples));
    }

    {
        auto find_first_std = [](const std::bitset<N>& bits) -> size_t {
            for (size_t i = 0; i < N; ++i)
            {
                if (bits.test(i))
                {
                    return i;
                }
            }
            return N;
        };
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                sum += find_first_std(std_bits);
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("std::bitset (manual scan)", compute_stats(samples));
    }

#if HAS_BOOST
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                sum += boost_bits.find_first();
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("boost::dynamic_bitset", compute_stats(samples));
    }
#endif

#if HAS_LLVM
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                int pos = llvm_bits.find_first();
                sum += (pos >= 0) ? static_cast<size_t>(pos) : 0;
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("llvm::BitVector", compute_stats(samples));
    }
#endif

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                sum += roaring_bits.minimum();
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                bm::bvector<>::size_type pos;
                if (bm_bits.find(0, pos))
                {
                    sum += pos;
                }
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif

    // Iterate all set bits
    std::cout << "\n--- Iterate All Set Bits ---\n";
    print_result_header();
    const size_t iter_iters = 1000;

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (size_t pos = fatp_bits.find_first(); pos < N; pos = fatp_bits.find_next(pos))
                {
                    sum += pos;
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet", compute_stats(samples));
    }

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (size_t j = 0; j < N; ++j)
                {
                    if (std_bits.test(j))
                    {
                        sum += j;
                    }
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("std::bitset (O(N) scan)", compute_stats(samples));
    }

#if HAS_BOOST
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (size_t pos = boost_bits.find_first(); pos != boost::dynamic_bitset<>::npos;
                     pos = boost_bits.find_next(pos))
                {
                    sum += pos;
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("boost::dynamic_bitset", compute_stats(samples));
    }
#endif

#if HAS_LLVM
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (int pos = llvm_bits.find_first(); pos >= 0; pos = llvm_bits.find_next(pos))
                {
                    sum += static_cast<size_t>(pos);
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("llvm::BitVector", compute_stats(samples));
    }
#endif

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (auto val : roaring_bits)
                {
                    sum += val;
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iter_iters; ++i)
            {
                for (auto it = bm_bits.first(); it != bm_bits.end(); ++it)
                {
                    sum += *it;
                }
            }
            samples.push_back(t.elapsed_ns() / iter_iters);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 4: Sparse Iteration Scaling
// ============================================================================

template <size_t N>
void benchmark_sparse_iteration(size_t num_set_bits)
{
    print_header("Section 4: Sparse Iteration (N=" + std::to_string(N) + ", k=" + std::to_string(num_set_bits) + ")");
    double density = static_cast<double>(num_set_bits) / N * 100.0;
    std::cout << "Density: " << std::fixed << std::setprecision(2) << density << "%\n\n";

    auto sparse = generate_sparse_indices(num_set_bits, N, g_config.seed);
    fat_p::BitSet<N> fatp_bits;
    std::bitset<N> std_bits;
    for (size_t idx : sparse)
    {
        fatp_bits.setUnchecked(idx);
        std_bits.set(idx);
    }

#if HAS_CROARING
    roaring::Roaring roaring_bits;
    for (size_t idx : sparse)
    {
        roaring_bits.add(static_cast<uint32_t>(idx));
    }
#endif
#if HAS_BITMAGIC
    bm::bvector<> bm_bits;
    for (size_t idx : sparse)
    {
        bm_bits.set(static_cast<bm::bvector<>::size_type>(idx));
    }
#endif

    const size_t iters = 10000;
    print_result_header();

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                for (size_t idx : fatp_bits)
                {
                    sum += idx;
                }
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet (iterator)", compute_stats(samples));
    }

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                for (size_t j = 0; j < N; ++j)
                {
                    if (std_bits.test(j))
                    {
                        sum += j;
                    }
                }
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("std::bitset (O(N) scan)", compute_stats(samples));
    }

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                for (auto val : roaring_bits)
                {
                    sum += val;
                }
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iters; ++i)
            {
                for (auto it = bm_bits.first(); it != bm_bits.end(); ++it)
                {
                    sum += *it;
                }
            }
            samples.push_back(t.elapsed_ns() / iters);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 5: Bitwise Operations
// ============================================================================

template <size_t N>
void benchmark_bitwise_operations(size_t iterations)
{
    print_header("Section 5: Bitwise Operations (N=" + std::to_string(N) + ")");
    auto indices1 = generate_sparse_indices(N / 4, N, g_config.seed);
    auto indices2 = generate_sparse_indices(N / 4, N, g_config.seed + 1);

    fat_p::BitSet<N> fatp_a, fatp_b;
    std::bitset<N> std_a, std_b;
    for (size_t idx : indices1)
    {
        fatp_a.setUnchecked(idx);
        std_a.set(idx);
    }
    for (size_t idx : indices2)
    {
        fatp_b.setUnchecked(idx);
        std_b.set(idx);
    }

#if HAS_CROARING
    roaring::Roaring roaring_a, roaring_b;
    for (size_t idx : indices1)
    {
        roaring_a.add(static_cast<uint32_t>(idx));
    }
    for (size_t idx : indices2)
    {
        roaring_b.add(static_cast<uint32_t>(idx));
    }
#endif
#if HAS_BITMAGIC
    bm::bvector<> bm_a, bm_b;
    for (size_t idx : indices1)
    {
        bm_a.set(idx);
    }
    for (size_t idx : indices2)
    {
        bm_b.set(idx);
    }
#endif

    std::cout << "--- AND Operation ---\n";
    print_result_header();

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                auto result = fatp_a & fatp_b;
                sum += result.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet", compute_stats(samples));
    }

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                auto result = std_a & std_b;
                sum += result.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("std::bitset", compute_stats(samples));
    }

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                auto result = roaring_a & roaring_b;
                sum += result.cardinality();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bm::bvector<> result = bm_a & bm_b;
                sum += result.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<>", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 6: Range Operations
// ============================================================================

template <size_t N>
void benchmark_range_operations(size_t range_size, size_t iterations)
{
    print_header("Section 6: Range Operations (N=" + std::to_string(N) + ", range=" + std::to_string(range_size) + ")");
    std::cout << "Fat-P has native setRange. Others require loops.\n\n";

    print_result_header();

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                fat_p::BitSet<N> bits;
                bits.setRange(0, range_size);
                sum += bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("fat_p::BitSet (native)", compute_stats(samples));
    }

    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                std::bitset<N> bits;
                for (size_t j = 0; j < range_size; ++j)
                {
                    bits.set(j);
                }
                sum += bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("std::bitset (loop)", compute_stats(samples));
    }

#if HAS_LLVM
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                llvm::BitVector bits(static_cast<unsigned>(N));
                bits.set(0, static_cast<unsigned>(range_size));
                sum += bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("llvm::BitVector (native)", compute_stats(samples));
    }
#endif

#if HAS_CROARING
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                roaring::Roaring bits;
                bits.addRange(0, range_size);
                sum += bits.cardinality();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("roaring::Roaring (native)", compute_stats(samples));
    }
#endif

#if HAS_BITMAGIC
    {
        std::vector<double> samples;
        for (size_t r = 0; r < MEASURED_RUNS(); ++r)
        {
            size_t sum = 0;
            Timer t;
            t.start();
            for (size_t i = 0; i < iterations; ++i)
            {
                bm::bvector<> bits;
                bits.set_range(0, range_size - 1);
                sum += bits.count();
            }
            samples.push_back(t.elapsed_ns() / iterations);
            benchmark_sink += sum;
        }
        print_result("bm::bvector<> (native)", compute_stats(samples));
    }
#endif
}

// ============================================================================
// Section 7: Object Sizes
// ============================================================================

void benchmark_object_sizes()
{
    print_header("Section 7: Object Sizes (bytes)");
    std::cout << std::setw(30) << "Type" << " | " << std::setw(12) << "Size\n" << std::string(45, '-') << "\n";

    std::cout << std::setw(30) << "fat_p::BitSet<64>" << " | " << std::setw(12) << sizeof(fat_p::BitSet<64>) << "\n";
    std::cout << std::setw(30) << "fat_p::BitSet<1024>" << " | " << std::setw(12) << sizeof(fat_p::BitSet<1024>)
              << "\n";
    std::cout << std::setw(30) << "std::bitset<64>" << " | " << std::setw(12) << sizeof(std::bitset<64>) << "\n";
    std::cout << std::setw(30) << "std::bitset<1024>" << " | " << std::setw(12) << sizeof(std::bitset<1024>) << "\n";

#if HAS_BOOST
    std::cout << std::setw(30) << "boost::dynamic_bitset (base)" << " | " << std::setw(12)
              << sizeof(boost::dynamic_bitset<>) << "\n";
#endif
#if HAS_LLVM
    std::cout << std::setw(30) << "llvm::BitVector (base)" << " | " << std::setw(12) << sizeof(llvm::BitVector) << "\n";
    std::cout << std::setw(30) << "llvm::SmallBitVector" << " | " << std::setw(12) << sizeof(llvm::SmallBitVector)
              << "\n";
#endif
#if HAS_CROARING
    std::cout << std::setw(30) << "roaring::Roaring (base)" << " | " << std::setw(12) << sizeof(roaring::Roaring)
              << "\n";
#endif
#if HAS_BITMAGIC
    std::cout << std::setw(30) << "bm::bvector<> (base)" << " | " << std::setw(12) << sizeof(bm::bvector<>) << "\n";
#endif
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;
    g_config = fat_p::bench::BenchConfig::fromEnv();

    // =========================================================================
    // Standardized header (via FatPBenchmarkHeader.h)
    // =========================================================================
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "BitSet";
    hdr.warmup = WARMUP_RUNS();
    hdr.measured = MEASURED_RUNS();
    hdr.seed = 12345; // BitSet uses fixed seed

    // Competitors
    hdr.competitors.push_back({"fat_p::BitSet", true, "primary"});
    hdr.competitors.push_back({"std::bitset", true, "baseline"});
#if HAS_BOOST
    hdr.competitors.push_back({"boost::dynamic_bitset", true, ""});
#else
    hdr.competitors.push_back({"boost::dynamic_bitset", false, "not detected"});
#endif
#if HAS_LLVM
    hdr.competitors.push_back({"llvm::BitVector", true, ""});
    hdr.competitors.push_back({"llvm::SmallBitVector", true, ""});
#else
    hdr.competitors.push_back({"llvm::BitVector", false, "not detected"});
#endif
#if HAS_CROARING
    hdr.competitors.push_back({"roaring::Roaring", true, "CRoaring"});
#else
    hdr.competitors.push_back({"roaring::Roaring", false, "not detected"});
#endif
#if HAS_BITMAGIC
    hdr.competitors.push_back({"bm::bvector", true, "BitMagic"});
#else
    hdr.competitors.push_back({"bm::bvector", false, "not detected"});
#endif

    hdr.has_extended_config = false;
    hdr.is_multi_library = true;
    hdr.has_correctness_checks = false;

    fat_p::bench::print_standard_header(hdr);


    constexpr size_t MEDIUM = 1024;
    constexpr size_t LARGE = 10000;

    benchmark_single_bit_operations<MEDIUM>(100000);
    benchmark_population_count<MEDIUM>(100000);
    benchmark_find_operations<MEDIUM>(100);
    benchmark_find_operations<LARGE>(1000);
    benchmark_sparse_iteration<MEDIUM>(50);
    benchmark_sparse_iteration<LARGE>(100);
    benchmark_bitwise_operations<MEDIUM>(10000);
    benchmark_range_operations<MEDIUM>(100, 10000);
    benchmark_object_sizes();

    std::cout << "\n======================================================================\n";
    std::cout << "  Benchmark Complete\n";
    std::cout << "======================================================================\n";
    return 0;
}
