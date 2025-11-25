// test_FeatureManager.cpp
//
// Unified Test Suite for FeatureManager
// Includes:
// 1. Graph Logic & State Machine Tests
// 2. Serialization & Factory Tests
// 3. Performance Benchmarks

#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <algorithm>
#include <memory>

#include "FeatureManager.h"
#include "FatPTest.h"

// ============================================================================
// SECTION 0: Global Enums & Policies (Required for Type-Safe Tests)
// ============================================================================

namespace fat_p {

// Custom enum for testing group states
enum class NetworkState {
    Disconnected,
    Connecting,
    Connected,
    Error
};

// EnumStringPolicy for NetworkState
template<>
struct EnumStringPolicy<NetworkState> {
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {
        "Disconnected", "Connecting", "Connected", "Error"
    };
    
    static std::string_view to_string(NetworkState e) {
        return names[static_cast<size_t>(e)];
    }
    
    static NetworkState from_string(std::string_view str) {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end()) {
            throw std::invalid_argument("Invalid NetworkState string");
        }
        return static_cast<NetworkState>(std::distance(names.begin(), it));
    }
};

enum class LogLevel {
    Off,
    Basic,
    Verbose,
    Debug
};

// EnumStringPolicy for LogLevel
template<>
struct EnumStringPolicy<LogLevel> {
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {
        "Off", "Basic", "Verbose", "Debug"
    };
    
    static std::string_view to_string(LogLevel e) {
        return names[static_cast<size_t>(e)];
    }
    
    static LogLevel from_string(std::string_view str) {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end()) {
            throw std::invalid_argument("Invalid LogLevel string");
        }
        return static_cast<LogLevel>(std::distance(names.begin(), it));
    }
};

} // namespace fat_p

// ============================================================================
// SECTION 1: Graph Logic & Core Feature Tests
// ============================================================================

namespace fat_p::testing::logic {

// Custom state computer for network group
NetworkState network_state_computer(const std::set<std::string>& group_flags,
                                    size_t enabled_count,
                                    bool has_conflict,
                                    bool all_checks_pass) {
    if (has_conflict || !all_checks_pass) return NetworkState::Error;
    if (enabled_count == 0) return NetworkState::Disconnected;
    if (enabled_count == 1) return NetworkState::Connecting;
    return NetworkState::Connected;
}

bool test_basic_operations() {
    // Test basic flag addition
    {
        FeatureManager<> graph;
        auto res = graph.add_feature("FeatureA");
        ASSERT_TRUE(res.has_value(), "Should add flag successfully");
        
        auto dup_res = graph.add_feature("FeatureA");
        ASSERT_FALSE(dup_res.has_value(), "Should fail to add duplicate flag");
    }

    // Test flag enable/disable
    {
        FeatureManager<> graph;
        (void)graph.add_feature("FeatureA");
        
        ASSERT_FALSE(graph.is_enabled("FeatureA"), "Flag should be disabled initially");
        
        auto enable_res = graph.enable("FeatureA");
        ASSERT_TRUE(enable_res.has_value(), "Should enable flag");
        ASSERT_TRUE(graph.is_enabled("FeatureA"), "Flag should be enabled");
        
        auto disable_res = graph.disable("FeatureA");
        ASSERT_TRUE(disable_res.has_value(), "Should disable flag");
        ASSERT_FALSE(graph.is_enabled("FeatureA"), "Flag should be disabled");
    }

    // Test get_enabled
    {
        FeatureManager<> graph;
        (void)graph.add_feature("A");
        (void)graph.add_feature("B");
        (void)graph.add_feature("C");
        
        (void)graph.enable("A");
        (void)graph.enable("C");
        
        auto enabled = graph.get_enabled();
        ASSERT_EQ(enabled.size(), 2u, "Should have 2 enabled flags");
        ASSERT_TRUE(std::find(enabled.begin(), enabled.end(), "A") != enabled.end(), "A should be enabled");
        ASSERT_TRUE(std::find(enabled.begin(), enabled.end(), "C") != enabled.end(), "C should be enabled");
    }
    return true;
}

bool test_interactions() {
    // Test Requires interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("HighRes");
        (void)graph.add_feature("GPU");
        (void)graph.add_relationship("HighRes", FeatureRelationship::Requires, "GPU");
        
        // Should auto-enable GPU when enabling HighRes
        auto res = graph.enable("HighRes");
        ASSERT_TRUE(res.has_value(), "Should auto-enable GPU");
        ASSERT_TRUE(graph.is_enabled("GPU"), "GPU should be auto-enabled");
        ASSERT_TRUE(graph.is_enabled("HighRes"), "HighRes should be enabled");
    }

    // Test Conflicts interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("HighQuality");
        (void)graph.add_feature("LowLatency");
        (void)graph.add_relationship("HighQuality", FeatureRelationship::Conflicts, "LowLatency");
        
        (void)graph.enable("HighQuality");
        auto res = graph.enable("LowLatency");
        ASSERT_FALSE(res.has_value(), "Should fail: flags conflict");
        
        // Disable HighQuality, then LowLatency should work
        (void)graph.disable("HighQuality");
        res = graph.enable("LowLatency");
        ASSERT_TRUE(res.has_value(), "Should succeed: no conflict");
    }

    // Test Implies interaction (automatic propagation)
    {
        FeatureManager<> graph;
        (void)graph.add_feature("AdvancedGraphics");
        (void)graph.add_feature("BasicGraphics");
        (void)graph.add_relationship("AdvancedGraphics", FeatureRelationship::Implies, "BasicGraphics");
        
        (void)graph.enable("AdvancedGraphics");
        ASSERT_TRUE(graph.is_enabled("BasicGraphics"), "BasicGraphics should be auto-enabled by Implies");
    }

    // Test MutuallyExclusive interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("ModeA");
        (void)graph.add_feature("ModeB");
        (void)graph.add_relationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");
        
        (void)graph.enable("ModeA");
        auto res = graph.enable("ModeB");
        ASSERT_FALSE(res.has_value(), "Should fail: mutually exclusive");
    }

    // Test self-referential prevention
    {
        FeatureManager<> graph;
        (void)graph.add_feature("SelfRef");
        auto res = graph.add_relationship("SelfRef", FeatureRelationship::Requires, "SelfRef");
        ASSERT_FALSE(res.has_value(), "Should prevent self-referential interaction");
    }
    return true;
}

bool test_validation_and_cycles() {
    // Test custom check function
    {
        FeatureManager<> graph;
        bool check_pass = true;
        auto check = [&check_pass]() -> Expected<void, std::string> {
            if (check_pass) return {};
            return unexpected("Check failed");
        };
        
        (void)graph.add_feature("Checked", check);
        
        check_pass = true;
        auto res = graph.enable("Checked");
        ASSERT_TRUE(res.has_value(), "Should pass when check succeeds");
        
        (void)graph.disable("Checked");
        check_pass = false;
        res = graph.enable("Checked");
        ASSERT_FALSE(res.has_value(), "Should fail when check fails");
    }

    // Test cycle detection
    {
        FeatureManager<> graph;
        (void)graph.add_feature("A");
        (void)graph.add_feature("B");
        (void)graph.add_feature("C");
        
        // Create circular dependency: A->B->C->A
        (void)graph.add_relationship("A", FeatureRelationship::Implies, "B");
        (void)graph.add_relationship("B", FeatureRelationship::Implies, "C");
        (void)graph.add_relationship("C", FeatureRelationship::Implies, "A");
        
        // Attempt to enable A should fail due to cycle detection
        auto res = graph.enable("A");
        ASSERT_FALSE(res.has_value(), "Should detect cycle and prevent enable");
        
        ASSERT_TRUE(res.error().find("Circular") != std::string::npos || 
                    res.error().find("depth") != std::string::npos, 
                    "Error should mention cycle or depth limit");
    }

    // Test depth limit
    {
        FeatureManager<> graph;
        const int chain_length = 150; // Exceeds MAX_VALIDATION_DEPTH
        
        for (int i = 0; i < chain_length; ++i) {
            (void)graph.add_feature("Flag" + std::to_string(i));
        }
        
        for (int i = 0; i < chain_length - 1; ++i) {
            (void)graph.add_relationship("Flag" + std::to_string(i), 
                                     FeatureRelationship::Implies, 
                                     "Flag" + std::to_string(i + 1));
        }
        
        auto res = graph.enable("Flag0");
        ASSERT_FALSE(res.has_value(), "Should hit depth limit");
        ASSERT_TRUE(res.error().find("depth") != std::string::npos, 
                    "Error should mention depth limit");
    }
    return true;
}

bool test_groups() {
    // Test basic group with default FeatureGroupState
    {
        FeatureManager<> graph;
        (void)graph.add_feature("LogBasic");
        (void)graph.add_feature("LogVerbose");
        (void)graph.add_feature("LogDebug");
        
        auto res = graph.add_group("Logging", {"LogBasic", "LogVerbose", "LogDebug"});
        ASSERT_TRUE(res.has_value(), "Should add group");
        
        auto state = graph.get_group_state("Logging");
        ASSERT_TRUE(state.has_value(), "Should get group state");
        ASSERT_TRUE(*state == FeatureGroupState::Inactive, "Group should be inactive");
        
        (void)graph.enable("LogBasic");
        state = graph.get_group_state("Logging");
        ASSERT_TRUE(*state == FeatureGroupState::Partial, "Group should be partial");
        
        (void)graph.enable("LogVerbose");
        (void)graph.enable("LogDebug");
        state = graph.get_group_state("Logging");
        ASSERT_TRUE(*state == FeatureGroupState::Active, "Group should be active");
    }

    // Test custom state enum
    {
        FeatureManager<> graph;
        (void)graph.add_feature("WiFi");
        (void)graph.add_feature("Bluetooth");
        
        auto res = graph.add_group<NetworkState>("Network", {"WiFi", "Bluetooth"}, network_state_computer);
        ASSERT_TRUE(res.has_value(), "Should add group with custom state");
        
        auto state = graph.get_group_state<NetworkState>("Network");
        ASSERT_TRUE(state.has_value(), "Should get custom state");
        ASSERT_TRUE(*state == NetworkState::Disconnected, "Should be disconnected");
        
        (void)graph.enable("WiFi");
        state = graph.get_group_state<NetworkState>("Network");
        ASSERT_TRUE(*state == NetworkState::Connecting, "Should be connecting");
        
        (void)graph.enable("Bluetooth");
        state = graph.get_group_state<NetworkState>("Network");
        ASSERT_TRUE(*state == NetworkState::Connected, "Should be connected");
    }

    // Test mutually exclusive group
    {
        FeatureManager<> graph;
        (void)graph.add_feature("Red");
        (void)graph.add_feature("Green");
        (void)graph.add_feature("Blue");
        
        auto res = graph.add_mutually_exclusive_group("Color", {"Red", "Green", "Blue"});
        ASSERT_TRUE(res.has_value(), "Should add mutually exclusive group");
        
        (void)graph.enable("Red");
        auto enable_res = graph.enable("Green");
        ASSERT_FALSE(enable_res.has_value(), "Should fail: mutually exclusive");
    }
    return true;
}

bool test_complex_scenario() {
    // Test realistic game graphics configuration
    FeatureManager<> graph;
    
    // Add flags
    (void)graph.add_feature("DX12");
    (void)graph.add_feature("Vulkan");
    (void)graph.add_feature("OpenGL");
    (void)graph.add_feature("HighRes");
    (void)graph.add_feature("MSAA");
    (void)graph.add_feature("RayTracing");
    (void)graph.add_feature("VSync");
    
    // Mutually exclusive rendering backends
    (void)graph.add_mutually_exclusive_group("RenderBackend", {"DX12", "Vulkan", "OpenGL"});
    
    // Dependencies
    (void)graph.add_relationship("RayTracing", FeatureRelationship::Requires, "DX12");
    (void)graph.add_relationship("HighRes", FeatureRelationship::Implies, "MSAA");
    
    // Group for advanced features
    (void)graph.add_group("AdvancedGraphics", {"HighRes", "MSAA", "RayTracing"});
    
    // Enable DX12
    auto res = graph.enable("DX12");
    ASSERT_TRUE(res.has_value(), "Should enable DX12");
    
    // Enable RayTracing (requires DX12, which is enabled)
    res = graph.enable("RayTracing");
    ASSERT_TRUE(res.has_value(), "Should enable RayTracing with DX12");
    
    // Try to enable Vulkan (should fail, mutually exclusive with DX12)
    res = graph.enable("Vulkan");
    ASSERT_FALSE(res.has_value(), "Should fail: mutually exclusive with DX12");
    
    // Enable HighRes (should auto-enable MSAA via Implies)
    res = graph.enable("HighRes");
    ASSERT_TRUE(res.has_value(), "Should enable HighRes");
    ASSERT_TRUE(graph.is_enabled("MSAA"), "MSAA should be auto-enabled");
    
    // Check group state
    auto state = graph.get_group_state("AdvancedGraphics");
    ASSERT_TRUE(*state == FeatureGroupState::Active, "All advanced features should be active");

    return true;
}

bool test_thread_safety() {
    FeatureManager<MutexSynchronizationPolicy> graph;
    (void)graph.add_feature("SharedFlag");
    
    std::atomic<int> success_count{0};
    
    auto worker = [&]() {
        for (int i = 0; i < 100; ++i) {
            auto res = graph.enable("SharedFlag");
            if (res.has_value()) {
                success_count++;
            }
            (void)graph.disable("SharedFlag");
            bool enabled = graph.is_enabled("SharedFlag");
            (void)enabled;
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    ASSERT_TRUE(success_count > 0, "Should have some successful operations");
    return true;
}

bool test_observers() {
    FeatureManager<> graph;
    (void)graph.add_feature("Observed");
    
    int call_count = 0;
    bool last_state = false;
    bool last_success = false;
    
    FeatureObserver cb = [&](const std::string& name, bool new_state, bool success) {
        call_count++;
        last_state = new_state;
        last_success = success;
    };
    
    (void)graph.add_observer(cb, 0);
    
    (void)graph.enable("Observed");
    ASSERT_EQ(call_count, 1, "Should call observer on enable");
    ASSERT_TRUE(last_state, "Should be enabled");
    ASSERT_TRUE(last_success, "Should be successful");
    
    (void)graph.disable("Observed");
    ASSERT_EQ(call_count, 2, "Should call observer on disable");
    ASSERT_FALSE(last_state, "Should be disabled");

    return true;
}

bool test_dot_export() {
    FeatureManager<> graph;
    (void)graph.add_feature("NodeA");
    (void)graph.add_feature("NodeB");
    (void)graph.add_feature("NodeC");
    (void)graph.add_relationship("NodeA", FeatureRelationship::Requires, "NodeB");
    (void)graph.add_relationship("NodeA", FeatureRelationship::Conflicts, "NodeC");
    (void)graph.add_group("TestGroup", {"NodeA", "NodeB"});
    
    std::string dot = graph.to_dot();
    ASSERT_FALSE(dot.empty(), "DOT output should not be empty");
    ASSERT_TRUE(dot.find("digraph") != std::string::npos, "Should contain digraph declaration");
    ASSERT_TRUE(dot.find("NodeA") != std::string::npos, "Should contain NodeA");
    ASSERT_TRUE(dot.find("Requires") != std::string::npos, "Should contain interaction label");
    
    return true;
}

} // namespace fat_p::testing::logic

// ============================================================================
// SECTION 2: Serialization & Factory Tests
// ============================================================================

namespace fat_p::testing::factory {

// --- Helpers for Module Independence Test ---
namespace module_a {
    int hardware_check_call_count = 0;
    Expected<void, std::string> check_hardware() {
        ++hardware_check_call_count;
        return {};
    }
    void register_checks() {
        get_feature_check_factory().registerType("module_a.hardware", []() -> FeatureCheck {
            return []() { return check_hardware(); };
        });
    }
}

namespace module_b {
    int license_check_call_count = 0;
    Expected<void, std::string> check_license() {
        ++license_check_call_count;
        return {};
    }
    void register_checks() {
        get_feature_check_factory().registerType("module_b.license", []() -> FeatureCheck {
            return []() { return check_license(); };
        });
    }
}

// --- Actual Tests ---

bool test_basic_factory_registration() {
    auto& factory = get_feature_check_factory();
    factory.clear();
    
    bool registered = factory.registerType("test.simple", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> { return {}; };
    });
    ASSERT_TRUE(registered, "Should register new check");
    
    bool registered_again = factory.registerType("test.simple", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> { return unexpected("No"); };
    });
    ASSERT_FALSE(registered_again, "Should not allow duplicate registration");
    
    auto check_result = factory.make("test.simple");
    ASSERT_TRUE(check_result.has_value(), "Should find registered check");
    
    auto check = *check_result;
    auto result = check();
    ASSERT_TRUE(result.has_value(), "Check should pass");
    
    auto missing_result = factory.make("test.missing");
    ASSERT_FALSE(missing_result.has_value(), "Should not find non-existent check");
    
    factory.clear();
    return true;
}

bool test_json_serialization_roundtrip() {
    auto& factory = get_feature_check_factory();
    factory.clear();
    
    [[maybe_unused]] bool r1 = factory.registerType("hardware.gpu", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> { return {}; };
    });
    [[maybe_unused]] bool r2 = factory.registerType("license.valid", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> { return {}; };
    });
    
    FeatureManager<> manager;
    (void)manager.add_feature("GPUAcceleration", "hardware.gpu");
    (void)manager.add_feature("PremiumFeature", "license.valid");
    (void)manager.add_feature("BasicFeature");
    (void)manager.add_relationship("PremiumFeature", FeatureRelationship::Requires, "BasicFeature");
    
    (void)manager.enable("BasicFeature");
    (void)manager.enable("GPUAcceleration");
    
    std::string json = manager.to_json();
    ASSERT_TRUE(!json.empty(), "Should produce JSON");
    ASSERT_TRUE(json.find("hardware.gpu") != std::string::npos, "Should contain check key");
    
    auto restored_result = FeatureManager<>::from_json(json);
    ASSERT_TRUE(restored_result.has_value(), "Should deserialize successfully");
    
    auto& restored = *restored_result;
    ASSERT_TRUE(restored.is_enabled("GPUAcceleration"), "GPUAcceleration should be enabled");
    ASSERT_TRUE(restored.is_enabled("BasicFeature"), "BasicFeature should be enabled");
    ASSERT_FALSE(restored.is_enabled("PremiumFeature"), "PremiumFeature should not be enabled");
    
    factory.clear();
    return true;
}

bool test_raii_registration() {
    auto& factory = get_feature_check_factory();
    factory.clear();
    
    {
        FeatureCheckRegistration reg1("test.raii1", []() -> FeatureCheck {
            return []() -> Expected<void, std::string> { return {}; };
        });
        ASSERT_TRUE(factory.hasType("test.raii1"), "Should be registered");
        
        FeatureManager<> manager;
        auto add_result = manager.add_feature("Feature1", "test.raii1");
        ASSERT_TRUE(add_result.has_value(), "Should add feature");
    }
    
    ASSERT_FALSE(factory.hasType("test.raii1"), "Should be unregistered");
    factory.clear();
    return true;
}

bool test_module_independence() {
    auto& factory = get_feature_check_factory();
    factory.clear();
    
    module_a::register_checks();
    module_b::register_checks();
    
    FeatureManager<> manager;
    (void)manager.add_feature("HardwareFeature", "module_a.hardware");
    (void)manager.add_feature("LicenseFeature", "module_b.license");
    
    module_a::hardware_check_call_count = 0;
    module_b::license_check_call_count = 0;
    
    (void)manager.enable("HardwareFeature");
    ASSERT_EQ(module_a::hardware_check_call_count, 1, "Should call module A check");
    ASSERT_EQ(module_b::license_check_call_count, 0, "Should not call module B check");
    
    (void)manager.enable("LicenseFeature");
    ASSERT_EQ(module_a::hardware_check_call_count, 1, "Should not call module A check again");
    ASSERT_EQ(module_b::license_check_call_count, 1, "Should call module B check");
    
    factory.clear();
    return true;
}

bool test_complex_graph_serialization() {
    auto& factory = get_feature_check_factory();
    factory.clear();
    
    [[maybe_unused]] bool r1 = factory.registerType("check.a", []() -> FeatureCheck { return []() -> Expected<void, std::string> { return {}; }; });
    [[maybe_unused]] bool r2 = factory.registerType("check.b", []() -> FeatureCheck { return []() -> Expected<void, std::string> { return {}; }; });
    
    FeatureManager<> manager;
    (void)manager.add_feature("A", "check.a");
    (void)manager.add_feature("B", "check.b");
    (void)manager.add_feature("C");
    (void)manager.add_feature("D");
    
    (void)manager.add_relationship("B", FeatureRelationship::Requires, "A");
    (void)manager.add_relationship("C", FeatureRelationship::Implies, "D");
    (void)manager.add_relationship("A", FeatureRelationship::Conflicts, "D");
    
    (void)manager.enable("A");
    (void)manager.enable("B");
    
    std::string json = manager.to_json();
    auto restored_result = FeatureManager<>::from_json(json);
    ASSERT_TRUE(restored_result.has_value(), "Should deserialize complex graph");
    
    auto& restored = *restored_result;
    ASSERT_TRUE(restored.is_enabled("A"), "A should be enabled");
    ASSERT_TRUE(restored.is_enabled("B"), "B should be enabled");
    ASSERT_FALSE(restored.is_enabled("C"), "C should not be enabled");
    
    auto enable_d = restored.enable("D");
    ASSERT_FALSE(enable_d.has_value(), "Should not enable D due to conflict");
    
    factory.clear();
    return true;
}

} // namespace fat_p::testing::factory

// ============================================================================
// SECTION 3: Benchmarks
// ============================================================================

namespace fat_p::testing::bench {

void setup_dense_graph(FeatureManager<>& manager, int count, int dependency_density_percent) {
    std::mt19937 rng(42);
    for (int i = 0; i < count; ++i) {
        manager.add_feature("F" + std::to_string(i));
    }
    std::uniform_int_distribution<int> dist(0, 100);
    for (int i = 1; i < count; ++i) {
        if (dist(rng) < dependency_density_percent) {
            std::uniform_int_distribution<int> target_dist(0, i - 1);
            int target = target_dist(rng);
            manager.add_relationship("F" + std::to_string(i), 
                                   FeatureRelationship::Requires, 
                                   "F" + std::to_string(target));
        }
    }
}

void benchmark_hot_path_lookup() {
    FeatureManager<> manager;
    int node_count = 10000;
    setup_dense_graph(manager, node_count, 10);
    manager.enable("F5000");
    
    benchmark_detailed("Hot Path: is_enabled() [Hit]", [&]() {
        bool status = manager.is_enabled("F5000");
        DoNotOptimize(status);
    }, 100000, 50);

    benchmark_detailed("Hot Path: is_enabled() [Miss]", [&]() {
        bool status = manager.is_enabled("F9999");
        DoNotOptimize(status);
    }, 100000, 50);
}

void benchmark_dependency_resolution() {
    // Case A: Shallow
    {
        FeatureManager<> manager;
        setup_dense_graph(manager, 1000, 0);
        benchmark_detailed("Write: enable() [No Dependencies]", [&]() {
            FeatureManager<> temp;
            temp.add_feature("A");
            temp.enable("A");
            DoNotOptimize(temp);
        }, 1000, 20);
    }
    // Case B: Deep Chain
    {
        FeatureManager<> deep_manager;
        for(int i=0; i<51; ++i) deep_manager.add_feature("N" + std::to_string(i));
        for(int i=0; i<50; ++i) {
            deep_manager.add_relationship("N" + std::to_string(i), 
                                        FeatureRelationship::Requires, 
                                        "N" + std::to_string(i+1));
        }
        benchmark_detailed("Write: enable() [Chain Depth 50]", [&]() {
            deep_manager.disable("N0"); 
            deep_manager.enable("N0");
        }, 10000, 20);
    }
}

void benchmark_full_validation() {
    FeatureManager<> manager;
    setup_dense_graph(manager, 1000, 5);
    benchmark_detailed("Maintenance: validate() [1k nodes]", [&]() {
        auto res = manager.validate();
        DoNotOptimize(res);
    }, 100, 10);
}

void benchmark_mutex_overhead() {
    FeatureManager<SingleThreadedPolicy> st_manager;
    st_manager.add_feature("F1");
    st_manager.enable("F1");

    FeatureManager<MutexSynchronizationPolicy> mt_manager;
    mt_manager.add_feature("F1");
    mt_manager.enable("F1");

    benchmark_compare("SingleThreaded Read", [&]() {
        bool s = st_manager.is_enabled("F1");
        DoNotOptimize(s);
    },
    "MutexLock Read", [&]() {
        bool s = mt_manager.is_enabled("F1");
        DoNotOptimize(s);
    }, 1000000);
}

} // namespace fat_p::testing::bench

// ============================================================================
// MAIN: Unified Test Runner
// ============================================================================


namespace fat_p::testing
{

    bool test_FeatureManager() {
        // Configuration
        get_test_config().verbose = true;
        get_test_config().colored_output = true;

        TestRunner runner;
        bool all_passed = true;

        // ------------------------------------------------------------------------
        // 1. Run Logic Tests
        // ------------------------------------------------------------------------
        PRINT_HEADER(LOGIC LAYER TESTS);

        runner.run_test("Basic Operations", logic::test_basic_operations);
        runner.run_test("Interactions (Requires/Conflicts)", logic::test_interactions);
        runner.run_test("Validation & Cycles", logic::test_validation_and_cycles);
        runner.run_test("Groups & States", logic::test_groups);
        runner.run_test("Complex Scenarios", logic::test_complex_scenario);
        runner.run_test("Thread Safety", logic::test_thread_safety);
        runner.run_test("Observers", logic::test_observers);
        runner.run_test("DOT Export", logic::test_dot_export);

        if (runner.print_summary() > 0) all_passed = false;
        runner.clear();

        // ------------------------------------------------------------------------
        // 2. Run Factory/Serialization Tests
        // ------------------------------------------------------------------------
        PRINT_HEADER(FACTORY & SERIALIZATION TESTS);

        runner.run_test("Factory Registration", factory::test_basic_factory_registration);
        runner.run_test("JSON Roundtrip", factory::test_json_serialization_roundtrip);
        runner.run_test("RAII Registration", factory::test_raii_registration);
        runner.run_test("Module Independence", factory::test_module_independence);
        runner.run_test("Complex Graph JSON", factory::test_complex_graph_serialization);

        if (runner.print_summary() > 0) all_passed = false;

        // ------------------------------------------------------------------------
        // 3. Run Benchmarks
        // ------------------------------------------------------------------------
        if (all_passed) {
            PRINT_HEADER(PERFORMANCE BENCHMARKS);
            std::cout << fat_p::testing::colors::yellow()
                << "Note: Benchmarks include outliers and P99 stats."
                << fat_p::testing::colors::reset() << "\n\n";

            bench::benchmark_hot_path_lookup();
            std::cout << "\n";
            bench::benchmark_dependency_resolution();
            std::cout << "\n";
            bench::benchmark_full_validation();
            std::cout << "\n";
            bench::benchmark_mutex_overhead();
        }
        else {
            std::cout << fat_p::testing::colors::red()
                << "\nSkipping benchmarks due to test failures."
                << fat_p::testing::colors::reset() << "\n";
        }

        return all_passed;
    }

} // namespace fat_p::testing