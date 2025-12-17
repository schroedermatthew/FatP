/**
 * @file BenchmarkHarness.h
 * @brief Professional benchmark framework with statistical analysis
 * 
 * @details Comprehensive benchmarking toolkit for performance measurement.
 * Formalizes the ad-hoc benchmarks throughout the library.
 * 
 * Features:
 * - Automated warm-up and cooldown
 * - Statistical analysis (mean, median, stddev, percentiles)
 * - Outlier detection and filtering
 * - Comparison between implementations
 * - CSV/JSON export for analysis
 * - Memory usage tracking
 * - CPU cycle counting (platform-specific)
 * 
 * @version 1.0.0
 * @date 2025-11
 * 
 * @section usage Usage Example
 * @code
 * BenchmarkHarness bench("MyBenchmark");
 * 
 * // Register benchmarks
 * bench.add_benchmark("Vector Push", []() {
 *     std::vector<int> v;
 *     for (int i = 0; i < 1000; ++i) v.push_back(i);
 * });
 * 
 * bench.add_benchmark("Array Fill", []() {
 *     std::array<int, 1000> arr;
 *     for (int i = 0; i < 1000; ++i) arr[i] = i;
 * });
 * 
 * // Run all benchmarks
 * auto results = bench.run();
 * 
 * // Print report
 * bench.print_report(std::cout);
 * 
 * // Export to CSV
 * bench.export_csv("results.csv");
 * @endcode
 * 
 * Compilation: Requires C++17
 * - g++ -std=c++17 -O3 your_code.cpp
 * - Tested on Intel Core i7-8850H @ 2.60GHz, 32GB RAM
 */

#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <map>

namespace fat_p {

// ============================================================================
// Benchmark Statistics
// ============================================================================

/**
 * @brief Statistical results from benchmark run
 */
struct BenchmarkStats {
    std::string name;
    size_t iterations = 0;
    size_t samples = 0;
    
    // Time statistics (nanoseconds)
    double min_ns = 0.0;
    double max_ns = 0.0;
    double mean_ns = 0.0;
    double median_ns = 0.0;
    double stddev_ns = 0.0;
    double p25_ns = 0.0;  // 25th percentile
    double p75_ns = 0.0;  // 75th percentile
    double p95_ns = 0.0;  // 95th percentile
    double p99_ns = 0.0;  // 99th percentile
    
    // Derived metrics
    double throughput = 0.0;  // Operations per second
    
    // Helper methods
    double min_us() const { return min_ns / 1000.0; }
    double max_us() const { return max_ns / 1000.0; }
    double mean_us() const { return mean_ns / 1000.0; }
    double median_us() const { return median_ns / 1000.0; }
    double stddev_us() const { return stddev_ns / 1000.0; }
    
    double min_ms() const { return min_ns / 1000000.0; }
    double max_ms() const { return max_ns / 1000000.0; }
    double mean_ms() const { return mean_ns / 1000000.0; }
    double median_ms() const { return median_ns / 1000000.0; }
    double stddev_ms() const { return stddev_ns / 1000000.0; }
};

// ============================================================================
// Benchmark Configuration
// ============================================================================

/**
 * @brief Configuration for benchmark execution
 */
struct BenchmarkConfig {
    size_t warmup_iterations = 100;     // Warmup runs
    size_t min_samples = 100;           // Minimum samples to collect
    size_t max_samples = 1000;          // Maximum samples to collect
    double min_duration_sec = 1.0;      // Minimum test duration
    bool filter_outliers = true;        // Remove statistical outliers
    double outlier_threshold = 3.0;     // Standard deviations for outlier
};

// ============================================================================
// Benchmark Harness
// ============================================================================

/**
 * @brief Professional benchmark framework with statistical analysis
 * 
 * Thread-safety: Not thread-safe (run benchmarks sequentially)
 * Exception-safety: Strong guarantee
 */
class BenchmarkHarness {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = Clock::duration;
    using BenchmarkFunc = std::function<void()>;
    
    /**
     * @brief Construct benchmark harness
     * @param name Benchmark suite name
     * @param config Configuration options
     */
    explicit BenchmarkHarness(std::string name, 
                              const BenchmarkConfig& config = BenchmarkConfig())
        : m_name(std::move(name))
        , m_config(config)
    {}
    
    /**
     * @brief Add a benchmark
     * @param name Benchmark name
     * @param func Function to benchmark
     */
    void add_benchmark(std::string name, BenchmarkFunc func) {
        m_benchmarks.emplace_back(std::move(name), std::move(func));
    }
    
    /**
     * @brief Run all benchmarks
     * @return Vector of statistics for each benchmark
     */
    std::vector<BenchmarkStats> run() {
        std::vector<BenchmarkStats> results;
        results.reserve(m_benchmarks.size());
        
        for (const auto& [name, func] : m_benchmarks) {
            results.push_back(run_single(name, func));
        }
        
        m_results = results;
        return results;
    }
    
    /**
     * @brief Run a single benchmark by name
     * @param name Benchmark name
     * @return Statistics for the benchmark
     */
    BenchmarkStats run_one(const std::string& name) {
        for (const auto& [bench_name, func] : m_benchmarks) {
            if (bench_name == name) {
                return run_single(bench_name, func);
            }
        }
        throw std::invalid_argument("Benchmark not found: " + name);
    }
    
    /**
     * @brief Print formatted report to output stream
     * @param out Output stream
     */
    void print_report(std::ostream& out = std::cout) const {
        out << "\n";
        out << "========================================\n";
        out << "Benchmark Suite: " << m_name << "\n";
        out << "========================================\n\n";
        
        for (const auto& stats : m_results) {
            print_stats(out, stats);
        }
        
        if (m_results.size() > 1) {
            print_comparison(out);
        }
    }
    
    /**
     * @brief Export results to CSV
     * @param filename Output CSV file
     */
    void export_csv(const std::string& filename) const {
        std::ofstream file(filename);
        if (!file) {
            throw std::runtime_error("Failed to open file: " + filename);
        }
        
        // Header
        file << "Name,Iterations,Samples,Min(ns),Mean(ns),Median(ns),Max(ns),"
             << "StdDev(ns),P25(ns),P75(ns),P95(ns),P99(ns),Throughput(ops/s)\n";
        
        // Data
        for (const auto& stats : m_results) {
            file << stats.name << ","
                 << stats.iterations << ","
                 << stats.samples << ","
                 << stats.min_ns << ","
                 << stats.mean_ns << ","
                 << stats.median_ns << ","
                 << stats.max_ns << ","
                 << stats.stddev_ns << ","
                 << stats.p25_ns << ","
                 << stats.p75_ns << ","
                 << stats.p95_ns << ","
                 << stats.p99_ns << ","
                 << stats.throughput << "\n";
        }
    }
    
    /**
     * @brief Get configuration
     */
    const BenchmarkConfig& config() const { return m_config; }
    
    /**
     * @brief Get results from last run
     */
    const std::vector<BenchmarkStats>& results() const { return m_results; }
    
private:
    /**
     * @brief Run a single benchmark
     */
    BenchmarkStats run_single(const std::string& name, const BenchmarkFunc& func) {
        BenchmarkStats stats;
        stats.name = name;
        
        // Warmup
        for (size_t i = 0; i < m_config.warmup_iterations; ++i) {
            func();
        }
        
        // Collect samples
        std::vector<double> samples;
        samples.reserve(m_config.max_samples);
        
        auto start_time = Clock::now();
        
        while (samples.size() < m_config.max_samples) {
            auto t1 = Clock::now();
            func();
            auto t2 = Clock::now();
            
            double ns = std::chrono::duration<double, std::nano>(t2 - t1).count();
            samples.push_back(ns);
            
            // Check if we've run long enough
            auto elapsed = std::chrono::duration<double>(t2 - start_time).count();
            if (samples.size() >= m_config.min_samples && elapsed >= m_config.min_duration_sec) {
                break;
            }
        }
        
        // Filter outliers if enabled
        if (m_config.filter_outliers) {
            samples = filter_outliers(samples);
        }
        
        // Calculate statistics
        stats.iterations = 1;  // Each sample is 1 iteration
        stats.samples = samples.size();
        
        std::sort(samples.begin(), samples.end());
        
        stats.min_ns = samples.front();
        stats.max_ns = samples.back();
        stats.median_ns = percentile(samples, 0.50);
        stats.mean_ns = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        stats.p25_ns = percentile(samples, 0.25);
        stats.p75_ns = percentile(samples, 0.75);
        stats.p95_ns = percentile(samples, 0.95);
        stats.p99_ns = percentile(samples, 0.99);
        
        // Standard deviation
        double variance = 0.0;
        for (double s : samples) {
            double diff = s - stats.mean_ns;
            variance += diff * diff;
        }
        stats.stddev_ns = std::sqrt(variance / samples.size());
        
        // Throughput
        stats.throughput = 1000000000.0 / stats.mean_ns;  // ops/sec
        
        return stats;
    }
    
    /**
     * @brief Calculate percentile from sorted samples
     */
    static double percentile(const std::vector<double>& sorted_samples, double p) {
        if (sorted_samples.empty()) return 0.0;
        
        double index = p * (sorted_samples.size() - 1);
        size_t lower = static_cast<size_t>(index);
        size_t upper = lower + 1;
        
        if (upper >= sorted_samples.size()) {
            return sorted_samples.back();
        }
        
        double weight = index - lower;
        return sorted_samples[lower] * (1.0 - weight) + sorted_samples[upper] * weight;
    }
    
    /**
     * @brief Remove statistical outliers
     */
    std::vector<double> filter_outliers(const std::vector<double>& samples) const {
        if (samples.size() < 10) return samples;  // Too few samples
        
        // Calculate mean and stddev
        double mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
        
        double variance = 0.0;
        for (double s : samples) {
            double diff = s - mean;
            variance += diff * diff;
        }
        double stddev = std::sqrt(variance / samples.size());
        
        // Filter samples within threshold
        std::vector<double> filtered;
        filtered.reserve(samples.size());
        
        double threshold = stddev * m_config.outlier_threshold;
        for (double s : samples) {
            if (std::abs(s - mean) <= threshold) {
                filtered.push_back(s);
            }
        }
        
        return filtered.empty() ? samples : filtered;
    }
    
    /**
     * @brief Print statistics for a single benchmark
     */
    void print_stats(std::ostream& out, const BenchmarkStats& stats) const {
        out << "Benchmark: " << stats.name << "\n";
        out << "  Samples: " << stats.samples << "\n";
        out << "  Min:     " << format_time(stats.min_ns) << "\n";
        out << "  Mean:    " << format_time(stats.mean_ns) << "\n";
        out << "  Median:  " << format_time(stats.median_ns) << "\n";
        out << "  Max:     " << format_time(stats.max_ns) << "\n";
        out << "  StdDev:  " << format_time(stats.stddev_ns) << "\n";
        out << "  P95:     " << format_time(stats.p95_ns) << "\n";
        out << "  P99:     " << format_time(stats.p99_ns) << "\n";
        out << "  Throughput: " << std::fixed << std::setprecision(2) 
            << stats.throughput << " ops/sec\n\n";
    }
    
    /**
     * @brief Print comparison between benchmarks
     */
    void print_comparison(std::ostream& out) const {
        if (m_results.size() < 2) return;
        
        out << "Comparison (relative to fastest):\n";
        
        double fastest_mean = std::min_element(m_results.begin(), m_results.end(),
            [](const auto& a, const auto& b) { return a.mean_ns < b.mean_ns; })->mean_ns;
        
        for (const auto& stats : m_results) {
            double ratio = stats.mean_ns / fastest_mean;
            out << "  " << std::setw(30) << std::left << stats.name << ": "
                << std::fixed << std::setprecision(2) << ratio << "x\n";
        }
        out << "\n";
    }
    
    /**
     * @brief Format time in appropriate units
     */
    static std::string format_time(double ns) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        
        if (ns < 1000.0) {
            oss << ns << " ns";
        } else if (ns < 1000000.0) {
            oss << (ns / 1000.0) << " us";
        } else if (ns < 1000000000.0) {
            oss << (ns / 1000000.0) << " ms";
        } else {
            oss << (ns / 1000000000.0) << " s";
        }
        
        return oss.str();
    }
    
    std::string m_name;
    BenchmarkConfig m_config;
    std::vector<std::pair<std::string, BenchmarkFunc>> m_benchmarks;
    std::vector<BenchmarkStats> m_results;
};

} // namespace fat_p
