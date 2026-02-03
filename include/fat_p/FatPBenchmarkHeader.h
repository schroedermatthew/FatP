#pragma once
/*
FATP_META:
  meta_version: 1
  component: Benchmark
  file_role: internal_header
  path: include/fat_p/FatPBenchmarkHeader.h
  namespace: fat_p::bench
  layer: Testing
  summary: "Standardized benchmark header output utilities."
  api_stability: in_work
  related:
    docs_search: "Benchmark"
    headers:
      - include/fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: manual
*/

// ===========================================================================
// FatPBenchmarkHeader.h - Standardized benchmark header output
// ===========================================================================
// Usage:
//   #include "FatPBenchmarkHeader.h"
//   
//   int main() {
//       fat_p::bench::HeaderConfig cfg;
//       cfg.component = "StableHashMap";
//       cfg.competitors = {
//           {"fat_p::StableHashMap", true, "primary"},
//           {"tsl::robin_map", true, ""},
//           {"std::unordered_map", true, "baseline"},
//           {"abseil::flat_hash_map", false, "not found"},
//       };
//       cfg.has_extended_config = true;
//       cfg.target_work = 5000000;
//       cfg.min_batch_ms = 50;
//       
//       fat_p::bench::print_standard_header(cfg);
//       // ... benchmark code ...
//   }
// ===========================================================================

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <ctime>

namespace fat_p::bench {

// ---------------------------------------------------------------------------
// Configuration structures
// ---------------------------------------------------------------------------

struct Competitor {
    std::string name;
    bool detected = true;
    std::string note;  // e.g., "primary", "baseline", "header-only"
};

struct HeaderConfig {
    // Required
    std::string component;  // e.g., "StableHashMap"
    
    // Platform (auto-detected if empty)
    std::string platform;   // e.g., "Windows-x64"
    std::string compiler;   // e.g., "MSVC-1942"
    
    // Basic config (from env vars, use defaults if not set)
    std::size_t warmup = 3;
    std::size_t measured = 15;
    std::uint64_t seed = 12345;
    
    // Competitors
    std::vector<Competitor> competitors;
    
    // Extended config (only printed if has_extended_config = true)
    bool has_extended_config = false;
    std::size_t target_work = 0;
    std::size_t min_batch_ms = 50;
    bool scope_enabled = true;
    bool stabilize_enabled = true;
    bool cooldown_enabled = true;
    
    // Cooling delays (only printed if any > 0)
    std::size_t cool_section_ms = 0;
    std::size_t cool_size_ms = 0;
    std::size_t cool_case_ms = 0;
    
    // Design invariants (use standard set, can customize)
    bool is_multi_library = true;
    bool has_correctness_checks = true;
    bool has_stabilization = true;
    
    // CPU info (populated by get_cpu_info())
    double cpu_current_mhz = 0;
    double cpu_base_mhz = 0;
    bool cpu_ref_is_max = false;
};

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------

inline std::string detect_platform() {
#if defined(_WIN32) || defined(_WIN64)
    #if defined(_M_X64) || defined(__x86_64__)
        return "Windows-x64";
    #elif defined(_M_ARM64)
        return "Windows-arm64";
    #else
        return "Windows-x86";
    #endif
#elif defined(__linux__)
    #if defined(__x86_64__)
        return "Linux-x64";
    #elif defined(__aarch64__)
        return "Linux-arm64";
    #else
        return "Linux-x86";
    #endif
#elif defined(__APPLE__)
    #if defined(__arm64__)
        return "macOS-arm64";
    #else
        return "macOS-x64";
    #endif
#else
    return "Unknown";
#endif
}

inline std::string detect_compiler() {
#if defined(_MSC_VER)
    return "MSVC-" + std::to_string(_MSC_VER);
#elif defined(__clang__)
    return "Clang-" + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__);
#elif defined(__GNUC__)
    return "GCC-" + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__);
#else
    return "Unknown";
#endif
}

// ---------------------------------------------------------------------------
// Timestamp formatting
// ---------------------------------------------------------------------------

inline std::string current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now;
#if defined(_WIN32)
    localtime_s(&tm_now, &time_t_now);
#else
    localtime_r(&time_t_now, &tm_now);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Header printing functions
// ---------------------------------------------------------------------------

inline void print_banner(const std::string& component) {
    std::cout << std::string(80, '=') << "\n";
    std::cout << "  fat_p::" << component << " Benchmark Suite\n";
    std::cout << std::string(80, '=') << "\n\n";
}

inline void print_platform_line(const HeaderConfig& cfg) {
    std::string platform = cfg.platform.empty() ? detect_platform() : cfg.platform;
    std::string compiler = cfg.compiler.empty() ? detect_compiler() : cfg.compiler;
    
    std::cout << "Platform: " << platform << " " << compiler 
              << " | warmup=" << cfg.warmup 
              << " measured=" << cfg.measured 
              << " seed=" << cfg.seed << "\n\n";
}

inline void print_competitors(const std::vector<Competitor>& competitors) {
    if (competitors.empty()) return;
    
    std::cout << "Competitors:\n";
    for (const auto& c : competitors) {
        std::cout << "  [" << (c.detected ? "x" : " ") << "] " << c.name;
        if (!c.note.empty()) {
            std::cout << " (" << c.note << ")";
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

inline void print_extended_config(const HeaderConfig& cfg) {
    if (!cfg.has_extended_config) return;
    
    std::cout << "Configuration:\n";
    if (cfg.target_work > 0) {
        std::cout << "  Target work:    " << cfg.target_work << " ops/batch\n";
    }
    std::cout << "  Min batch ms:   " << cfg.min_batch_ms << "\n";
    std::cout << "  Scope:          " << (cfg.scope_enabled ? "ON" : "OFF") << "\n";
    std::cout << "  Stabilize:      " << (cfg.stabilize_enabled ? "ON" : "OFF") << "\n";
    std::cout << "  Cooldown:       " << (cfg.cooldown_enabled ? "ON" : "OFF") << "\n";
    std::cout << "\n";
}

inline void print_cpu_line(const HeaderConfig& cfg) {
    if (cfg.cpu_current_mhz <= 0) return;
    
    std::cout << "CPU: " << static_cast<int>(cfg.cpu_current_mhz) << " MHz";
    
    if (cfg.cpu_base_mhz > 0) {
        const char* ref_label = cfg.cpu_ref_is_max ? "max" : "base";
        std::cout << " (" << ref_label << ": " << static_cast<int>(cfg.cpu_base_mhz) << " MHz)";
        
        // Only show throttle/turbo when reference is true base
        if (!cfg.cpu_ref_is_max && cfg.cpu_base_mhz > 0) {
            double throttle_pct = (1.0 - cfg.cpu_current_mhz / cfg.cpu_base_mhz) * 100.0;
            if (throttle_pct > 5.0) {
                std::cout << " [THROTTLED " << static_cast<int>(throttle_pct) << "%]";
            } else if (throttle_pct < -5.0) {
                std::cout << " [TURBO]";
            }
        }
    }
    std::cout << "\n\n";
}

inline void print_design_invariants(const HeaderConfig& cfg) {
    std::cout << "Design Invariants:\n";
    int n = 1;
    
    if (cfg.is_multi_library) {
        std::cout << "  " << n++ << ". Round-robin execution with randomized order per run\n";
    }
    std::cout << "  " << n++ << ". Setup/teardown outside timed regions\n";
    if (cfg.is_multi_library) {
        std::cout << "  " << n++ << ". All libraries observe same distribution of machine states\n";
    }
    std::cout << "  " << n++ << ". Medians are the primary reported statistic\n";
    if (cfg.has_correctness_checks) {
        std::cout << "  " << n++ << ". Correctness verified after each benchmark\n";
    }
    if (cfg.has_stabilization && cfg.stabilize_enabled) {
        std::cout << "  " << n++ << ". CPU frequency stabilized before measurement\n";
    }
    std::cout << "\n";
}

inline void print_cooling_delays(const HeaderConfig& cfg) {
    if (cfg.cool_section_ms == 0 && cfg.cool_size_ms == 0 && cfg.cool_case_ms == 0) {
        return;
    }
    std::cout << "Cooling: section=" << cfg.cool_section_ms << "ms"
              << " size=" << cfg.cool_size_ms << "ms"
              << " case=" << cfg.cool_case_ms << "ms\n\n";
}

inline void print_stability_status(double current_mhz, double base_mhz, 
                                   double variance_pct, bool stable) {
    std::cout << "[" << current_timestamp() << "] ";
    if (stable) {
        std::cout << "CPU stable at " << static_cast<int>(current_mhz) << " MHz";
        if (base_mhz > 0) {
            int pct_of_base = static_cast<int>(100.0 * current_mhz / base_mhz);
            std::cout << " (" << pct_of_base << "% of base";
            if (variance_pct >= 0) {
                std::cout << ", variance " << std::fixed << std::setprecision(1) 
                          << variance_pct << "%";
            }
            std::cout << ")";
        }
    } else {
        std::cout << "WARNING: CPU not stable";
        if (current_mhz > 0) {
            std::cout << " (" << static_cast<int>(current_mhz) << " MHz";
            if (base_mhz > 0) {
                int pct_of_base = static_cast<int>(100.0 * current_mhz / base_mhz);
                std::cout << ", " << pct_of_base << "% of base";
            }
            std::cout << ")";
        }
    }
    std::cout << "\n\n";
}

// ---------------------------------------------------------------------------
// Main entry point
// ---------------------------------------------------------------------------

inline void print_standard_header(const HeaderConfig& cfg) {
    // 1. BenchmarkScope line (printed separately by BenchmarkScope RAII)
    
    // 2. Title banner
    print_banner(cfg.component);
    
    // 3. Platform line
    print_platform_line(cfg);
    
    // 4. Competitors
    print_competitors(cfg.competitors);
    
    // 5. Extended configuration
    print_extended_config(cfg);
    
    // 6. CPU diagnostics
    print_cpu_line(cfg);
    
    // 7. Design invariants
    print_design_invariants(cfg);
    
    // 8. Cooling delays
    print_cooling_delays(cfg);
    
    // 9. Stability status (call separately after stabilization)
}

} // namespace fat_p::bench
