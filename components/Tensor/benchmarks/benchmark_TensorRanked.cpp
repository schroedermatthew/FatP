/**
 * @file benchmark_TensorRanked.cpp
 * @brief Ranked, dynamic, and fully static Tensor object-size and operation baselines.
 *
 * Build:
 *   g++ -std=c++20 -O3 -DNDEBUG -march=native -iquote include/fat_p \
 *       components/Tensor/benchmarks/benchmark_TensorRanked.cpp -o benchmark_TensorRanked
 *   cl /std:c++20 /O2 /DNDEBUG /EHsc /Iinclude\fat_p \
 *       components\Tensor\benchmarks\benchmark_TensorRanked.cpp /Fe:benchmark_TensorRanked.exe \
 *       /link advapi32.lib
 */

/*
FATP_META:
  meta_version: 1
  component: TensorRanked
  file_role: benchmark
  path: components/Tensor/benchmarks/benchmark_TensorRanked.cpp
  namespace: fat_p
  layer: Testing
  summary: "Object-size, construction, copy, indexing, kernel, and adaptation evidence for TensorRanked."
  api_stability: in_work
  related:
    docs:
      - components/Tensor/docs/User Manual - TensorRanked.md
      - components/Tensor/docs/Design Note - Tensor Architecture Additions Plan.md
    headers:
      - include/fat_p/TensorRanked.h
    tests:
      - components/Tensor/tests/test_TensorRanked.cpp
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

#include "FatPBenchmarkRunner.h"
#include "TensorAlgorithms.h"
#include "TensorRanked.h"
#include "TensorStatic.h"

#include <cstddef>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace
{

using fat_p::DynamicExtents;
using fat_p::Matrix;
using fat_p::RankedExtents;
using fat_p::RankedTensor;
using fat_p::RankedTensorLayout;
using fat_p::RankedTensorView;
using fat_p::StaticTensor;
using fat_p::Tensor;
using fat_p::add;
using fat_p::asDynamicView;
using fat_p::asRankedView;
using fat_p::bench::BenchmarkRunner;
using fat_p::bench::DoNotOptimize;

constexpr std::size_t iterations = 2'000;

template <std::size_t Rank>
void printRankSize()
{
    std::cout << "  " << Rank << ',' << sizeof(RankedExtents<Rank>) << ','
              << sizeof(RankedTensorLayout<Rank>) << ',' << sizeof(RankedTensorView<double, Rank>)
              << ',' << sizeof(RankedTensor<double, Rank>) << '\n';
}

} // namespace

int main()
{
    BenchmarkRunner runner("TensorRanked");
    runner.printHeader();

    std::cout << "Object sizes (bytes; observations, not ABI guarantees)\n";
    std::cout << "  rank,extents,layout,borrowed_view,owner\n";
    printRankSize<0>();
    printRankSize<1>();
    printRankSize<2>();
    printRankSize<3>();
    printRankSize<4>();
    printRankSize<5>();
    printRankSize<6>();
    printRankSize<7>();
    printRankSize<8>();
    std::cout << "  dynamic," << sizeof(DynamicExtents) << ',' << sizeof(fat_p::TensorLayout)
              << ',' << sizeof(fat_p::TensorView<double>) << ',' << sizeof(Tensor<double>) << "\n\n";

    RankedTensor<double, 2> ranked({4, 4}, 1.0);
    Tensor<double> dynamic({4, 4}, 1.0);
    StaticTensor<double, Matrix<4, 4>> fixed(1.0);
    RankedTensor<double, 8> rankedHigh({1, 1, 1, 1, 1, 1, 4, 4}, 1.0);
    Tensor<double> dynamicHigh({1, 1, 1, 1, 1, 1, 4, 4}, 1.0);

    runner.section("CONSTRUCTION AND COPY")
        .contract("Each reported operation constructs or copies one 4x4 owner");
    runner.addWithOps("ranked_construct", [] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            RankedTensor<double, 2> value({4, 4}, 1.0);
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("dynamic_construct", [] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            Tensor<double> value({4, 4}, 1.0);
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("static_construct", [] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            StaticTensor<double, Matrix<4, 4>> value(1.0);
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("ranked_copy", [&ranked] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = ranked;
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("dynamic_copy", [&dynamic] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = dynamic;
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("static_copy", [&fixed] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = fixed;
            DoNotOptimize(value.data());
        }
        return iterations;
    });

    runner.section("INDEXING AND NATIVE KERNEL")
        .contract("Indexing reports logical element reads; add reports one materialized 4x4 result");
    runner.addWithOps("ranked_index", [&ranked] {
        double sum = 0.0;
        for (std::size_t repeat = 0; repeat < iterations; ++repeat)
        {
            for (std::size_t row = 0; row < 4; ++row)
            {
                for (std::size_t column = 0; column < 4; ++column)
                {
                    sum += ranked(row, column);
                }
            }
        }
        DoNotOptimize(sum);
        return iterations * 16;
    });
    runner.addWithOps("dynamic_index", [&dynamic] {
        double sum = 0.0;
        for (std::size_t repeat = 0; repeat < iterations; ++repeat)
        {
            for (std::size_t row = 0; row < 4; ++row)
            {
                for (std::size_t column = 0; column < 4; ++column)
                {
                    sum += dynamic(row, column);
                }
            }
        }
        DoNotOptimize(sum);
        return iterations * 16;
    });
    runner.addWithOps("static_index", [&fixed] {
        double sum = 0.0;
        for (std::size_t repeat = 0; repeat < iterations; ++repeat)
        {
            for (std::size_t row = 0; row < 4; ++row)
            {
                for (std::size_t column = 0; column < 4; ++column)
                {
                    sum += fixed.at(row, column);
                }
            }
        }
        DoNotOptimize(sum);
        return iterations * 16;
    });
    runner.addWithOps("ranked_add", [&ranked] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = add(ranked, ranked);
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("dynamic_add", [&dynamic] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = add(dynamic, dynamic);
            DoNotOptimize(value.data());
        }
        return iterations;
    });
    runner.addWithOps("static_add", [&fixed] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto value = fixed + fixed;
            DoNotOptimize(value.data());
        }
        return iterations;
    });

    runner.section("ZERO-COPY VIEW ADAPTATION")
        .contract("Rank-two stays within inline dynamic metadata; rank-eight records fallback metadata cost");
    runner.addWithOps("ranked2_to_dynamic_view", [&ranked] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto view = asDynamicView(ranked);
            DoNotOptimize(view.data());
        }
        return iterations;
    });
    runner.addWithOps("dynamic2_to_ranked_view", [&dynamic] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto view = asRankedView<2>(dynamic);
            DoNotOptimize(view.data());
        }
        return iterations;
    });
    runner.addWithOps("ranked8_to_dynamic_view", [&rankedHigh] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto view = asDynamicView(rankedHigh);
            DoNotOptimize(view.data());
        }
        return iterations;
    });
    runner.addWithOps("dynamic8_to_ranked_view", [&dynamicHigh] {
        for (std::size_t index = 0; index < iterations; ++index)
        {
            auto view = asRankedView<8>(dynamicHigh);
            DoNotOptimize(view.data());
        }
        return iterations;
    });

    runner.run();
    runner.exportCsvIfConfigured();
    runner.exportJsonIfConfigured();
    runner.printReport();
    return 0;
}
