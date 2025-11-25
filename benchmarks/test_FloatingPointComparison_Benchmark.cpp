// test_FloatingPointComparison_Benchmark.cpp
// Comprehensive benchmark suite for FloatingPointComparison library

#include <iostream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <random>
#include <vector>
#include "FloatingPointComparison.h"
#include "FatPTest.h"

namespace fat_p::testing
{

// ============================================================================
// Benchmark Infrastructure
// ============================================================================

class BenchmarkData
{
public:
    static constexpr size_t DATASET_SIZE = 10000;
    std::vector<double> values_near_zero;
    std::vector<double> values_normal;
    std::vector<double> values_large;
    std::vector<double> values_mixed;
    
    BenchmarkData()
    {
        std::mt19937_64 rng(42);
        std::uniform_real_distribution<double> dist_near_zero(1e-10, 1e-8);
        std::uniform_real_distribution<double> dist_normal(1.0, 10.0);
        std::uniform_real_distribution<double> dist_large(1e6, 1e8);
        std::uniform_real_distribution<double> dist_mixed(1e-10, 1e8);
        
        values_near_zero.reserve(DATASET_SIZE);
        values_normal.reserve(DATASET_SIZE);
        values_large.reserve(DATASET_SIZE);
        values_mixed.reserve(DATASET_SIZE);
        
        for (size_t i = 0; i < DATASET_SIZE; ++i)
        {
            values_near_zero.push_back(dist_near_zero(rng));
            values_normal.push_back(dist_normal(rng));
            values_large.push_back(dist_large(rng));
            values_mixed.push_back(dist_mixed(rng));
        }
    }
};

// ============================================================================
// Policy Performance Benchmarks
// ============================================================================

void benchmark_policy_standard(const BenchmarkData& data)
{
    std::cout << "\n=== Policy: StandardComparisonPolicy ===" << std::endl;
    
    benchmark_detailed("Standard - Normal values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
        {
            result = floatEqual(data.values_normal[i], 
                              data.values_normal[i + 1], 
                              1e-9);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Standard - Near zero", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_near_zero.size() - 1; ++i)
        {
            result = floatEqual(data.values_near_zero[i], 
                              data.values_near_zero[i + 1], 
                              1e-12);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Standard - Large values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_large.size() - 1; ++i)
        {
            result = floatEqual(data.values_large[i], 
                              data.values_large[i + 1], 
                              1e-3);
        }
        return result;
    }, 1000);
}

void benchmark_policy_relative(const BenchmarkData& data)
{
    std::cout << "\n=== Policy: RelativeComparisonPolicy ===" << std::endl;
    
    benchmark_detailed("Relative - Normal values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
        {
            result = floatEqual<double, RelativeComparisonPolicy>(
                data.values_normal[i], 
                data.values_normal[i + 1], 
                1e-9);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Relative - Large values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_large.size() - 1; ++i)
        {
            result = floatEqual<double, RelativeComparisonPolicy>(
                data.values_large[i], 
                data.values_large[i + 1], 
                1e-6);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Relative - Mixed scales", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_mixed.size() - 1; ++i)
        {
            result = floatEqual<double, RelativeComparisonPolicy>(
                data.values_mixed[i], 
                data.values_mixed[i + 1], 
                1e-6);
        }
        return result;
    }, 1000);
}

void benchmark_policy_ulp(const BenchmarkData& data)
{
    std::cout << "\n=== Policy: UlpComparisonPolicy ===" << std::endl;
    
    benchmark_detailed("ULP - Normal values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
        {
            result = floatEqual<double, UlpComparisonPolicy>(
                data.values_normal[i], 
                data.values_normal[i + 1], 
                4.0);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("ULP - Large values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_large.size() - 1; ++i)
        {
            result = floatEqual<double, UlpComparisonPolicy>(
                data.values_large[i], 
                data.values_large[i + 1], 
                4.0);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("ULP - Adjacent values (1 ULP)", [&]()
    {
        volatile bool result = false;
        double base = 1.0;
        for (size_t i = 0; i < 1000; ++i)
        {
            double next = std::nextafter(base, 2.0);
            result = floatEqual<double, UlpComparisonPolicy>(base, next, 1.0);
            base = next;
        }
        return result;
    }, 1000);
}

void benchmark_policy_hybrid(const BenchmarkData& data)
{
    std::cout << "\n=== Policy: HybridComparisonPolicy ===" << std::endl;
    
    benchmark_detailed("Hybrid - Normal values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
        {
            result = approximateEqual(data.values_normal[i], 
                                    data.values_normal[i + 1], 
                                    1e-9, 1e-12);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Hybrid - Near zero", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_near_zero.size() - 1; ++i)
        {
            result = approximateEqual(data.values_near_zero[i], 
                                    data.values_near_zero[i + 1], 
                                    1e-9, 1e-12);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Hybrid - Large values", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_large.size() - 1; ++i)
        {
            result = approximateEqual(data.values_large[i], 
                                    data.values_large[i + 1], 
                                    1e-6, 1e-3);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Hybrid - Mixed scales", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < data.values_mixed.size() - 1; ++i)
        {
            result = approximateEqual(data.values_mixed[i], 
                                    data.values_mixed[i + 1], 
                                    1e-6, 1e-12);
        }
        return result;
    }, 1000);
}

// ============================================================================
// Cross-Policy Comparisons
// ============================================================================

void benchmark_policy_comparison(const BenchmarkData& data)
{
    std::cout << "\n=== Cross-Policy Performance Comparison ===" << std::endl;
    
    benchmark_compare(
        "Standard",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < 1000; ++i)
            {
                result = floatEqual(data.values_normal[i], 
                                  data.values_normal[i + 1], 
                                  1e-9);
            }
            return result;
        },
        "Relative",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < 1000; ++i)
            {
                result = floatEqual<double, RelativeComparisonPolicy>(
                    data.values_normal[i], 
                    data.values_normal[i + 1], 
                    1e-9);
            }
            return result;
        },
        10000
    );
    
    benchmark_compare(
        "Hybrid",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < 1000; ++i)
            {
                result = approximateEqual(data.values_normal[i], 
                                        data.values_normal[i + 1], 
                                        1e-9, 1e-12);
            }
            return result;
        },
        "ULP",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < 1000; ++i)
            {
                result = floatEqual<double, UlpComparisonPolicy>(
                    data.values_normal[i], 
                    data.values_normal[i + 1], 
                    4.0);
            }
            return result;
        },
        10000
    );
}

// ============================================================================
// Special Value Benchmarks
// ============================================================================

void benchmark_special_values()
{
    std::cout << "\n=== Special Value Handling ===" << std::endl;
    
    double nan_val = std::numeric_limits<double>::quiet_NaN();
    double inf_val = std::numeric_limits<double>::infinity();
    double zero_val = 0.0;
    
    benchmark_detailed("NaN comparisons", [&]()
    {
        volatile bool result = false;
        for (int i = 0; i < 10000; ++i)
        {
            result = approximateEqual(nan_val, nan_val);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Infinity comparisons", [&]()
    {
        volatile bool result = false;
        for (int i = 0; i < 10000; ++i)
        {
            result = approximateEqual(inf_val, inf_val);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Zero comparisons", [&]()
    {
        volatile bool result = false;
        for (int i = 0; i < 10000; ++i)
        {
            result = approximateEqual(zero_val, -zero_val);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Subnormal comparisons", [&]()
    {
        volatile bool result = false;
        double denorm = std::numeric_limits<double>::denorm_min();
        for (int i = 0; i < 10000; ++i)
        {
            result = approximateEqual(denorm, zero_val);
        }
        return result;
    }, 1000);
}

// ============================================================================
// Comparison vs Raw Implementations
// ============================================================================

void benchmark_vs_raw_checks(const BenchmarkData& data)
{
    std::cout << "\n=== Library vs Raw Implementation ===" << std::endl;
    
    double epsilon = 1e-9;
    
    benchmark_compare(
        "Raw fabs check",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
            {
                result = (std::fabs(data.values_normal[i] - 
                                   data.values_normal[i + 1]) <= epsilon);
            }
            return result;
        },
        "StandardComparisonPolicy",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
            {
                result = floatEqual(data.values_normal[i], 
                                  data.values_normal[i + 1], 
                                  epsilon);
            }
            return result;
        },
        1000
    );
    
    benchmark_compare(
        "Raw relative check",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
            {
                double a = data.values_normal[i];
                double b = data.values_normal[i + 1];
                double maxAbs = std::max(std::fabs(a), std::fabs(b));
                result = (std::fabs(a - b) <= epsilon * maxAbs);
            }
            return result;
        },
        "RelativeComparisonPolicy",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < data.values_normal.size() - 1; ++i)
            {
                result = floatEqual<double, RelativeComparisonPolicy>(
                    data.values_normal[i], 
                    data.values_normal[i + 1], 
                    epsilon);
            }
            return result;
        },
        1000
    );
}

// ============================================================================
// Float vs Double Performance
// ============================================================================

void benchmark_type_performance()
{
    std::cout << "\n=== Float vs Double Performance ===" << std::endl;
    
    std::vector<float> floats(10000);
    std::vector<double> doubles(10000);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist_f(1.0f, 10.0f);
    std::uniform_real_distribution<double> dist_d(1.0, 10.0);
    
    for (size_t i = 0; i < 10000; ++i)
    {
        floats[i] = dist_f(rng);
        doubles[i] = dist_d(rng);
    }
    
    benchmark_compare(
        "Float Standard",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < floats.size() - 1; ++i)
            {
                result = floatEqual(floats[i], floats[i + 1], 1e-5f);
            }
            return result;
        },
        "Double Standard",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < doubles.size() - 1; ++i)
            {
                result = floatEqual(doubles[i], doubles[i + 1], 1e-9);
            }
            return result;
        },
        1000
    );
    
    benchmark_compare(
        "Float ULP",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < floats.size() - 1; ++i)
            {
                result = floatEqual<float, UlpComparisonPolicy>(
                    floats[i], floats[i + 1], 4.0f);
            }
            return result;
        },
        "Double ULP",
        [&]()
        {
            volatile bool result = false;
            for (size_t i = 0; i < doubles.size() - 1; ++i)
            {
                result = floatEqual<double, UlpComparisonPolicy>(
                    doubles[i], doubles[i + 1], 4.0);
            }
            return result;
        },
        1000
    );
}

// ============================================================================
// Best/Worst Case Performance
// ============================================================================

void benchmark_branch_prediction()
{
    std::cout << "\n=== Branch Prediction Analysis ===" << std::endl;
    
    std::vector<double> identical(10000, 1.0);
    std::vector<double> different(10000);
    std::vector<double> alternating(10000);
    
    for (size_t i = 0; i < different.size(); ++i)
    {
        different[i] = static_cast<double>(i);
        alternating[i] = (i % 2 == 0) ? 1.0 : 2.0;
    }
    
    benchmark_detailed("Always equal (best case)", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < identical.size() - 1; ++i)
        {
            result = approximateEqual(identical[i], identical[i + 1]);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Always different (worst case)", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < different.size() - 1; ++i)
        {
            result = approximateEqual(different[i], different[i + 1]);
        }
        return result;
    }, 1000);
    
    benchmark_detailed("Alternating (unpredictable)", [&]()
    {
        volatile bool result = false;
        for (size_t i = 0; i < alternating.size() - 1; ++i)
        {
            result = approximateEqual(alternating[i], alternating[i + 1]);
        }
        return result;
    }, 1000);
}

// ============================================================================
// Main Benchmark Runner
// ============================================================================

bool benchmark_FloatingPointComparison()
{
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  FloatingPointComparison.h - Performance Benchmarks\n";
    std::cout << "================================================================\n";
    std::cout << "\nTest Environment:\n";
    std::cout << "  Platform: " << sizeof(void*) * 8 << "-bit\n";
    std::cout << "  Float size: " << sizeof(float) << " bytes\n";
    std::cout << "  Double size: " << sizeof(double) << " bytes\n";
    std::cout << "  Build: " 
#ifdef NDEBUG
              << "Release (optimized)\n";
#else
              << "Debug (not optimized - benchmarks may be slow)\n";
#endif
    std::cout << "\nNOTE: Benchmarks should be run in Release mode for accurate results.\n";
    
    BenchmarkData data;
    
    benchmark_policy_standard(data);
    benchmark_policy_relative(data);
    benchmark_policy_ulp(data);
    benchmark_policy_hybrid(data);
    
    benchmark_policy_comparison(data);
    benchmark_special_values();
    benchmark_vs_raw_checks(data);
    benchmark_type_performance();
    benchmark_branch_prediction();
    
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  Benchmark Suite Complete\n";
    std::cout << "================================================================\n";
    
    return true;
}

} // namespace fat_p::testing

#ifdef ENABLE_BENCHMARK_APPLICATION
int main()
{
    return fat_p::testing::benchmark_FloatingPointComparison() ? 0 : 1;
}
#endif
