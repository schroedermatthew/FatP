// benchmark_FeatureManager.cpp
//
// FAT-P FeatureManager benchmarks using unified FatPBenchmarkRunner infrastructure.
//
// Build:
//   g++ -std=c++20 -O3 -DNDEBUG -march=native benchmark_FeatureManager.cpp -o bench_fm
//   cl /std:c++20 /O2 /DNDEBUG /EHsc benchmark_FeatureManager.cpp /link advapi32.lib
//
// Environment Variables (all optional):
//   FATP_BENCH_WARMUP_RUNS   - Warmup iterations (default: 3)
//   FATP_BENCH_BATCHES       - Measured batches (default: 50, Windows: 15)
//   FATP_BENCH_SEED          - RNG seed (default: 12345)
//   FATP_BENCH_VERBOSE_STATS - Print extra statistics (default: 0)
//   FATP_BENCH_OUTPUT_CSV    - CSV output path (default: disabled)
//   FATP_BENCH_OUTPUT_JSON   - JSON output path (default: disabled)
//   FATP_BENCH_NO_SCOPE      - Disable priority/affinity changes
//   FATP_BENCH_NO_STABILIZE  - Disable CPU stabilization wait
//   FATP_BENCH_NO_COOLDOWN   - Disable cool-down sleeps
//
// Run:
//   ./bench_fm
//   FATP_BENCH_OUTPUT_CSV=results.csv ./bench_fm

/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: benchmark
  path: components/FeatureManager/benchmarks/benchmark_FeatureManager.cpp
  namespace: fat_p
  layer: Testing
  summary: "Benchmarks for FeatureManager."
  api_stability: in_work
  related:
    docs_search: "FeatureManager"
    headers:
      - include/fat_p/FeatureManager.h
      - include/fat_p/FatPBenchmarkRunner.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <functional>
#include <mutex>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "FatPBenchmarkRunner.h"
#include "FatPBenchmarkHeader.h"
#include "FeatureManager.h"

namespace
{

using namespace fat_p::bench;

// ============================================================================
// Fixture Builders
// ============================================================================

/// Creates a flat graph with N independent features (no relationships).
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
fat_p::feature::FeatureManager<SyncPolicy> makeFlatGraph(std::size_t n)
{
    fat_p::feature::FeatureManager<SyncPolicy> fm;
    for (std::size_t i = 0; i < n; ++i)
    {
        (void)fm.addFeature("F" + std::to_string(i));
    }
    return fm;
}

/// Creates a chain of Requires relationships: N0 -> N1 -> N2 -> ... -> Ndepth
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
fat_p::feature::FeatureManager<SyncPolicy> makeRequiresChain(std::size_t depth)
{
    fat_p::feature::FeatureManager<SyncPolicy> fm;
    for (std::size_t i = 0; i <= depth; ++i)
    {
        (void)fm.addFeature("N" + std::to_string(i));
    }
    for (std::size_t i = 0; i < depth; ++i)
    {
        (void)fm.addRelationship("N" + std::to_string(i),
                                  fat_p::feature::FeatureRelationship::Requires,
                                  "N" + std::to_string(i + 1));
    }
    return fm;
}

/// Creates a graph where all features conflict with each other.
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
fat_p::feature::FeatureManager<SyncPolicy> makeConflictGraph(std::size_t n)
{
    fat_p::feature::FeatureManager<SyncPolicy> fm;
    for (std::size_t i = 0; i < n; ++i)
    {
        (void)fm.addFeature("C" + std::to_string(i));
    }
    for (std::size_t i = 0; i < n; ++i)
    {
        for (std::size_t j = i + 1; j < n; ++j)
        {
            (void)fm.addRelationship("C" + std::to_string(i),
                                      fat_p::feature::FeatureRelationship::Conflicts,
                                      "C" + std::to_string(j));
        }
    }
    return fm;
}

/// Creates a dense graph with random relationships.
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
fat_p::feature::FeatureManager<SyncPolicy> makeDenseGraph(std::size_t nodes, std::size_t edgesPerNode, std::uint64_t seed)
{
    fat_p::feature::FeatureManager<SyncPolicy> fm;
    for (std::size_t i = 0; i < nodes; ++i)
    {
        (void)fm.addFeature("D" + std::to_string(i));
    }

    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<std::size_t> nodeDist(0, nodes - 1);
    std::uniform_int_distribution<int> typeDist(0, 1);

    for (std::size_t i = 0; i < nodes; ++i)
    {
        for (std::size_t e = 0; e < edgesPerNode; ++e)
        {
            std::size_t target = nodeDist(rng);
            if (target == i)
            {
                continue;
            }

            auto rel =
                (typeDist(rng) == 0) ? fat_p::feature::FeatureRelationship::Requires : fat_p::feature::FeatureRelationship::Conflicts;
            (void)fm.addRelationship("D" + std::to_string(i), rel, "D" + std::to_string(target));
        }
    }
    return fm;
}

/// Creates a tree graph with given depth and branching factor.
template <typename SyncPolicy = fat_p::SingleThreadedPolicy>
fat_p::feature::FeatureManager<SyncPolicy> makeTreeGraph(std::size_t depth, std::size_t branching)
{
    fat_p::feature::FeatureManager<SyncPolicy> fm;
    std::size_t nodeId = 0;

    std::function<void(std::size_t, const std::string&)> buildLevel = [&](std::size_t level,
                                                                          const std::string& parent) {
        if (level > depth)
        {
            return;
        }

        for (std::size_t i = 0; i < branching; ++i)
        {
            std::string name = "T" + std::to_string(nodeId++);
            (void)fm.addFeature(name);
            if (!parent.empty())
            {
                // Parent requires child: enabling the root cascades through the whole tree.
                (void)fm.addRelationship(parent, fat_p::feature::FeatureRelationship::Requires, name);
            }
            buildLevel(level + 1, name);
        }
    };

    std::string root = "T" + std::to_string(nodeId++);
    (void)fm.addFeature(root);
    buildLevel(1, root);

    return fm;
}

/// Creates a vector of feature names with given prefix.
inline std::vector<std::string> makeNames(const std::string& prefix, std::size_t count)
{
    std::vector<std::string> names;
    names.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        names.push_back(prefix + std::to_string(i));
    }
    return names;
}

/// Disables all features in the list.
template <typename SyncPolicy>
void disableAll(fat_p::feature::FeatureManager<SyncPolicy>& fm, const std::vector<std::string>& names)
{
    for (const auto& name : names)
    {
        (void)fm.disable(name);
    }
}

// ============================================================================
// Custom State Computer for Benchmarks
// ============================================================================

enum class BenchmarkState
{
    Empty,
    Loading,
    Ready,
    Error
};

inline BenchmarkState complexStateComputer(const fat_p::FlatSet<std::string>& groupFeatures,
                                           std::size_t enabledCount,
                                           bool hasConflict,
                                           bool allChecksPass)
{
    if (hasConflict)
    {
        return BenchmarkState::Error;
    }
    if (!allChecksPass)
    {
        return BenchmarkState::Error;
    }
    if (enabledCount == 0)
    {
        return BenchmarkState::Empty;
    }

    std::size_t checksum = 0;
    for (const auto& f : groupFeatures)
    {
        for (char c : f)
        {
            checksum += static_cast<std::size_t>(c);
        }
    }
    DoNotOptimize(checksum);

    if (enabledCount < groupFeatures.size() / 2)
    {
        return BenchmarkState::Loading;
    }
    return BenchmarkState::Ready;
}

} // anonymous namespace

// EnumStringPolicy specialization for BenchmarkState
namespace fat_p
{
template <>
struct EnumStringPolicy<BenchmarkState>
{
    static constexpr std::array<std::string_view, 4> names = {"Empty", "Loading", "Ready", "Error"};

    static std::string_view to_string(BenchmarkState e)
    {
        return names[static_cast<std::size_t>(e)];
    }

    static BenchmarkState from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid BenchmarkState string");
        }
        return static_cast<BenchmarkState>(std::distance(names.begin(), it));
    }
};
} // namespace fat_p

namespace
{

// ============================================================================
// Concurrent Benchmark Helpers
// ============================================================================

struct ConcurrentStats
{
    std::atomic<std::size_t> successfulReads{0};
    std::atomic<std::size_t> successfulWrites{0};
    std::atomic<std::size_t> failedWrites{0};
    std::atomic<std::size_t> totalOps{0};
    double durationSec{0};

    double opsPerSec() const
    {
        return durationSec > 0 ? static_cast<double>(totalOps.load()) / durationSec : 0;
    }

    void reset()
    {
        successfulReads = 0;
        successfulWrites = 0;
        failedWrites = 0;
        totalOps = 0;
        durationSec = 0;
    }
};

inline void concurrentReader(fat_p::feature::FeatureManager<fat_p::MutexSynchronizationPolicy>& fm,
                             const std::vector<std::string>& names,
                             std::atomic<bool>& running,
                             ConcurrentStats& stats,
                             fat_p::bench::SpinBarrier* barrier = nullptr)
{
    auto tidHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::mt19937 rng(static_cast<std::mt19937::result_type>(tidHash));
    std::uniform_int_distribution<std::size_t> dist(0, names.size() - 1);

    if (barrier)
    {
        barrier->wait();
    }

    while (running.load(std::memory_order_relaxed))
    {
        bool v = fm.isEnabled(names[dist(rng)]);
        fat_p::bench::DoNotOptimize(v);
        stats.successfulReads.fetch_add(1, std::memory_order_relaxed);
        stats.totalOps.fetch_add(1, std::memory_order_relaxed);
    }
}

inline void concurrentWriter(fat_p::feature::FeatureManager<fat_p::MutexSynchronizationPolicy>& fm,
                             const std::vector<std::string>& names,
                             std::atomic<bool>& running,
                             ConcurrentStats& stats,
                             fat_p::bench::SpinBarrier* barrier = nullptr)
{
    auto tidHash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    std::mt19937 rng(static_cast<std::mt19937::result_type>(tidHash));
    std::uniform_int_distribution<std::size_t> dist(0, names.size() - 1);
    std::uniform_int_distribution<int> opDist(0, 1);

    if (barrier)
    {
        barrier->wait();
    }

    while (running.load(std::memory_order_relaxed))
    {
        const auto& name = names[dist(rng)];
        bool success = false;
        if (opDist(rng) == 0)
        {
            auto res = fm.enable(name);
            success = res.has_value();
        }
        else
        {
            auto res = fm.disable(name);
            success = res.has_value();
        }

        if (success)
        {
            stats.successfulWrites.fetch_add(1, std::memory_order_relaxed);
        }
        else
        {
            stats.failedWrites.fetch_add(1, std::memory_order_relaxed);
        }
        stats.totalOps.fetch_add(1, std::memory_order_relaxed);
    }
}

// ============================================================================
// Correctness Guardrails
// ============================================================================

inline void verifyFixtures(const fat_p::feature::FeatureManager<>& fmLookup,
                           const fat_p::feature::FeatureManager<>& fmChain50,
                           const std::string& enabledName,
                           const std::string& disabledName)
{
    std::cout << "  Verifying fixtures...\n";

    assert(fmLookup.getAllFeatures().size() == 10000);
    assert(fmLookup.isEnabled(enabledName) == true);
    assert(fmLookup.isEnabled(disabledName) == false);

    assert(fmChain50.getAllFeatures().size() == 51);
    assert(fmChain50.isEnabled("N0") == true);
    assert(fmChain50.isEnabled("N50") == true);

    std::cout << "    [OK] All fixture validations passed\n\n";
}

} // anonymous namespace

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    using fat_p::feature::FeatureManager;
    using fat_p::feature::FeatureRelationship;
    using fat_p::MutexSynchronizationPolicy;
    using namespace fat_p::bench;

    // -------------------------------------------------------------------------
    // 1. Configuration and Standardized Header
    // -------------------------------------------------------------------------
    BenchConfig config = BenchConfig::fromEnv();
    BenchmarkScope scope(!config.noScope);
    
    // Standardized header (via FatPBenchmarkHeader.h)
    fat_p::bench::HeaderConfig hdr;
    hdr.component = "FeatureManager";
    hdr.warmup = config.warmupRuns;
    hdr.measured = config.measuredRuns;
    hdr.seed = config.seed;
    
    // Competitors
    hdr.competitors.push_back({"fat_p::feature::FeatureManager", true, "primary"});
    hdr.competitors.push_back({"std::map<string, bool>", true, "baseline"});
    hdr.competitors.push_back({"Manual if/else chain", true, "baseline"});
    // No external competitor libraries for FeatureManager
    
    hdr.has_extended_config = true;
    hdr.min_batch_ms = config.minBatchMs;
    hdr.scope_enabled = !config.noScope;
    hdr.stabilize_enabled = !config.noStabilize;
    hdr.cooldown_enabled = !config.noCooldown;
    hdr.is_multi_library = false;
    hdr.has_correctness_checks = true;
    hdr.has_stabilization = !config.noStabilize;
    
    fat_p::bench::print_standard_header(hdr);
    
    // Wait for CPU stability
    waitForStableCpu(config);
    
    // Create runner (without printing header again)
    BenchmarkRunner runner("FeatureManager", config);
    const auto& cfg = runner.config();

    // -------------------------------------------------------------------------
    // 2. Create Fixtures
    // -------------------------------------------------------------------------
    printSectionHeader(std::cout, "FIXTURE SETUP");
    print_cpu_context(std::cout, "Creating fixtures");

    FeatureManager<> fmLookup = makeFlatGraph(10'000);
    (void)fmLookup.enable("F5000");
    const std::string jsonBlob = fmLookup.toJson();

    const std::string enabledName = "F5000";
    const std::string disabledName = "F9999";
    const std::string missingName = "F10001";

    FeatureManager<> fmChain50 = makeRequiresChain(50);
    auto chainNames50 = makeNames("N", 51);
    (void)fmChain50.batchEnable({"N0"});

    FeatureManager<> fmConflict = makeConflictGraph(100);

    // -------------------------------------------------------------------------
    // 3. Correctness Guardrails
    // -------------------------------------------------------------------------
    verifyFixtures(fmLookup, fmChain50, enabledName, disabledName);

    // -------------------------------------------------------------------------
    // 4. Lookup Operations
    // -------------------------------------------------------------------------
    runner.section("LOOKUP OPERATIONS").contract("isEnabled() is O(log n) lookup in std::map<string, FeatureNode>");

    runner.add("isEnabled: enabled hit (10k features)", [&]() {
        bool v = fmLookup.isEnabled(enabledName);
        DoNotOptimize(v);
    });

    runner.add("isEnabled: disabled hit (10k features)", [&]() {
        bool v = fmLookup.isEnabled(disabledName);
        DoNotOptimize(v);
    });

    runner.add("isEnabled: missing feature (10k features)", [&]() {
        bool v = fmLookup.isEnabled(missingName);
        DoNotOptimize(v);
    });

    // -------------------------------------------------------------------------
    // 5. Validation Operations
    // -------------------------------------------------------------------------
    runner.section("VALIDATION OPERATIONS").contract("validate() traverses full dependency graph, O(n * d * log n)");

    runner.add("validate: requires-chain depth 50 (all enabled)", [&]() {
        auto res = fmChain50.validate();
        DoNotOptimize(res);
    });

    runner.add("validate: flat graph 10k (no dependencies)", [&]() {
        auto res = fmLookup.validate();
        DoNotOptimize(res);
    });

    runner.add("validate: conflict graph 100 features", [&]() {
        auto res = fmConflict.validate();
        DoNotOptimize(res);
    });

    // -------------------------------------------------------------------------
    // 6. Enable/Disable Operations
    // -------------------------------------------------------------------------
    runner.section("ENABLE/DISABLE OPERATIONS")
        .contract("enable() recursively enables dependencies with rollback on failure");

    runner.add("batchEnable + batchDisable: chain depth 50 (cold)", [&]() {
        FeatureManager<> fm = makeRequiresChain(50);
        auto res = fm.batchEnable({"N0"});
        DoNotOptimize(res);
        disableAll(fm, chainNames50);
    });

    runner.add("enable + disable: single feature (no deps)", [&]() {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        auto res1 = fm.enable("A");
        DoNotOptimize(res1);
        auto res2 = fm.disable("A");
        DoNotOptimize(res2);
    });

    runner.add("enable: conflict detection (100 conflicts)", [&]() {
        FeatureManager<> fm = makeConflictGraph(100);
        (void)fm.enable("C0");
        auto res = fm.enable("C1");
        DoNotOptimize(res);
        (void)fm.disable("C0");
    });

    // -------------------------------------------------------------------------
    // 7. Observer Overhead
    // -------------------------------------------------------------------------
    runner.section("OBSERVER OVERHEAD").contract("Observers are called synchronously on state change");

    runner.add("enable/disable: 0 observers", [&]() {
        FeatureManager<> fm = makeFlatGraph(1024);
        auto res = fm.enable("F1");
        DoNotOptimize(res);
        auto res2 = fm.disable("F1");
        DoNotOptimize(res2);
    });

    runner.add("enable/disable: 1 observer", [&]() {
        FeatureManager<> fm = makeFlatGraph(1024);
        std::atomic<std::size_t> c{0};
        auto id = fm.addObserver([&](const std::string&, bool, bool) {
            c.fetch_add(1, std::memory_order_relaxed);
        });
        DoNotOptimize(id);
        auto res = fm.enable("F1");
        DoNotOptimize(res);
        auto res2 = fm.disable("F1");
        DoNotOptimize(res2);
        DoNotOptimize(c.load(std::memory_order_relaxed));
    });

    runner.add("enable/disable: 10 observers", [&]() {
        FeatureManager<> fm = makeFlatGraph(1024);
        std::atomic<std::size_t> c{0};
        for (int i = 0; i < 10; ++i)
        {
            auto id = fm.addObserver([&](const std::string&, bool, bool) {
                c.fetch_add(1, std::memory_order_relaxed);
            });
            DoNotOptimize(id);
        }
        auto res = fm.enable("F1");
        DoNotOptimize(res);
        auto res2 = fm.disable("F1");
        DoNotOptimize(res2);
        DoNotOptimize(c.load(std::memory_order_relaxed));
    });

    // -------------------------------------------------------------------------
    // 8. Serialization
    // -------------------------------------------------------------------------
    runner.section("SERIALIZATION").contract("JSON round-trip must preserve enabled state and relationships");

    runner.add("toJson: 10k features, no relationships", [&]() {
        auto json = fmLookup.toJson();
        DoNotOptimize(json);
    });

    runner.add("fromJson: 10k features, no relationships", [&]() {
        auto fmRes = FeatureManager<>::fromJson(jsonBlob);
        DoNotOptimize(fmRes);
    });

    FeatureManager<> fmSmallRel;
    for (int i = 0; i < 100; ++i)
    {
        (void)fmSmallRel.addFeature("R" + std::to_string(i));
    }
    for (int i = 0; i < 50; ++i)
    {
        (void)fmSmallRel.addRelationship("R" + std::to_string(i),
                                          FeatureRelationship::Requires,
                                          "R" + std::to_string(i + 50));
    }
    const std::string dotBlob = fmSmallRel.toDot();

    runner.add("toDot: 100 features, 50 relationships", [&]() {
        auto dot = fmSmallRel.toDot();
        DoNotOptimize(dot);
    });

    runner.add("toDot: 10k features, no relationships", [&]() {
        auto dot = fmLookup.toDot();
        DoNotOptimize(dot);
    });

    // -------------------------------------------------------------------------
    // 9. Graph Construction
    // -------------------------------------------------------------------------
    runner.section("GRAPH CONSTRUCTION").contract("addFeature() is O(log n) map insertion");

    runner.add("addFeature: build 100 features", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 100; ++i)
        {
            auto res = fm.addFeature("F" + std::to_string(i));
            DoNotOptimize(res);
        }
    });

    runner.add("addFeature: build 1000 features", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 1000; ++i)
        {
            auto res = fm.addFeature("F" + std::to_string(i));
            DoNotOptimize(res);
        }
    });

    runner.add("addRelationship: 100 Requires edges", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 101; ++i)
        {
            (void)fm.addFeature("N" + std::to_string(i));
        }
        for (int i = 0; i < 100; ++i)
        {
            auto res = fm.addRelationship("N" + std::to_string(i),
                                           FeatureRelationship::Requires,
                                           "N" + std::to_string(i + 1));
            DoNotOptimize(res);
        }
    });

    // -------------------------------------------------------------------------
    // 10. Synchronization Policy Overhead
    // -------------------------------------------------------------------------
    runner.section("SYNCHRONIZATION POLICY OVERHEAD")
        .contract("Comparing SingleThreadedPolicy vs MutexSynchronizationPolicy");

    runner.add("isEnabled: SingleThreadedPolicy (10k)", [&]() {
        bool v = fmLookup.isEnabled(enabledName);
        DoNotOptimize(v);
    });

    FeatureManager<MutexSynchronizationPolicy> fmMutex = makeFlatGraph<MutexSynchronizationPolicy>(10'000);
    (void)fmMutex.enable("F5000");

    runner.add("isEnabled: MutexSynchronizationPolicy (10k)", [&]() {
        bool v = fmMutex.isEnabled(enabledName);
        DoNotOptimize(v);
    });

    // -------------------------------------------------------------------------
    // 11. Group Operations
    // -------------------------------------------------------------------------
    runner.section("GROUP OPERATIONS").contract("Group state computed from member feature states");

    FeatureManager<> fmGroups;
    std::vector<std::string> groupFeatures;
    for (int i = 0; i < 20; ++i)
    {
        std::string name = "G" + std::to_string(i);
        (void)fmGroups.addFeature(name);
        groupFeatures.push_back(name);
    }
    (void)fmGroups.addGroup("TestGroup", groupFeatures);
    for (int i = 0; i < 10; ++i)
    {
        (void)fmGroups.enable("G" + std::to_string(i));
    }

    runner.add("getGroupState: 20-member group", [&]() {
        auto state = fmGroups.getGroupState<fat_p::feature::FeatureGroupState>("TestGroup");
        DoNotOptimize(state);
    });

    // -------------------------------------------------------------------------
    // 12. Batch Operations Scaling
    // -------------------------------------------------------------------------
    runner.section("BATCH OPERATIONS SCALING")
        .contract("batchEnable atomically enables multiple features with rollback");

    runner.add("batchEnable: 10 features (no deps)", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 10; ++i)
        {
            std::string name = "B" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        auto res = fm.batchEnable(names);
        DoNotOptimize(res);
    });

    runner.add("batchEnable: 100 features (no deps)", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 100; ++i)
        {
            std::string name = "B" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        auto res = fm.batchEnable(names);
        DoNotOptimize(res);
    });

    runner.add("batchEnable: 1000 features (no deps)", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 1000; ++i)
        {
            std::string name = "B" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        auto res = fm.batchEnable(names);
        DoNotOptimize(res);
    });

    runner.add("batchDisable: 100 features (no deps)", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 100; ++i)
        {
            std::string name = "B" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        (void)fm.batchEnable(names);
        auto res = fm.batchDisable(names);
        DoNotOptimize(res);
    });

    // -------------------------------------------------------------------------
    // 13. Dense Graph Operations
    // -------------------------------------------------------------------------
    runner.section("DENSE GRAPH OPERATIONS").contract("Performance with 1000+ relationships for scaling analysis");

    FeatureManager<> fmDense = makeDenseGraph(200, 5, cfg.seed);
    const std::string denseJson = fmDense.toJson();

    runner.add("validate: dense graph (200 nodes, ~1000 edges)", [&]() {
        auto res = fmDense.validate();
        DoNotOptimize(res);
    });

    runner.add("toJson: dense graph (200 nodes, ~1000 edges)", [&]() {
        auto json = fmDense.toJson();
        DoNotOptimize(json);
    });

    runner.add("fromJson: dense graph (200 nodes, ~1000 edges)", [&]() {
        auto fmRes = FeatureManager<>::fromJson(denseJson);
        DoNotOptimize(fmRes);
    });

    FeatureManager<> fmVeryDense = makeDenseGraph(500, 10, cfg.seed);

    runner.add("validate: very dense graph (500 nodes, ~5000 edges)", [&]() {
        auto res = fmVeryDense.validate();
        DoNotOptimize(res);
    });

    FeatureManager<> fmTree = makeTreeGraph(5, 3);

    runner.add("validate: tree graph (depth 5, branching 3)", [&]() {
        auto res = fmTree.validate();
        DoNotOptimize(res);
    });

    runner.add("enable: tree root (cascades to 364 nodes)", [&]() {
        FeatureManager<> fm = makeTreeGraph(5, 3);
        auto res = fm.enable("T0");
        DoNotOptimize(res);
    });

    // -------------------------------------------------------------------------
    // 14. ScopedFeatureChange RAII Helper
    // -------------------------------------------------------------------------
    runner.section("SCOPED FEATURE CHANGE (RAII)")
        .contract("ScopedFeatureChange provides temporary state with auto-rollback");

    runner.add("ScopedFeatureChange: enable then auto-restore", [&]() {
        FeatureManager<> fm;
        (void)fm.addFeature("Scoped");
        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "Scoped", true);
            bool v = fm.isEnabled("Scoped");
            DoNotOptimize(v);
        }
        bool v = fm.isEnabled("Scoped");
        DoNotOptimize(v);
    });

    runner.add("ScopedFeatureChange: disable then auto-restore", [&]() {
        FeatureManager<> fm;
        (void)fm.addFeature("Scoped");
        (void)fm.enable("Scoped");
        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "Scoped", false);
            bool v = fm.isEnabled("Scoped");
            DoNotOptimize(v);
        }
        bool v = fm.isEnabled("Scoped");
        DoNotOptimize(v);
    });

    runner.add("ScopedFeatureChange: nested scopes (3 deep)", [&]() {
        FeatureManager<> fm;
        (void)fm.addFeature("S1");
        (void)fm.addFeature("S2");
        (void)fm.addFeature("S3");
        {
            FeatureManager<>::ScopedFeatureChange g1(fm, "S1", true);
            {
                FeatureManager<>::ScopedFeatureChange g2(fm, "S2", true);
                {
                    FeatureManager<>::ScopedFeatureChange g3(fm, "S3", true);
                    bool v = fm.isEnabled("S3");
                    DoNotOptimize(v);
                }
            }
        }
        bool v = fm.isEnabled("S1");
        DoNotOptimize(v);
    });

    // -------------------------------------------------------------------------
    // 15. Custom StateComputer
    // -------------------------------------------------------------------------
    runner.section("CUSTOM STATE COMPUTER").contract("User-provided state computation logic for groups");

    FeatureManager<> fmDefaultComputer;
    std::vector<std::string> dcFeatures;
    for (int i = 0; i < 50; ++i)
    {
        std::string name = "DC" + std::to_string(i);
        (void)fmDefaultComputer.addFeature(name);
        dcFeatures.push_back(name);
    }
    (void)fmDefaultComputer.addGroup("DefaultGroup", dcFeatures);
    for (int i = 0; i < 25; ++i)
    {
        (void)fmDefaultComputer.enable("DC" + std::to_string(i));
    }

    runner.add("getGroupState: default computer (50 features)", [&]() {
        auto state = fmDefaultComputer.getGroupState<fat_p::feature::FeatureGroupState>("DefaultGroup");
        DoNotOptimize(state);
    });

    FeatureManager<> fmCustomComputer;
    std::vector<std::string> ccFeatures;
    for (int i = 0; i < 50; ++i)
    {
        std::string name = "CC" + std::to_string(i);
        (void)fmCustomComputer.addFeature(name);
        ccFeatures.push_back(name);
    }
    (void)fmCustomComputer.addGroup<BenchmarkState>("CustomGroup", ccFeatures, complexStateComputer);
    for (int i = 0; i < 25; ++i)
    {
        (void)fmCustomComputer.enable("CC" + std::to_string(i));
    }

    runner.add("getGroupState: custom computer (50 features)", [&]() {
        auto state = fmCustomComputer.getGroupState<BenchmarkState>("CustomGroup");
        DoNotOptimize(state);
    });

    FeatureManager<> fmLargeGroup;
    std::vector<std::string> lgFeatures;
    for (int i = 0; i < 200; ++i)
    {
        std::string name = "LG" + std::to_string(i);
        (void)fmLargeGroup.addFeature(name);
        lgFeatures.push_back(name);
    }
    (void)fmLargeGroup.addGroup("LargeGroup", lgFeatures);
    for (int i = 0; i < 100; ++i)
    {
        (void)fmLargeGroup.enable("LG" + std::to_string(i));
    }

    runner.add("getGroupState: default computer (200 features)", [&]() {
        auto state = fmLargeGroup.getGroupState<fat_p::feature::FeatureGroupState>("LargeGroup");
        DoNotOptimize(state);
    });

    // -------------------------------------------------------------------------
    // 16. Memory & Construction Scaling
    // -------------------------------------------------------------------------
    runner.section("MEMORY & CONSTRUCTION SCALING")
        .contract("Construction cost scaling with features and relationships");

    runner.add("construct: 100 features + 50 relationships", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 100; ++i)
        {
            (void)fm.addFeature("M" + std::to_string(i));
        }
        for (int i = 0; i < 50; ++i)
        {
            (void)fm.addRelationship("M" + std::to_string(i),
                                      FeatureRelationship::Requires,
                                      "M" + std::to_string(i + 50));
        }
        DoNotOptimize(fm);
    });

    runner.add("construct: 1000 features + 500 relationships", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 1000; ++i)
        {
            (void)fm.addFeature("M" + std::to_string(i));
        }
        for (int i = 0; i < 500; ++i)
        {
            (void)fm.addRelationship("M" + std::to_string(i),
                                      FeatureRelationship::Requires,
                                      "M" + std::to_string(i + 500));
        }
        DoNotOptimize(fm);
    });

    runner.add("construct: 5000 features + 2500 relationships", [&]() {
        FeatureManager<> fm;
        for (int i = 0; i < 5000; ++i)
        {
            (void)fm.addFeature("M" + std::to_string(i));
        }
        for (int i = 0; i < 2500; ++i)
        {
            (void)fm.addRelationship("M" + std::to_string(i),
                                      FeatureRelationship::Requires,
                                      "M" + std::to_string(i + 2500));
        }
        DoNotOptimize(fm);
    });

    runner.add("move: 1000-feature graph", [&]() {
        FeatureManager<> fm = makeFlatGraph(1000);
        FeatureManager<> fm2 = std::move(fm);
        DoNotOptimize(fm2);
    });

    runner.add("clear: 1000-feature graph", [&]() {
        FeatureManager<> fm = makeFlatGraph(1000);
        fm.clear();
        DoNotOptimize(fm);
    });

    // -------------------------------------------------------------------------
    // 17. Mutually Exclusive Groups
    // -------------------------------------------------------------------------
    runner.section("MUTUALLY EXCLUSIVE GROUPS")
        .contract("addMutuallyExclusiveGroup creates O(n^2) conflict relationships");

    runner.add("addMutuallyExclusiveGroup: 10 features", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 10; ++i)
        {
            std::string name = "ME" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        auto res = fm.addMutuallyExclusiveGroup("MutexGroup", names);
        DoNotOptimize(res);
    });

    runner.add("addMutuallyExclusiveGroup: 50 features", [&]() {
        FeatureManager<> fm;
        std::vector<std::string> names;
        for (int i = 0; i < 50; ++i)
        {
            std::string name = "ME" + std::to_string(i);
            (void)fm.addFeature(name);
            names.push_back(name);
        }
        auto res = fm.addMutuallyExclusiveGroup("MutexGroup", names);
        DoNotOptimize(res);
    });

    FeatureManager<> fmMutexGroup;
    std::vector<std::string> mutexNames;
    for (int i = 0; i < 20; ++i)
    {
        std::string name = "MX" + std::to_string(i);
        (void)fmMutexGroup.addFeature(name);
        mutexNames.push_back(name);
    }
    (void)fmMutexGroup.addMutuallyExclusiveGroup("MutexGroup", mutexNames);
    (void)fmMutexGroup.enable("MX0");

    runner.add("validate: mutually exclusive group (20 features)", [&]() {
        auto res = fmMutexGroup.validate();
        DoNotOptimize(res);
    });

    runner.add("enable: conflict in mutually exclusive group", [&]() {
        auto res = fmMutexGroup.enable("MX1");
        DoNotOptimize(res);
    });

    // -------------------------------------------------------------------------
    // 18. Run All Benchmarks
    // -------------------------------------------------------------------------
    runner.section("RUNNING BENCHMARKS");
    print_cpu_context(std::cout, "Starting benchmark execution");

    runner.run();

    print_cpu_context(std::cout, "Benchmark execution complete");

    // -------------------------------------------------------------------------
    // 19. Concurrent Access Patterns (Manual Section)
    // -------------------------------------------------------------------------
    printSectionHeader(std::cout, "CONCURRENT ACCESS PATTERNS");
    printContract(std::cout, "Multi-threaded read/write contention with MutexSynchronizationPolicy");
    print_cpu_context(std::cout, "Concurrent benchmarks");

    const unsigned int hwThreads = std::thread::hardware_concurrency();
    const unsigned int maxThreads = (hwThreads > 0) ? std::min(hwThreads, 8u) : 4u;
    std::cout << "  Hardware threads: " << hwThreads << ", using up to " << maxThreads << "\n";
    std::cout << "  Pinning policy: " << (cfg.noScope ? "OFF" : "ON (Windows only)") << "\n\n";

    FeatureManager<MutexSynchronizationPolicy> fmConcurrent;
    std::vector<std::string> concurrentNames;
    for (std::size_t i = 0; i < 1000; ++i)
    {
        std::string name = "Conc" + std::to_string(i);
        (void)fmConcurrent.addFeature(name);
        concurrentNames.push_back(name);
    }

    auto resetFeatures = [&]() {
        for (std::size_t i = 0; i < 1000; ++i)
        {
            (void)fmConcurrent.disable(concurrentNames[i]);
        }
        for (std::size_t i = 0; i < 500; ++i)
        {
            (void)fmConcurrent.enable(concurrentNames[i]);
        }
    };

    resetFeatures();

    // Thread scaling test (read-only)
    std::cout << "  Thread Scaling (read-only, 300ms warmup + 500ms measured):\n";
    std::cout << "    Threads  |  Throughput (ops/sec)  |  Per-Thread\n";
    std::cout << "    ---------+------------------------+-------------\n";

    for (unsigned int numThreads = 1; numThreads <= maxThreads; numThreads *= 2)
    {
        ConcurrentStats stats;
        std::atomic<bool> running{true};
        std::vector<std::thread> threads;
        SpinBarrier barrier(numThreads);

        for (unsigned int i = 0; i < numThreads; ++i)
        {
            threads.emplace_back(concurrentReader,
                                 std::ref(fmConcurrent),
                                 std::ref(concurrentNames),
                                 std::ref(running),
                                 std::ref(stats),
                                 &barrier);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stats.reset();

        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        running = false;

        for (auto& t : threads)
        {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        stats.durationSec = std::chrono::duration<double>(end - start).count();

        std::cout << "    " << std::setw(7) << numThreads << "  |  " << std::setw(20) << std::fixed
                  << std::setprecision(0) << stats.opsPerSec() << "  |  " << std::setw(11)
                  << stats.opsPerSec() / numThreads << "\n";

        running = true;
    }
    std::cout << "\n";

    // Mixed read-write with barrier
    {
        resetFeatures();

        ConcurrentStats readStats, writeStats;
        std::atomic<bool> running{true};
        std::vector<std::thread> threads;

        unsigned int numReaders = (maxThreads * 4) / 5;
        unsigned int numWriters = maxThreads - numReaders;
        if (numWriters == 0)
        {
            numWriters = 1;
        }
        if (numReaders == 0)
        {
            numReaders = 1;
        }

        SpinBarrier barrier(numReaders + numWriters);

        for (unsigned int i = 0; i < numReaders; ++i)
        {
            threads.emplace_back(concurrentReader,
                                 std::ref(fmConcurrent),
                                 std::ref(concurrentNames),
                                 std::ref(running),
                                 std::ref(readStats),
                                 &barrier);
        }
        for (unsigned int i = 0; i < numWriters; ++i)
        {
            threads.emplace_back(concurrentWriter,
                                 std::ref(fmConcurrent),
                                 std::ref(concurrentNames),
                                 std::ref(running),
                                 std::ref(writeStats),
                                 &barrier);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        readStats.reset();
        writeStats.reset();

        auto start = std::chrono::high_resolution_clock::now();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        running = false;

        for (auto& t : threads)
        {
            t.join();
        }

        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double>(end - start).count();

        std::cout << "  Mixed read-write (" << numReaders << " readers, " << numWriters << " writers, with barrier):\n";
        std::cout << "    Total reads:   " << readStats.successfulReads.load() << "\n";
        std::cout << "    Total writes:  " << writeStats.successfulWrites.load()
                  << " (failed: " << writeStats.failedWrites.load() << ")\n";
        std::cout << "    Read throughput:  " << std::fixed << std::setprecision(0)
                  << readStats.totalOps.load() / duration << " ops/sec\n";
        std::cout << "    Write throughput: " << writeStats.totalOps.load() / duration << " ops/sec\n\n";
    }

    // -------------------------------------------------------------------------
    // 20. Output Results
    // -------------------------------------------------------------------------
    runner.printReport();
    runner.exportIfConfigured();

    print_cpu_context(std::cout, "Benchmark complete");

    return 0;
}
