/**
 * @file test_FeatureManager.cpp
 * @brief Comprehensive unit tests for FeatureManager.h
 */
/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: test
  path: components/FeatureManager/tests/test_FeatureManager.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for FeatureManager."
  api_stability: in_work
  related:
    docs_search: "FeatureManager"
    headers:
      - include/fat_p/FeatureManager.h
      - include/fat_p/FatPTest.h
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

// test_FeatureManager.cpp
//
// Unified Test Suite for FeatureManager
// Includes:
// 1. Graph Logic & State Machine Tests
// 2. Serialization & Factory Tests
// 3. Performance Benchmarks

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "FeatureManager.h"

// ============================================================================
// SECTION 0: Global Enums & Policies (Required for Type-Safe Tests)
// ============================================================================

namespace fat_p
{

// Custom enum for testing group states
enum class NetworkState
{
    Disconnected,
    Connecting,
    Connected,
    Error
};

// EnumStringPolicy for NetworkState
template <>
struct EnumStringPolicy<NetworkState>
{
    static constexpr std::array<std::string_view, 4> names = {"Disconnected", "Connecting", "Connected", "Error"};

    static std::string_view to_string(NetworkState e)
    {
        return names[static_cast<size_t>(e)];
    }

    static NetworkState from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid NetworkState string");
        }
        return static_cast<NetworkState>(std::distance(names.begin(), it));
    }
};

enum class LogLevel
{
    Off,
    Basic,
    Verbose,
    Debug
};

// EnumStringPolicy for LogLevel
template <>
struct EnumStringPolicy<LogLevel>
{
    static constexpr std::array<std::string_view, 4> names = {"Off", "Basic", "Verbose", "Debug"};

    static std::string_view to_string(LogLevel e)
    {
        return names[static_cast<size_t>(e)];
    }

    static LogLevel from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid LogLevel string");
        }
        return static_cast<LogLevel>(std::distance(names.begin(), it));
    }
};

} // namespace fat_p

// ============================================================================
// SECTION 1: Graph Logic & Core Feature Tests
// ============================================================================

namespace fat_p::testing::logic
{

// Custom state computer for network group
NetworkState network_state_computer([[maybe_unused]] const std::set<std::string>& group_flags,
                                    size_t enabled_count,
                                    bool has_conflict,
                                    bool all_checks_pass)
{
    if (has_conflict || !all_checks_pass)
    {
        return NetworkState::Error;
    }
    if (enabled_count == 0)
    {
        return NetworkState::Disconnected;
    }
    if (enabled_count == 1)
    {
        return NetworkState::Connecting;
    }
    return NetworkState::Connected;
}

FATP_TEST_CASE(basic_operations)
{
    // Test basic flag addition
    {
        FeatureManager<> graph;
        auto res = graph.add_feature("FeatureA");
        FATP_ASSERT_TRUE(res.has_value(), "Should add flag successfully");

        auto dup_res = graph.add_feature("FeatureA");
        FATP_ASSERT_FALSE(dup_res.has_value(), "Should fail to add duplicate flag");
    }

    // Test flag enable/disable
    {
        FeatureManager<> graph;
        (void)graph.add_feature("FeatureA");

        FATP_ASSERT_FALSE(graph.is_enabled("FeatureA"), "Flag should be disabled initially");

        auto enable_res = graph.enable("FeatureA");
        FATP_ASSERT_TRUE(enable_res.has_value(), "Should enable flag");
        FATP_ASSERT_TRUE(graph.is_enabled("FeatureA"), "Flag should be enabled");

        auto disable_res = graph.disable("FeatureA");
        FATP_ASSERT_TRUE(disable_res.has_value(), "Should disable flag");
        FATP_ASSERT_FALSE(graph.is_enabled("FeatureA"), "Flag should be disabled");
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
        FATP_ASSERT_EQ(enabled.size(), 2u, "Should have 2 enabled flags");
        FATP_ASSERT_TRUE(std::find(enabled.begin(), enabled.end(), "A") != enabled.end(), "A should be enabled");
        FATP_ASSERT_TRUE(std::find(enabled.begin(), enabled.end(), "C") != enabled.end(), "C should be enabled");
    }
    return true;
}

FATP_TEST_CASE(interactions)
{
    // Test Requires interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("HighRes");
        (void)graph.add_feature("GPU");
        (void)graph.add_relationship("HighRes", FeatureRelationship::Requires, "GPU");

        // Should auto-enable GPU when enabling HighRes
        auto res = graph.enable("HighRes");
        FATP_ASSERT_TRUE(res.has_value(), "Should auto-enable GPU");
        FATP_ASSERT_TRUE(graph.is_enabled("GPU"), "GPU should be auto-enabled");
        FATP_ASSERT_TRUE(graph.is_enabled("HighRes"), "HighRes should be enabled");
    }

    // Test Conflicts interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("HighQuality");
        (void)graph.add_feature("LowLatency");
        (void)graph.add_relationship("HighQuality", FeatureRelationship::Conflicts, "LowLatency");

        (void)graph.enable("HighQuality");
        auto res = graph.enable("LowLatency");
        FATP_ASSERT_FALSE(res.has_value(), "Should fail: flags conflict");

        // Disable HighQuality, then LowLatency should work
        (void)graph.disable("HighQuality");
        res = graph.enable("LowLatency");
        FATP_ASSERT_TRUE(res.has_value(), "Should succeed: no conflict");
    }

    // Test Implies interaction (automatic propagation)
    {
        FeatureManager<> graph;
        (void)graph.add_feature("AdvancedGraphics");
        (void)graph.add_feature("BasicGraphics");
        (void)graph.add_relationship("AdvancedGraphics", FeatureRelationship::Implies, "BasicGraphics");

        (void)graph.enable("AdvancedGraphics");
        FATP_ASSERT_TRUE(graph.is_enabled("BasicGraphics"), "BasicGraphics should be auto-enabled by Implies");
    }

    // Test MutuallyExclusive interaction
    {
        FeatureManager<> graph;
        (void)graph.add_feature("ModeA");
        (void)graph.add_feature("ModeB");
        (void)graph.add_relationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");

        (void)graph.enable("ModeA");
        auto res = graph.enable("ModeB");
        FATP_ASSERT_FALSE(res.has_value(), "Should fail: mutually exclusive");
    }

    // Test self-referential prevention
    {
        FeatureManager<> graph;
        (void)graph.add_feature("SelfRef");
        auto res = graph.add_relationship("SelfRef", FeatureRelationship::Requires, "SelfRef");
        FATP_ASSERT_FALSE(res.has_value(), "Should prevent self-referential interaction");
    }
    return true;
}

FATP_TEST_CASE(validation_and_cycles)
{
    // Test custom check function
    {
        FeatureManager<> graph;
        bool check_pass = true;
        auto check = [&check_pass]() -> Expected<void, std::string> {
            if (check_pass)
            {
                return {};
            }
            return unexpected("Check failed");
        };

        (void)graph.add_feature("Checked", check);

        check_pass = true;
        auto res = graph.enable("Checked");
        FATP_ASSERT_TRUE(res.has_value(), "Should pass when check succeeds");

        (void)graph.disable("Checked");
        check_pass = false;
        res = graph.enable("Checked");
        FATP_ASSERT_FALSE(res.has_value(), "Should fail when check fails");
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
        FATP_ASSERT_FALSE(res.has_value(), "Should detect cycle and prevent enable");

        FATP_ASSERT_TRUE(res.error().find("Circular") != std::string::npos ||
                             res.error().find("depth") != std::string::npos,
                         "Error should mention cycle or depth limit");
    }

    // Test cycle path ordering (regression test: path should be in traversal order, not alphabetical)
    {
        FeatureManager<> graph;
        // Use names that would be reordered if using std::set (alphabetical)
        // Traversal order: Zebra -> Apple -> Banana -> Zebra
        // Alphabetical order would wrongly report: Apple -> Banana -> Zebra -> Zebra
        (void)graph.add_feature("Zebra");
        (void)graph.add_feature("Apple");
        (void)graph.add_feature("Banana");

        (void)graph.add_relationship("Zebra", FeatureRelationship::Requires, "Apple");
        (void)graph.add_relationship("Apple", FeatureRelationship::Requires, "Banana");
        (void)graph.add_relationship("Banana", FeatureRelationship::Requires, "Zebra");

        auto res = graph.enable("Zebra");
        FATP_ASSERT_FALSE(res.has_value(), "Should detect cycle");

        // The path should start with "Zebra" (the entry point), not "Apple" (alphabetically first)
        std::string err = res.error();
        FATP_ASSERT_TRUE(err.find("Zebra -> Apple -> Banana -> Zebra") != std::string::npos,
                         "Cycle path should be in traversal order: " + err);
    }

    // Test depth limit
    {
        FeatureManager<> graph;
        const int chain_length = 150; // Exceeds MAX_VALIDATION_DEPTH

        for (int i = 0; i < chain_length; ++i)
        {
            (void)graph.add_feature("Flag" + std::to_string(i));
        }

        for (int i = 0; i < chain_length - 1; ++i)
        {
            (void)graph.add_relationship("Flag" + std::to_string(i),
                                         FeatureRelationship::Implies,
                                         "Flag" + std::to_string(i + 1));
        }

        auto res = graph.enable("Flag0");
        FATP_ASSERT_FALSE(res.has_value(), "Should hit depth limit");
        FATP_ASSERT_TRUE(res.error().find("depth") != std::string::npos, "Error should mention depth limit");
    }
    return true;
}

FATP_TEST_CASE(groups)
{
    // Test basic group with default FeatureGroupState
    {
        FeatureManager<> graph;
        (void)graph.add_feature("LogBasic");
        (void)graph.add_feature("LogVerbose");
        (void)graph.add_feature("LogDebug");

        auto res = graph.add_group("Logging", {"LogBasic", "LogVerbose", "LogDebug"});
        FATP_ASSERT_TRUE(res.has_value(), "Should add group");

        auto state = graph.get_group_state("Logging");
        FATP_ASSERT_TRUE(state.has_value(), "Should get group state");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Inactive, "Group should be inactive");

        (void)graph.enable("LogBasic");
        state = graph.get_group_state("Logging");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Partial, "Group should be partial");

        (void)graph.enable("LogVerbose");
        (void)graph.enable("LogDebug");
        state = graph.get_group_state("Logging");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Active, "Group should be active");
    }

    // Test custom state enum
    {
        FeatureManager<> graph;
        (void)graph.add_feature("WiFi");
        (void)graph.add_feature("Bluetooth");

        auto res = graph.add_group<NetworkState>("Network", {"WiFi", "Bluetooth"}, network_state_computer);
        FATP_ASSERT_TRUE(res.has_value(), "Should add group with custom state");

        auto state = graph.get_group_state<NetworkState>("Network");
        FATP_ASSERT_TRUE(state.has_value(), "Should get custom state");
        FATP_ASSERT_TRUE(*state == NetworkState::Disconnected, "Should be disconnected");

        (void)graph.enable("WiFi");
        state = graph.get_group_state<NetworkState>("Network");
        FATP_ASSERT_TRUE(*state == NetworkState::Connecting, "Should be connecting");

        (void)graph.enable("Bluetooth");
        state = graph.get_group_state<NetworkState>("Network");
        FATP_ASSERT_TRUE(*state == NetworkState::Connected, "Should be connected");
    }

    // Test mutually exclusive group
    {
        FeatureManager<> graph;
        (void)graph.add_feature("Red");
        (void)graph.add_feature("Green");
        (void)graph.add_feature("Blue");

        auto res = graph.add_mutually_exclusive_group("Color", {"Red", "Green", "Blue"});
        FATP_ASSERT_TRUE(res.has_value(), "Should add mutually exclusive group");

        (void)graph.enable("Red");
        auto enable_res = graph.enable("Green");
        FATP_ASSERT_FALSE(enable_res.has_value(), "Should fail: mutually exclusive");
    }
    return true;
}

FATP_TEST_CASE(complex_scenario)
{
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
    FATP_ASSERT_TRUE(res.has_value(), "Should enable DX12");

    // Enable RayTracing (requires DX12, which is enabled)
    res = graph.enable("RayTracing");
    FATP_ASSERT_TRUE(res.has_value(), "Should enable RayTracing with DX12");

    // Try to enable Vulkan (should fail, mutually exclusive with DX12)
    res = graph.enable("Vulkan");
    FATP_ASSERT_FALSE(res.has_value(), "Should fail: mutually exclusive with DX12");

    // Enable HighRes (should auto-enable MSAA via Implies)
    res = graph.enable("HighRes");
    FATP_ASSERT_TRUE(res.has_value(), "Should enable HighRes");
    FATP_ASSERT_TRUE(graph.is_enabled("MSAA"), "MSAA should be auto-enabled");

    // Check group state
    auto state = graph.get_group_state("AdvancedGraphics");
    FATP_ASSERT_TRUE(*state == FeatureGroupState::Active, "All advanced features should be active");

    return true;
}

FATP_TEST_CASE(thread_safety)
{
    FeatureManager<MutexSynchronizationPolicy> graph;
    (void)graph.add_feature("SharedFlag");

    std::atomic<int> success_count{0};

    auto worker = [&]() {
        for (int i = 0; i < 100; ++i)
        {
            auto res = graph.enable("SharedFlag");
            if (res.has_value())
            {
                success_count++;
            }
            (void)graph.disable("SharedFlag");
            bool enabled = graph.is_enabled("SharedFlag");
            (void)enabled;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto& t : threads)
    {
        t.join();
    }

    FATP_ASSERT_TRUE(success_count > 0, "Should have some successful operations");
    return true;
}

FATP_TEST_CASE(observers)
{
    FeatureManager<> graph;
    (void)graph.add_feature("Observed");

    int call_count = 0;
    bool last_state = false;
    bool last_success = false;

    FeatureObserver cb = [&](const std::string& name, bool new_state, bool success) {
        (void)name;
        call_count++;
        last_state = new_state;
        last_success = success;
    };

    graph.add_observer(cb, 0);

    (void)graph.enable("Observed");
    FATP_ASSERT_EQ(call_count, 1, "Should call observer on enable");
    FATP_ASSERT_TRUE(last_state, "Should be enabled");
    FATP_ASSERT_TRUE(last_success, "Should be successful");

    (void)graph.disable("Observed");
    FATP_ASSERT_EQ(call_count, 2, "Should call observer on disable");
    FATP_ASSERT_FALSE(last_state, "Should be disabled");

    return true;
}

FATP_TEST_CASE(dot_export)
{
    FeatureManager<> graph;
    (void)graph.add_feature("NodeA");
    (void)graph.add_feature("NodeB");
    (void)graph.add_feature("NodeC");
    (void)graph.add_relationship("NodeA", FeatureRelationship::Requires, "NodeB");
    (void)graph.add_relationship("NodeA", FeatureRelationship::Conflicts, "NodeC");
    (void)graph.add_group("TestGroup", {"NodeA", "NodeB"});

    std::string dot = graph.to_dot();
    FATP_ASSERT_FALSE(dot.empty(), "DOT output should not be empty");
    FATP_ASSERT_TRUE(dot.find("digraph") != std::string::npos, "Should contain digraph declaration");
    FATP_ASSERT_TRUE(dot.find("NodeA") != std::string::npos, "Should contain NodeA");
    FATP_ASSERT_TRUE(dot.find("Requires") != std::string::npos, "Should contain interaction label");

    return true;
}

FATP_TEST_CASE(batch_disable)
{
    // Basic batch disable
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");
        (void)manager.add_feature("B");
        (void)manager.add_feature("C");

        (void)manager.enable("A");
        (void)manager.enable("B");
        (void)manager.enable("C");

        auto result = manager.batch_disable({"A", "B"});
        FATP_ASSERT_TRUE(result.has_value(), "Should disable A and B");
        FATP_ASSERT_FALSE(manager.is_enabled("A"), "A should be disabled");
        FATP_ASSERT_FALSE(manager.is_enabled("B"), "B should be disabled");
        FATP_ASSERT_TRUE(manager.is_enabled("C"), "C should still be enabled");
    }

    // Batch disable with dependency violation (rollback)
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Base");
        (void)manager.add_feature("Dependent");
        (void)manager.add_relationship("Dependent", FeatureRelationship::Requires, "Base");

        (void)manager.enable("Dependent"); // Also enables Base

        auto result = manager.batch_disable({"Base"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Dependent requires Base");
        FATP_ASSERT_TRUE(manager.is_enabled("Base"), "Base should still be enabled (rollback)");
        FATP_ASSERT_TRUE(manager.is_enabled("Dependent"), "Dependent should still be enabled");
    }

    // Non-existent feature
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        auto result = manager.batch_disable({"A", "NonExistent"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail for non-existent feature");
    }

    // Empty batch
    {
        FeatureManager<> manager;
        auto result = manager.batch_disable({});
        FATP_ASSERT_TRUE(result.has_value(), "Empty batch should succeed");
    }

    // Batch disable with duplicate entries must still rollback correctly
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Base");
        (void)manager.add_feature("Dependent");
        (void)manager.add_relationship("Dependent", FeatureRelationship::Requires, "Base");

        (void)manager.enable("Dependent"); // Also enables Base
        FATP_ASSERT_TRUE(manager.is_enabled("Base"), "Base should be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("Dependent"), "Dependent should be enabled");

        // Try to disable Base twice in same batch - should fail and rollback correctly
        auto result = manager.batch_disable({"Base", "Base"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Dependent requires Base (with duplicates)");
        FATP_ASSERT_TRUE(manager.is_enabled("Base"), "Base should still be enabled after rollback");
        FATP_ASSERT_TRUE(manager.is_enabled("Dependent"), "Dependent should still be enabled");
    }

    return true;
}

FATP_TEST_CASE(batch_enable_rollback)
{
    // Test that implicit dependencies are rolled back on failure
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");
        (void)manager.add_feature("B");
        (void)manager.add_feature("C");
        (void)manager.add_feature("Conflict");

        // A requires B (so enabling A will also enable B)
        (void)manager.add_relationship("A", FeatureRelationship::Requires, "B");
        // C conflicts with Conflict
        (void)manager.add_relationship("C", FeatureRelationship::Conflicts, "Conflict");

        // Enable Conflict first
        (void)manager.enable("Conflict");

        // Now try to batch enable A and C
        // A will succeed (enabling B implicitly)
        // C will fail (conflicts with Conflict)
        // Both A and B should be rolled back
        auto result = manager.batch_enable({"A", "C"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail due to conflict");
        FATP_ASSERT_FALSE(manager.is_enabled("A"), "A should be rolled back");
        FATP_ASSERT_FALSE(manager.is_enabled("B"), "B (implicit) should be rolled back");
        FATP_ASSERT_FALSE(manager.is_enabled("C"), "C should not be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("Conflict"), "Conflict should remain enabled");
    }

    // Test rollback with deeper dependency chain
    {
        FeatureManager<> manager;
        (void)manager.add_feature("L1");
        (void)manager.add_feature("L2");
        (void)manager.add_feature("L3");
        (void)manager.add_feature("Fail");
        (void)manager.add_feature("Blocker");

        // L1 -> L2 -> L3 dependency chain
        (void)manager.add_relationship("L1", FeatureRelationship::Requires, "L2");
        (void)manager.add_relationship("L2", FeatureRelationship::Requires, "L3");
        // Fail conflicts with Blocker
        (void)manager.add_relationship("Fail", FeatureRelationship::Conflicts, "Blocker");

        (void)manager.enable("Blocker");

        // Try to enable L1 (will enable L2, L3) and Fail (will fail)
        auto result = manager.batch_enable({"L1", "Fail"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_FALSE(manager.is_enabled("L1"), "L1 should be rolled back");
        FATP_ASSERT_FALSE(manager.is_enabled("L2"), "L2 should be rolled back");
        FATP_ASSERT_FALSE(manager.is_enabled("L3"), "L3 should be rolled back");
    }

    return true;
}

// Test that single enable() has full transactional semantics
// (Regression test for dirty state bug where dependencies were left enabled on failure)
FATP_TEST_CASE(enable_transactional)
{
    // Test: A requires B and C, C conflicts with D (which is enabled)
    // When enable(A) fails, B should NOT be left enabled
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");
        (void)manager.add_feature("B");
        (void)manager.add_feature("C");
        (void)manager.add_feature("D");

        (void)manager.add_relationship("A", FeatureRelationship::Requires, "B");
        (void)manager.add_relationship("A", FeatureRelationship::Requires, "C");
        (void)manager.add_relationship("C", FeatureRelationship::Conflicts, "D");

        // Enable D first (so C will conflict)
        (void)manager.enable("D");

        // Try to enable A - should fail because C conflicts with D
        auto result = manager.enable("A");

        FATP_ASSERT_FALSE(result.has_value(), "enable(A) should fail due to C/D conflict");
        FATP_ASSERT_FALSE(manager.is_enabled("A"), "A should not be enabled");
        FATP_ASSERT_FALSE(manager.is_enabled("B"), "B should NOT be left enabled (transactional rollback)");
        FATP_ASSERT_FALSE(manager.is_enabled("C"), "C should not be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("D"), "D should remain enabled");
    }

    // Test with deeper chain: X requires Y requires Z, Z conflicts with Blocker
    {
        FeatureManager<> manager;
        (void)manager.add_feature("X");
        (void)manager.add_feature("Y");
        (void)manager.add_feature("Z");
        (void)manager.add_feature("Blocker");

        (void)manager.add_relationship("X", FeatureRelationship::Requires, "Y");
        (void)manager.add_relationship("Y", FeatureRelationship::Requires, "Z");
        (void)manager.add_relationship("Z", FeatureRelationship::Conflicts, "Blocker");

        (void)manager.enable("Blocker");

        auto result = manager.enable("X");

        FATP_ASSERT_FALSE(result.has_value(), "enable(X) should fail");
        FATP_ASSERT_FALSE(manager.is_enabled("X"), "X should not be enabled");
        FATP_ASSERT_FALSE(manager.is_enabled("Y"), "Y should be rolled back");
        FATP_ASSERT_FALSE(manager.is_enabled("Z"), "Z should not be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("Blocker"), "Blocker should remain");
    }

    // Test successful enable still works
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");
        (void)manager.add_feature("B");
        (void)manager.add_relationship("A", FeatureRelationship::Requires, "B");

        auto result = manager.enable("A");

        FATP_ASSERT_TRUE(result.has_value(), "enable(A) should succeed");
        FATP_ASSERT_TRUE(manager.is_enabled("A"), "A should be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("B"), "B should be enabled (dependency)");
    }

    return true;
}

FATP_TEST_CASE(remove_observer)
{
    // Test basic add and remove
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int call_count = 0;
        ObserverId id = manager.add_observer([&](auto, auto, auto) {
            ++call_count;
        });

        (void)manager.enable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer should be called once");

        bool removed = manager.remove_observer(id);
        FATP_ASSERT_TRUE(removed, "Should remove existing observer");

        (void)manager.disable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer should not be called after removal");
    }

    // Test remove non-existent
    {
        FeatureManager<> manager;
        bool removed = manager.remove_observer(999);
        FATP_ASSERT_FALSE(removed, "Should return false for non-existent ID");
    }

    // Test multiple observers with removal
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int count1 = 0, count2 = 0;
        ObserverId id1 = manager.add_observer([&](auto, auto, auto) {
            ++count1;
        });
        ObserverId id2 = manager.add_observer([&](auto, auto, auto) {
            ++count2;
        });

        (void)manager.enable("A");
        FATP_ASSERT_EQ(count1, 1, "Observer 1 called");
        FATP_ASSERT_EQ(count2, 1, "Observer 2 called");

        (void)manager.remove_observer(id1);
        (void)manager.disable("A");

        FATP_ASSERT_EQ(count1, 1, "Observer 1 not called after removal");
        FATP_ASSERT_EQ(count2, 2, "Observer 2 still called");

        (void)manager.remove_observer(id2);
    }

    // Test clear_observers
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int count = 0;
        (void)manager.add_observer([&](auto, auto, auto) {
            ++count;
        });
        (void)manager.add_observer([&](auto, auto, auto) {
            ++count;
        });

        manager.clear_observers();

        (void)manager.enable("A");
        FATP_ASSERT_EQ(count, 0, "No observers should be called after clear");
    }

    return true;
}

FATP_TEST_CASE(scoped_observer)
{
    // Test basic RAII semantics
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int call_count = 0;
        {
            FeatureManager<>::ScopedObserver scoped(manager, [&](auto, auto, auto) {
                ++call_count;
            });

            (void)manager.enable("A");
            FATP_ASSERT_EQ(call_count, 1, "Observer called while in scope");
        }

        (void)manager.disable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer not called after scope ends");
    }

    // Test move semantics
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int call_count = 0;
        std::optional<FeatureManager<>::ScopedObserver> holder;

        {
            FeatureManager<>::ScopedObserver scoped(manager, [&](auto, auto, auto) {
                ++call_count;
            });
            holder.emplace(std::move(scoped));
        }

        (void)manager.enable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer still active after move");

        holder.reset();
        (void)manager.disable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer removed when holder destroyed");
    }

    // Test release()
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int call_count = 0;
        ObserverId released_id;
        {
            FeatureManager<>::ScopedObserver scoped(manager, [&](auto, auto, auto) {
                ++call_count;
            });
            released_id = scoped.release();
        }

        (void)manager.enable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer still active after release");

        // Manual cleanup
        (void)manager.remove_observer(released_id);
    }

    return true;
}

FATP_TEST_CASE(batch_observer)
{
    // Test batch observer receives all changed features
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Core");
        (void)manager.add_feature("Module1");
        (void)manager.add_feature("Module2");
        (void)manager.add_relationship("Module1", FeatureRelationship::Requires, "Core");
        (void)manager.add_relationship("Module2", FeatureRelationship::Requires, "Core");

        std::string requested;
        std::vector<std::string> all_changed;
        bool was_enabled = false;
        bool was_success = false;

        (void)manager.add_batch_observer([&](auto req, auto changed, auto en, auto ok) {
            requested = req;
            all_changed = changed;
            was_enabled = en;
            was_success = ok;
        });

        // Enable Module1 - should also enable Core
        (void)manager.enable("Module1");

        FATP_ASSERT_EQ(requested, "Module1", "Requested feature should be Module1");
        FATP_ASSERT_TRUE(was_enabled, "Should be enable operation");
        FATP_ASSERT_TRUE(was_success, "Should succeed");
        FATP_ASSERT_TRUE(all_changed.size() >= 2, "Should have at least 2 changed features");

        // Check that both Core and Module1 are in the changed list
        bool has_core = std::find(all_changed.begin(), all_changed.end(), "Core") != all_changed.end();
        bool has_module1 = std::find(all_changed.begin(), all_changed.end(), "Module1") != all_changed.end();
        FATP_ASSERT_TRUE(has_core, "Core should be in changed list");
        FATP_ASSERT_TRUE(has_module1, "Module1 should be in changed list");
    }

    // Test ScopedBatchObserver
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");

        int call_count = 0;
        {
            FeatureManager<>::ScopedBatchObserver scoped(manager, [&](auto, auto, auto, auto) {
                ++call_count;
            });

            (void)manager.enable("A");
            FATP_ASSERT_EQ(call_count, 1, "Batch observer called while in scope");
        }

        (void)manager.disable("A");
        FATP_ASSERT_EQ(call_count, 1, "Batch observer not called after scope ends");
    }

    return true;
}

FATP_TEST_CASE(implicit_notifications)
{
    // Test that individual observers are notified for ALL changed features
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Base1");
        (void)manager.add_feature("Base2");
        (void)manager.add_feature("Dependent");
        (void)manager.add_relationship("Dependent", FeatureRelationship::Requires, "Base1");
        (void)manager.add_relationship("Dependent", FeatureRelationship::Requires, "Base2");

        std::vector<std::string> notified_features;
        (void)manager.add_observer([&](const std::string& name, bool enabled, bool) {
            if (enabled)
            {
                notified_features.push_back(name);
            }
        });

        // Enable Dependent - should trigger notifications for Base1, Base2, and Dependent
        (void)manager.enable("Dependent");

        FATP_ASSERT_EQ(notified_features.size(), 3u, "Should notify 3 features");

        bool has_base1 =
            std::find(notified_features.begin(), notified_features.end(), "Base1") != notified_features.end();
        bool has_base2 =
            std::find(notified_features.begin(), notified_features.end(), "Base2") != notified_features.end();
        bool has_dependent =
            std::find(notified_features.begin(), notified_features.end(), "Dependent") != notified_features.end();

        FATP_ASSERT_TRUE(has_base1, "Base1 should be notified");
        FATP_ASSERT_TRUE(has_base2, "Base2 should be notified");
        FATP_ASSERT_TRUE(has_dependent, "Dependent should be notified");
    }

    // Test Implies relationships also trigger notifications
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Premium");
        (void)manager.add_feature("AllFeatures");
        (void)manager.add_relationship("Premium", FeatureRelationship::Implies, "AllFeatures");

        std::vector<std::string> notified;
        (void)manager.add_observer([&](const std::string& name, bool enabled, bool) {
            if (enabled)
            {
                notified.push_back(name);
            }
        });

        (void)manager.enable("Premium");

        FATP_ASSERT_EQ(notified.size(), 2u, "Should notify both Premium and AllFeatures");

        bool has_premium = std::find(notified.begin(), notified.end(), "Premium") != notified.end();
        bool has_all = std::find(notified.begin(), notified.end(), "AllFeatures") != notified.end();

        FATP_ASSERT_TRUE(has_premium, "Premium should be notified");
        FATP_ASSERT_TRUE(has_all, "AllFeatures should be notified");
    }

    return true;
}

FATP_TEST_CASE(batch_disable_implies)
{
    // Test that batch_disable checks Implies relationships
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Premium");
        (void)manager.add_feature("AllFeatures");
        (void)manager.add_relationship("Premium", FeatureRelationship::Implies, "AllFeatures");

        // Enable Premium (which implies AllFeatures)
        (void)manager.enable("Premium");
        FATP_ASSERT_TRUE(manager.is_enabled("Premium"), "Premium should be enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("AllFeatures"), "AllFeatures should be enabled");

        // Try to disable AllFeatures while Premium is still enabled
        auto result = manager.batch_disable({"AllFeatures"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Premium implies AllFeatures");
        FATP_ASSERT_TRUE(manager.is_enabled("AllFeatures"), "AllFeatures should remain enabled");
        FATP_ASSERT_TRUE(manager.is_enabled("Premium"), "Premium should remain enabled");
    }

    // Test that disabling the implier first allows disabling the implied
    {
        FeatureManager<> manager;
        (void)manager.add_feature("A");
        (void)manager.add_feature("B");
        (void)manager.add_relationship("A", FeatureRelationship::Implies, "B");

        (void)manager.enable("A");

        // Disable A first (the implier)
        auto result1 = manager.disable("A");
        FATP_ASSERT_TRUE(result1.has_value(), "Should succeed to disable A");

        // Now B can be disabled
        auto result2 = manager.batch_disable({"B"});
        FATP_ASSERT_TRUE(result2.has_value(), "Should succeed to disable B now");
        FATP_ASSERT_FALSE(manager.is_enabled("B"), "B should be disabled");
    }

    // Test complex Implies chain
    {
        FeatureManager<> manager;
        (void)manager.add_feature("Top");
        (void)manager.add_feature("Middle");
        (void)manager.add_feature("Bottom");
        (void)manager.add_relationship("Top", FeatureRelationship::Implies, "Middle");
        (void)manager.add_relationship("Middle", FeatureRelationship::Implies, "Bottom");

        (void)manager.enable("Top");

        // Cannot disable Bottom while Middle is enabled (which implies it)
        auto r1 = manager.batch_disable({"Bottom"});
        FATP_ASSERT_FALSE(r1.has_value(), "Cannot disable Bottom: Middle implies it");

        // Cannot disable Middle while Top is enabled (which implies it)
        auto r2 = manager.batch_disable({"Middle"});
        FATP_ASSERT_FALSE(r2.has_value(), "Cannot disable Middle: Top implies it");

        // Can disable Top
        auto r3 = manager.disable("Top");
        FATP_ASSERT_TRUE(r3.has_value(), "Can disable Top");

        // Now can disable Middle
        auto r4 = manager.batch_disable({"Middle"});
        FATP_ASSERT_TRUE(r4.has_value(), "Can now disable Middle");

        // Now can disable Bottom
        auto r5 = manager.batch_disable({"Bottom"});
        FATP_ASSERT_TRUE(r5.has_value(), "Can now disable Bottom");
    }

    return true;
}


FATP_TEST_CASE(dot_roundtrip_parses_requires_and_ignores_global_attributes)
{
    FeatureManager<> manager;
    FATP_ASSERT_TRUE(manager.add_feature("A").has_value(), "Should add A");
    FATP_ASSERT_TRUE(manager.add_feature("B").has_value(), "Should add B");
    FATP_ASSERT_TRUE(manager.add_relationship("A", FeatureRelationship::Requires, "B").has_value(),
                     "Should add Requires relationship");

    const std::string dot = manager.to_dot();
    auto parsed_res = FeatureManager<>::from_dot(dot);
    FATP_ASSERT_TRUE(parsed_res.has_value(), "from_dot should parse to_dot output");

    auto& parsed = *parsed_res;
    FATP_ASSERT_TRUE(parsed.enable("A").has_value(), "Enabling A should succeed");
    FATP_ASSERT_TRUE(parsed.is_enabled("B"), "B should be enabled via Requires relationship");

    const auto all = parsed.get_all_features();
    FATP_ASSERT_FALSE(std::find(all.begin(), all.end(), "node") != all.end(),
                      "from_dot should not create spurious 'node' feature");

    return true;
}


} // namespace fat_p::testing::logic

// ============================================================================
// SECTION 2: Serialization & Factory Tests
// ============================================================================

namespace fat_p::testing::factory
{

// --- Helpers for Module Independence Test ---
namespace module_a
{
int hardware_check_call_count = 0;
Expected<void, std::string> check_hardware()
{
    ++hardware_check_call_count;
    return {};
}
void register_checks()
{
    (void)get_feature_check_factory().registerType("module_a.hardware", []() -> FeatureCheck {
        return []() {
            return check_hardware();
        };
    });
}
} // namespace module_a

namespace module_b
{
int license_check_call_count = 0;
Expected<void, std::string> check_license()
{
    ++license_check_call_count;
    return {};
}
void register_checks()
{
    (void)get_feature_check_factory().registerType("module_b.license", []() -> FeatureCheck {
        return []() {
            return check_license();
        };
    });
}
} // namespace module_b

// --- Actual Tests ---

FATP_TEST_CASE(basic_factory_registration)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    bool registered = factory.registerType("test.simple", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return {};
        };
    });
    FATP_ASSERT_TRUE(registered, "Should register new check");

    bool registered_again = factory.registerType("test.simple", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return unexpected("No");
        };
    });
    FATP_ASSERT_FALSE(registered_again, "Should not allow duplicate registration");

    auto check_result = factory.make("test.simple");
    FATP_ASSERT_TRUE(check_result.has_value(), "Should find registered check");

    auto check = *check_result;
    auto result = check();
    FATP_ASSERT_TRUE(result.has_value(), "Check should pass");

    auto missing_result = factory.make("test.missing");
    FATP_ASSERT_FALSE(missing_result.has_value(), "Should not find non-existent check");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_serialization_roundtrip)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    [[maybe_unused]] bool r1 = factory.registerType("hardware.gpu", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return {};
        };
    });
    [[maybe_unused]] bool r2 = factory.registerType("license.valid", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return {};
        };
    });

    FeatureManager<> manager;
    (void)manager.add_feature("GPUAcceleration", "hardware.gpu");
    (void)manager.add_feature("PremiumFeature", "license.valid");
    (void)manager.add_feature("BasicFeature");
    (void)manager.add_relationship("PremiumFeature", FeatureRelationship::Requires, "BasicFeature");

    (void)manager.enable("BasicFeature");
    (void)manager.enable("GPUAcceleration");

    std::string json = manager.to_json();
    FATP_ASSERT_TRUE(!json.empty(), "Should produce JSON");
    FATP_ASSERT_TRUE(json.find("hardware.gpu") != std::string::npos, "Should contain check key");

    auto restored_result = FeatureManager<>::from_json(json);
    FATP_ASSERT_TRUE(restored_result.has_value(), "Should deserialize successfully");

    auto& restored = *restored_result;
    FATP_ASSERT_TRUE(restored.is_enabled("GPUAcceleration"), "GPUAcceleration should be enabled");
    FATP_ASSERT_TRUE(restored.is_enabled("BasicFeature"), "BasicFeature should be enabled");
    FATP_ASSERT_FALSE(restored.is_enabled("PremiumFeature"), "PremiumFeature should not be enabled");

    factory.clear();
    return true;
}

FATP_TEST_CASE(raii_registration)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    {
        FeatureCheckRegistration reg1("test.raii1", []() -> FeatureCheck {
            return []() -> Expected<void, std::string> {
                return {};
            };
        });
        FATP_ASSERT_TRUE(factory.hasType("test.raii1"), "Should be registered");

        FeatureManager<> manager;
        auto add_result = manager.add_feature("Feature1", "test.raii1");
        FATP_ASSERT_TRUE(add_result.has_value(), "Should add feature");
    }

    FATP_ASSERT_FALSE(factory.hasType("test.raii1"), "Should be unregistered");
    factory.clear();
    return true;
}

FATP_TEST_CASE(module_independence)
{
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
    FATP_ASSERT_EQ(module_a::hardware_check_call_count, 1, "Should call module A check");
    FATP_ASSERT_EQ(module_b::license_check_call_count, 0, "Should not call module B check");

    (void)manager.enable("LicenseFeature");
    FATP_ASSERT_EQ(module_a::hardware_check_call_count, 1, "Should not call module A check again");
    FATP_ASSERT_EQ(module_b::license_check_call_count, 1, "Should call module B check");

    factory.clear();
    return true;
}

FATP_TEST_CASE(complex_graph_serialization)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    [[maybe_unused]] bool r1 = factory.registerType("check.a", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return {};
        };
    });
    [[maybe_unused]] bool r2 = factory.registerType("check.b", []() -> FeatureCheck {
        return []() -> Expected<void, std::string> {
            return {};
        };
    });

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
    FATP_ASSERT_TRUE(restored_result.has_value(), "Should deserialize complex graph");

    auto& restored = *restored_result;
    FATP_ASSERT_TRUE(restored.is_enabled("A"), "A should be enabled");
    FATP_ASSERT_TRUE(restored.is_enabled("B"), "B should be enabled");
    FATP_ASSERT_FALSE(restored.is_enabled("C"), "C should not be enabled");

    auto enable_d = restored.enable("D");
    FATP_ASSERT_FALSE(enable_d.has_value(), "Should not enable D due to conflict");

    factory.clear();
    return true;
}


FATP_TEST_CASE(raii_duplicate_registration_does_not_unregister_original)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    {
        FeatureCheckRegistration reg1("dup_test", []() {
            return []() -> Expected<void, std::string> {
                return {};
            };
        });

        FATP_ASSERT_TRUE(factory.hasType("dup_test"), "Original registration should exist");

        {
            // Duplicate registration should fail; destructor must NOT unregister original.
            FeatureCheckRegistration reg2("dup_test", []() {
                return []() -> Expected<void, std::string> {
                    return unexpected("fail");
                };
            });
        }

        FATP_ASSERT_TRUE(factory.hasType("dup_test"),
                         "Original registration must remain after failed duplicate registration");
    }

    FATP_ASSERT_FALSE(factory.hasType("dup_test"),
                      "Original registration should be removed once owning registration is destroyed");
    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_unknown_check_key_fails)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": {
                "enabled": true,
                "check_key": "does_not_exist"
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "from_json should fail when check_key is unknown");
    FATP_ASSERT_TRUE(fm_res.error().find("not found") != std::string::npos, "Error should mention missing check_key");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_group_with_missing_feature_fails)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": { "enabled": false }
        },
        "groups": {
            "G": ["A", "MISSING"]
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "from_json should fail when a group references a missing feature");
    FATP_ASSERT_TRUE(fm_res.error().find("references missing feature") != std::string::npos,
                     "Error should mention missing group feature");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_relationship_to_missing_feature_fails)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": {
                "enabled": false,
                "Requires": ["B"]
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "from_json should fail when relationships reference missing target features");
    FATP_ASSERT_TRUE(fm_res.error().find("to missing feature") != std::string::npos,
                     "Error should mention missing relationship target");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_symmetrizes_conflicts)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    // JSON with asymmetric conflict: only A->B, not B->A
    const std::string json = R"({
        "features": {
            "A": {
                "enabled": false,
                "Conflicts": ["B"]
            },
            "B": {
                "enabled": false
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_TRUE(fm_res.has_value(), "from_json should succeed with asymmetric conflicts");

    auto& fm = *fm_res;

    // Enable A first
    FATP_ASSERT_TRUE(fm.enable("A").has_value(), "Should enable A");

    // Try to enable B - should fail because from_json symmetrized the conflict
    auto b_res = fm.enable("B");
    FATP_ASSERT_FALSE(b_res.has_value(), "B should conflict with A after symmetrization");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_symmetrizes_mutually_exclusive)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    // JSON with asymmetric mutual exclusion: only A->B
    const std::string json = R"({
        "features": {
            "A": {
                "enabled": false,
                "MutuallyExclusive": ["B"]
            },
            "B": {
                "enabled": false
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_TRUE(fm_res.has_value(), "from_json should succeed");

    auto& fm = *fm_res;

    // Enable A first
    FATP_ASSERT_TRUE(fm.enable("A").has_value(), "Should enable A");

    // Try to enable B - should fail because from_json symmetrized
    auto b_res = fm.enable("B");
    FATP_ASSERT_FALSE(b_res.has_value(), "B should be mutually exclusive with A after symmetrization");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_detects_cycles)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    // JSON with circular dependency: A requires B, B requires A
    const std::string json = R"({
        "features": {
            "A": {
                "enabled": false,
                "Requires": ["B"]
            },
            "B": {
                "enabled": false,
                "Requires": ["A"]
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "from_json should fail when graph contains cycles");
    FATP_ASSERT_TRUE(fm_res.error().find("Circular") != std::string::npos ||
                         fm_res.error().find("cycle") != std::string::npos ||
                         fm_res.error().find("validation") != std::string::npos,
                     "Error should mention cycle or validation failure");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_validates_enabled_state_invariants)
{
    auto& factory = get_feature_check_factory();
    factory.clear();

    // JSON with invalid state: A enabled but required B is disabled
    const std::string json = R"({
        "features": {
            "A": {
                "enabled": true,
                "Requires": ["B"]
            },
            "B": {
                "enabled": false
            }
        }
    })";

    auto fm_res = FeatureManager<>::from_json(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "from_json should fail when enabled state violates Requires");
    FATP_ASSERT_TRUE(fm_res.error().find("validation") != std::string::npos ||
                         fm_res.error().find("requires") != std::string::npos,
                     "Error should mention validation failure");

    factory.clear();
    return true;
}


} // namespace fat_p::testing::factory

// ============================================================================
// SECTION 3: Benchmarks
// ============================================================================

namespace fat_p::testing::bench
{
} // namespace fat_p::testing::bench

// ============================================================================
// MAIN: Unified Test Runner
// ============================================================================

namespace fat_p::testing
{


void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_FeatureManager()
{
    // Configuration
    get_test_config().verbose = true;
    get_test_config().colored_output = true;

    TestRunner runner;
    bool all_passed = true;

    // ------------------------------------------------------------------------
    // 1. Run Logic Tests
    // ------------------------------------------------------------------------
    FATP_PRINT_HEADER(LOGIC LAYER TESTS);

    FATP_RUN_TEST_NS(runner, logic, basic_operations);
    FATP_RUN_TEST_NS(runner, logic, interactions);
    FATP_RUN_TEST_NS(runner, logic, validation_and_cycles);
    FATP_RUN_TEST_NS(runner, logic, groups);
    FATP_RUN_TEST_NS(runner, logic, complex_scenario);
    FATP_RUN_TEST_NS(runner, logic, thread_safety);
    FATP_RUN_TEST_NS(runner, logic, observers);
    FATP_RUN_TEST_NS(runner, logic, dot_export);
    FATP_RUN_TEST_NS(runner, logic, dot_roundtrip_parses_requires_and_ignores_global_attributes);
    FATP_RUN_TEST_NS(runner, logic, batch_disable);
    FATP_RUN_TEST_NS(runner, logic, batch_enable_rollback);
    FATP_RUN_TEST_NS(runner, logic, enable_transactional);
    FATP_RUN_TEST_NS(runner, logic, remove_observer);
    FATP_RUN_TEST_NS(runner, logic, scoped_observer);
    FATP_RUN_TEST_NS(runner, logic, batch_observer);
    FATP_RUN_TEST_NS(runner, logic, implicit_notifications);
    FATP_RUN_TEST_NS(runner, logic, batch_disable_implies);

    if (runner.print_summary() > 0)
    {
        all_passed = false;
    }
    runner.clear();

    // ------------------------------------------------------------------------
    // 2. Run Factory/Serialization Tests
    // ------------------------------------------------------------------------
    FATP_PRINT_HEADER(FACTORY & SERIALIZATION TESTS);

    FATP_RUN_TEST_NS(runner, factory, basic_factory_registration);
    FATP_RUN_TEST_NS(runner, factory, json_serialization_roundtrip);
    FATP_RUN_TEST_NS(runner, factory, raii_registration);
    FATP_RUN_TEST_NS(runner, factory, raii_duplicate_registration_does_not_unregister_original);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_unknown_check_key_fails);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_group_with_missing_feature_fails);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_relationship_to_missing_feature_fails);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_symmetrizes_conflicts);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_symmetrizes_mutually_exclusive);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_detects_cycles);
    FATP_RUN_TEST_NS(runner, factory, json_deserialize_validates_enabled_state_invariants);
    FATP_RUN_TEST_NS(runner, factory, module_independence);
    FATP_RUN_TEST_NS(runner, factory, complex_graph_serialization);

    if (runner.print_summary() > 0)
    {
        all_passed = false;
    }

    // ------------------------------------------------------------------------
    // 3. Run Benchmarks
    // ------------------------------------------------------------------------
    if (all_passed)
    {
        FATP_PRINT_HEADER(PERFORMANCE BENCHMARKS);
        std::cout << fat_p::testing::colors::yellow() << "Note: Benchmarks include outliers and P99 stats."
                  << fat_p::testing::colors::reset() << "\n\n";

                std::cout << "\n";
                std::cout << "\n";
                std::cout << "\n";
            }
    else
    {
        std::cout << fat_p::testing::colors::red() << "\nSkipping benchmarks due to test failures."
                  << fat_p::testing::colors::reset() << "\n";
    }

    return all_passed;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_FeatureManager() ? 0 : 1;
}
#endif
