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
  namespace: [fat_p::testing::logic, fat_p::testing::factory]
  layer: Testing
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

#ifndef ENABLE_TEST_APPLICATION
#include "test_FeatureManager.h"
#endif

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

using namespace fat_p::feature;

// Custom state computer for network group
NetworkState network_state_computer([[maybe_unused]] const FlatSet<std::string>& group_flags,
                                    size_t enabledCount,
                                    bool hasConflict,
                                    bool allChecksPass)
{
    if (hasConflict || !allChecksPass)
    {
        return NetworkState::Error;
    }
    if (enabledCount == 0)
    {
        return NetworkState::Disconnected;
    }
    if (enabledCount == 1)
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
        auto res = graph.addFeature("FeatureA");
        FATP_ASSERT_TRUE(res.has_value(), "Should add flag successfully");

        auto dup_res = graph.addFeature("FeatureA");
        FATP_ASSERT_FALSE(dup_res.has_value(), "Should fail to add duplicate flag");
    }

    // Test flag enable/disable
    {
        FeatureManager<> graph;
        (void)graph.addFeature("FeatureA");

        FATP_ASSERT_FALSE(graph.isEnabled("FeatureA"), "Flag should be disabled initially");

        auto enableRes = graph.enable("FeatureA");
        FATP_ASSERT_TRUE(enableRes.has_value(), "Should enable flag");
        FATP_ASSERT_TRUE(graph.isEnabled("FeatureA"), "Flag should be enabled");

        auto disable_res = graph.disable("FeatureA");
        FATP_ASSERT_TRUE(disable_res.has_value(), "Should disable flag");
        FATP_ASSERT_FALSE(graph.isEnabled("FeatureA"), "Flag should be disabled");
    }

    // Test getEnabled
    {
        FeatureManager<> graph;
        (void)graph.addFeature("A");
        (void)graph.addFeature("B");
        (void)graph.addFeature("C");

        (void)graph.enable("A");
        (void)graph.enable("C");

        auto enabled = graph.getEnabled();
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
        (void)graph.addFeature("HighRes");
        (void)graph.addFeature("GPU");
        (void)graph.addRelationship("HighRes", FeatureRelationship::Requires, "GPU");

        // Should auto-enable GPU when enabling HighRes
        auto res = graph.enable("HighRes");
        FATP_ASSERT_TRUE(res.has_value(), "Should auto-enable GPU");
        FATP_ASSERT_TRUE(graph.isEnabled("GPU"), "GPU should be auto-enabled");
        FATP_ASSERT_TRUE(graph.isEnabled("HighRes"), "HighRes should be enabled");
    }

    // Test Conflicts interaction
    {
        FeatureManager<> graph;
        (void)graph.addFeature("HighQuality");
        (void)graph.addFeature("LowLatency");
        (void)graph.addRelationship("HighQuality", FeatureRelationship::Conflicts, "LowLatency");

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
        (void)graph.addFeature("AdvancedGraphics");
        (void)graph.addFeature("BasicGraphics");
        (void)graph.addRelationship("AdvancedGraphics", FeatureRelationship::Implies, "BasicGraphics");

        (void)graph.enable("AdvancedGraphics");
        FATP_ASSERT_TRUE(graph.isEnabled("BasicGraphics"), "BasicGraphics should be auto-enabled by Implies");
    }

    // Test MutuallyExclusive interaction
    {
        FeatureManager<> graph;
        (void)graph.addFeature("ModeA");
        (void)graph.addFeature("ModeB");
        (void)graph.addRelationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");

        (void)graph.enable("ModeA");
        auto res = graph.enable("ModeB");
        FATP_ASSERT_FALSE(res.has_value(), "Should fail: mutually exclusive");
    }

    // Test self-referential prevention
    {
        FeatureManager<> graph;
        (void)graph.addFeature("SelfRef");
        auto res = graph.addRelationship("SelfRef", FeatureRelationship::Requires, "SelfRef");
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

        (void)graph.addFeature("Checked", check);

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
        (void)graph.addFeature("A");
        (void)graph.addFeature("B");
        (void)graph.addFeature("C");

        // Create circular dependency: A->B->C->A
        (void)graph.addRelationship("A", FeatureRelationship::Implies, "B");
        (void)graph.addRelationship("B", FeatureRelationship::Implies, "C");
        (void)graph.addRelationship("C", FeatureRelationship::Implies, "A");

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
        (void)graph.addFeature("Zebra");
        (void)graph.addFeature("Apple");
        (void)graph.addFeature("Banana");

        (void)graph.addRelationship("Zebra", FeatureRelationship::Requires, "Apple");
        (void)graph.addRelationship("Apple", FeatureRelationship::Requires, "Banana");
        (void)graph.addRelationship("Banana", FeatureRelationship::Requires, "Zebra");

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
        const int chain_length = 150; // Exceeds kMaxValidationDepth

        for (int i = 0; i < chain_length; ++i)
        {
            (void)graph.addFeature("Flag" + std::to_string(i));
        }

        for (int i = 0; i < chain_length - 1; ++i)
        {
            (void)graph.addRelationship("Flag" + std::to_string(i),
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
        (void)graph.addFeature("LogBasic");
        (void)graph.addFeature("LogVerbose");
        (void)graph.addFeature("LogDebug");

        auto res = graph.addGroup("Logging", {"LogBasic", "LogVerbose", "LogDebug"});
        FATP_ASSERT_TRUE(res.has_value(), "Should add group");

        auto state = graph.getGroupState("Logging");
        FATP_ASSERT_TRUE(state.has_value(), "Should get group state");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Inactive, "Group should be inactive");

        (void)graph.enable("LogBasic");
        state = graph.getGroupState("Logging");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Partial, "Group should be partial");

        (void)graph.enable("LogVerbose");
        (void)graph.enable("LogDebug");
        state = graph.getGroupState("Logging");
        FATP_ASSERT_TRUE(*state == FeatureGroupState::Active, "Group should be active");
    }

    // Test custom state enum
    {
        FeatureManager<> graph;
        (void)graph.addFeature("WiFi");
        (void)graph.addFeature("Bluetooth");

        auto res = graph.addGroup<NetworkState>("Network", {"WiFi", "Bluetooth"}, network_state_computer);
        FATP_ASSERT_TRUE(res.has_value(), "Should add group with custom state");

        auto state = graph.getGroupState<NetworkState>("Network");
        FATP_ASSERT_TRUE(state.has_value(), "Should get custom state");
        FATP_ASSERT_TRUE(*state == NetworkState::Disconnected, "Should be disconnected");

        (void)graph.enable("WiFi");
        state = graph.getGroupState<NetworkState>("Network");
        FATP_ASSERT_TRUE(*state == NetworkState::Connecting, "Should be connecting");

        (void)graph.enable("Bluetooth");
        state = graph.getGroupState<NetworkState>("Network");
        FATP_ASSERT_TRUE(*state == NetworkState::Connected, "Should be connected");
    }

    // Test mutually exclusive group
    {
        FeatureManager<> graph;
        (void)graph.addFeature("Red");
        (void)graph.addFeature("Green");
        (void)graph.addFeature("Blue");

        auto res = graph.addMutuallyExclusiveGroup("Color", {"Red", "Green", "Blue"});
        FATP_ASSERT_TRUE(res.has_value(), "Should add mutually exclusive group");

        (void)graph.enable("Red");
        auto enableRes = graph.enable("Green");
        FATP_ASSERT_FALSE(enableRes.has_value(), "Should fail: mutually exclusive");
    }
    return true;
}

FATP_TEST_CASE(complex_scenario)
{
    // Test realistic game graphics configuration
    FeatureManager<> graph;

    // Add flags
    (void)graph.addFeature("DX12");
    (void)graph.addFeature("Vulkan");
    (void)graph.addFeature("OpenGL");
    (void)graph.addFeature("HighRes");
    (void)graph.addFeature("MSAA");
    (void)graph.addFeature("RayTracing");
    (void)graph.addFeature("VSync");

    // Mutually exclusive rendering backends
    (void)graph.addMutuallyExclusiveGroup("RenderBackend", {"DX12", "Vulkan", "OpenGL"});

    // Dependencies
    (void)graph.addRelationship("RayTracing", FeatureRelationship::Requires, "DX12");
    (void)graph.addRelationship("HighRes", FeatureRelationship::Implies, "MSAA");

    // Group for advanced features
    (void)graph.addGroup("AdvancedGraphics", {"HighRes", "MSAA", "RayTracing"});

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
    FATP_ASSERT_TRUE(graph.isEnabled("MSAA"), "MSAA should be auto-enabled");

    // Check group state
    auto state = graph.getGroupState("AdvancedGraphics");
    FATP_ASSERT_TRUE(*state == FeatureGroupState::Active, "All advanced features should be active");

    return true;
}

FATP_TEST_CASE(thread_safety)
{
    FeatureManager<MutexSynchronizationPolicy> graph;
    (void)graph.addFeature("SharedFlag");

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
            bool enabled = graph.isEnabled("SharedFlag");
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
    (void)graph.addFeature("Observed");

    int call_count = 0;
    bool last_state = false;
    bool last_success = false;

    FeatureObserver cb = [&](const std::string& name, bool newState, bool success) {
        (void)name;
        call_count++;
        last_state = newState;
        last_success = success;
    };

    (void)graph.addObserver(cb, 0);

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
    (void)graph.addFeature("NodeA");
    (void)graph.addFeature("NodeB");
    (void)graph.addFeature("NodeC");
    (void)graph.addRelationship("NodeA", FeatureRelationship::Requires, "NodeB");
    (void)graph.addRelationship("NodeA", FeatureRelationship::Conflicts, "NodeC");
    (void)graph.addGroup("TestGroup", {"NodeA", "NodeB"});

    std::string dot = graph.toDot();
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
        (void)manager.addFeature("A");
        (void)manager.addFeature("B");
        (void)manager.addFeature("C");

        (void)manager.enable("A");
        (void)manager.enable("B");
        (void)manager.enable("C");

        auto result = manager.batchDisable({"A", "B"});
        FATP_ASSERT_TRUE(result.has_value(), "Should disable A and B");
        FATP_ASSERT_FALSE(manager.isEnabled("A"), "A should be disabled");
        FATP_ASSERT_FALSE(manager.isEnabled("B"), "B should be disabled");
        FATP_ASSERT_TRUE(manager.isEnabled("C"), "C should still be enabled");
    }

    // Batch disable with dependency violation (rollback)
    {
        FeatureManager<> manager;
        (void)manager.addFeature("Base");
        (void)manager.addFeature("Dependent");
        (void)manager.addRelationship("Dependent", FeatureRelationship::Requires, "Base");

        (void)manager.enable("Dependent"); // Also enables Base

        auto result = manager.batchDisable({"Base"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Dependent requires Base");
        FATP_ASSERT_TRUE(manager.isEnabled("Base"), "Base should still be enabled (rollback)");
        FATP_ASSERT_TRUE(manager.isEnabled("Dependent"), "Dependent should still be enabled");
    }

    // Non-existent feature
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");

        auto result = manager.batchDisable({"A", "NonExistent"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail for non-existent feature");
    }

    // Empty batch
    {
        FeatureManager<> manager;
        auto result = manager.batchDisable({});
        FATP_ASSERT_TRUE(result.has_value(), "Empty batch should succeed");
    }

    // Batch disable with duplicate entries must still rollback correctly
    {
        FeatureManager<> manager;
        (void)manager.addFeature("Base");
        (void)manager.addFeature("Dependent");
        (void)manager.addRelationship("Dependent", FeatureRelationship::Requires, "Base");

        (void)manager.enable("Dependent"); // Also enables Base
        FATP_ASSERT_TRUE(manager.isEnabled("Base"), "Base should be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("Dependent"), "Dependent should be enabled");

        // Try to disable Base twice in same batch - should fail and rollback correctly
        auto result = manager.batchDisable({"Base", "Base"});
        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Dependent requires Base (with duplicates)");
        FATP_ASSERT_TRUE(manager.isEnabled("Base"), "Base should still be enabled after rollback");
        FATP_ASSERT_TRUE(manager.isEnabled("Dependent"), "Dependent should still be enabled");
    }

    return true;
}

FATP_TEST_CASE(batch_enable_rollback)
{
    // Test that implicit dependencies are rolled back on failure
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");
        (void)manager.addFeature("B");
        (void)manager.addFeature("C");
        (void)manager.addFeature("Conflict");

        // A requires B (so enabling A will also enable B)
        (void)manager.addRelationship("A", FeatureRelationship::Requires, "B");
        // C conflicts with Conflict
        (void)manager.addRelationship("C", FeatureRelationship::Conflicts, "Conflict");

        // Enable Conflict first
        (void)manager.enable("Conflict");

        // Now try to batch enable A and C
        // A will succeed (enabling B implicitly)
        // C will fail (conflicts with Conflict)
        // Both A and B should be rolled back
        auto result = manager.batchEnable({"A", "C"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail due to conflict");
        FATP_ASSERT_FALSE(manager.isEnabled("A"), "A should be rolled back");
        FATP_ASSERT_FALSE(manager.isEnabled("B"), "B (implicit) should be rolled back");
        FATP_ASSERT_FALSE(manager.isEnabled("C"), "C should not be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("Conflict"), "Conflict should remain enabled");
    }

    // Test rollback with deeper dependency chain
    {
        FeatureManager<> manager;
        (void)manager.addFeature("L1");
        (void)manager.addFeature("L2");
        (void)manager.addFeature("L3");
        (void)manager.addFeature("Fail");
        (void)manager.addFeature("Blocker");

        // L1 -> L2 -> L3 dependency chain
        (void)manager.addRelationship("L1", FeatureRelationship::Requires, "L2");
        (void)manager.addRelationship("L2", FeatureRelationship::Requires, "L3");
        // Fail conflicts with Blocker
        (void)manager.addRelationship("Fail", FeatureRelationship::Conflicts, "Blocker");

        (void)manager.enable("Blocker");

        // Try to enable L1 (will enable L2, L3) and Fail (will fail)
        auto result = manager.batchEnable({"L1", "Fail"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail");
        FATP_ASSERT_FALSE(manager.isEnabled("L1"), "L1 should be rolled back");
        FATP_ASSERT_FALSE(manager.isEnabled("L2"), "L2 should be rolled back");
        FATP_ASSERT_FALSE(manager.isEnabled("L3"), "L3 should be rolled back");
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
        (void)manager.addFeature("A");
        (void)manager.addFeature("B");
        (void)manager.addFeature("C");
        (void)manager.addFeature("D");

        (void)manager.addRelationship("A", FeatureRelationship::Requires, "B");
        (void)manager.addRelationship("A", FeatureRelationship::Requires, "C");
        (void)manager.addRelationship("C", FeatureRelationship::Conflicts, "D");

        // Enable D first (so C will conflict)
        (void)manager.enable("D");

        // Try to enable A - should fail because C conflicts with D
        auto result = manager.enable("A");

        FATP_ASSERT_FALSE(result.has_value(), "enable(A) should fail due to C/D conflict");
        FATP_ASSERT_FALSE(manager.isEnabled("A"), "A should not be enabled");
        FATP_ASSERT_FALSE(manager.isEnabled("B"), "B should NOT be left enabled (transactional rollback)");
        FATP_ASSERT_FALSE(manager.isEnabled("C"), "C should not be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("D"), "D should remain enabled");
    }

    // Test with deeper chain: X requires Y requires Z, Z conflicts with Blocker
    {
        FeatureManager<> manager;
        (void)manager.addFeature("X");
        (void)manager.addFeature("Y");
        (void)manager.addFeature("Z");
        (void)manager.addFeature("Blocker");

        (void)manager.addRelationship("X", FeatureRelationship::Requires, "Y");
        (void)manager.addRelationship("Y", FeatureRelationship::Requires, "Z");
        (void)manager.addRelationship("Z", FeatureRelationship::Conflicts, "Blocker");

        (void)manager.enable("Blocker");

        auto result = manager.enable("X");

        FATP_ASSERT_FALSE(result.has_value(), "enable(X) should fail");
        FATP_ASSERT_FALSE(manager.isEnabled("X"), "X should not be enabled");
        FATP_ASSERT_FALSE(manager.isEnabled("Y"), "Y should be rolled back");
        FATP_ASSERT_FALSE(manager.isEnabled("Z"), "Z should not be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("Blocker"), "Blocker should remain");
    }

    // Test successful enable still works
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");
        (void)manager.addFeature("B");
        (void)manager.addRelationship("A", FeatureRelationship::Requires, "B");

        auto result = manager.enable("A");

        FATP_ASSERT_TRUE(result.has_value(), "enable(A) should succeed");
        FATP_ASSERT_TRUE(manager.isEnabled("A"), "A should be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("B"), "B should be enabled (dependency)");
    }

    return true;
}

FATP_TEST_CASE(remove_observer)
{
    // Test basic add and remove
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");

        int call_count = 0;
        ObserverId id = manager.addObserver([&](auto, auto, auto) {
            ++call_count;
        });

        (void)manager.enable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer should be called once");

        bool removed = manager.removeObserver(id);
        FATP_ASSERT_TRUE(removed, "Should remove existing observer");

        (void)manager.disable("A");
        FATP_ASSERT_EQ(call_count, 1, "Observer should not be called after removal");
    }

    // Test remove non-existent
    {
        FeatureManager<> manager;
        bool removed = manager.removeObserver(999);
        FATP_ASSERT_FALSE(removed, "Should return false for non-existent ID");
    }

    // Test multiple observers with removal
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");

        int count1 = 0, count2 = 0;
        ObserverId id1 = manager.addObserver([&](auto, auto, auto) {
            ++count1;
        });
        ObserverId id2 = manager.addObserver([&](auto, auto, auto) {
            ++count2;
        });

        (void)manager.enable("A");
        FATP_ASSERT_EQ(count1, 1, "Observer 1 called");
        FATP_ASSERT_EQ(count2, 1, "Observer 2 called");

        (void)manager.removeObserver(id1);
        (void)manager.disable("A");

        FATP_ASSERT_EQ(count1, 1, "Observer 1 not called after removal");
        FATP_ASSERT_EQ(count2, 2, "Observer 2 still called");

        (void)manager.removeObserver(id2);
    }

    // Test clearObservers
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");

        int count = 0;
        (void)manager.addObserver([&](auto, auto, auto) {
            ++count;
        });
        (void)manager.addObserver([&](auto, auto, auto) {
            ++count;
        });

        manager.clearObservers();

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
        (void)manager.addFeature("A");

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
        (void)manager.addFeature("A");

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
        (void)manager.addFeature("A");

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
        (void)manager.removeObserver(released_id);
    }

    return true;
}

FATP_TEST_CASE(batch_observer)
{
    // Test batch observer receives all changed features
    {
        FeatureManager<> manager;
        (void)manager.addFeature("Core");
        (void)manager.addFeature("Module1");
        (void)manager.addFeature("Module2");
        (void)manager.addRelationship("Module1", FeatureRelationship::Requires, "Core");
        (void)manager.addRelationship("Module2", FeatureRelationship::Requires, "Core");

        std::string requested;
        std::vector<FeatureChange> changes;
        bool was_success = false;

        (void)manager.addBatchObserver([&](auto req, auto ch, auto ok) {
            requested = req;
            changes = ch;
            was_success = ok;
        });

        // Enable Module1 - should also enable Core
        (void)manager.enable("Module1");

        FATP_ASSERT_EQ(requested, "Module1", "Requested feature should be Module1");
        FATP_ASSERT_TRUE(was_success, "Should succeed");
        FATP_ASSERT_TRUE(changes.size() >= 2, "Should have at least 2 changed features");

        // Check that both Core and Module1 are in the changed list (all enabled=true)
        bool has_core = std::any_of(changes.begin(), changes.end(),
                                    [](const FeatureChange& c) { return c.name == "Core" && c.newState; });
        bool has_module1 = std::any_of(changes.begin(), changes.end(),
                                       [](const FeatureChange& c) { return c.name == "Module1" && c.newState; });
        FATP_ASSERT_TRUE(has_core, "Core should be in changed list");
        FATP_ASSERT_TRUE(has_module1, "Module1 should be in changed list");
    }

    // Test ScopedBatchObserver
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");

        int call_count = 0;
        {
            FeatureManager<>::ScopedBatchObserver scoped(manager, [&](auto, auto, auto) {
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
        (void)manager.addFeature("Base1");
        (void)manager.addFeature("Base2");
        (void)manager.addFeature("Dependent");
        (void)manager.addRelationship("Dependent", FeatureRelationship::Requires, "Base1");
        (void)manager.addRelationship("Dependent", FeatureRelationship::Requires, "Base2");

        std::vector<std::string> notified_features;
        (void)manager.addObserver([&](const std::string& name, bool enabled, bool) {
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
        (void)manager.addFeature("Premium");
        (void)manager.addFeature("AllFeatures");
        (void)manager.addRelationship("Premium", FeatureRelationship::Implies, "AllFeatures");

        std::vector<std::string> notified;
        (void)manager.addObserver([&](const std::string& name, bool enabled, bool) {
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
    // Test that batchDisable checks Implies relationships
    {
        FeatureManager<> manager;
        (void)manager.addFeature("Premium");
        (void)manager.addFeature("AllFeatures");
        (void)manager.addRelationship("Premium", FeatureRelationship::Implies, "AllFeatures");

        // Enable Premium (which implies AllFeatures)
        (void)manager.enable("Premium");
        FATP_ASSERT_TRUE(manager.isEnabled("Premium"), "Premium should be enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("AllFeatures"), "AllFeatures should be enabled");

        // Try to disable AllFeatures while Premium is still enabled
        auto result = manager.batchDisable({"AllFeatures"});

        FATP_ASSERT_FALSE(result.has_value(), "Should fail: Premium implies AllFeatures");
        FATP_ASSERT_TRUE(manager.isEnabled("AllFeatures"), "AllFeatures should remain enabled");
        FATP_ASSERT_TRUE(manager.isEnabled("Premium"), "Premium should remain enabled");
    }

    // Test that disabling the implier first allows disabling the implied
    {
        FeatureManager<> manager;
        (void)manager.addFeature("A");
        (void)manager.addFeature("B");
        (void)manager.addRelationship("A", FeatureRelationship::Implies, "B");

        (void)manager.enable("A");

        // Disable A first (the implier)
        auto result1 = manager.disable("A");
        FATP_ASSERT_TRUE(result1.has_value(), "Should succeed to disable A");

        // Now B can be disabled
        auto result2 = manager.batchDisable({"B"});
        FATP_ASSERT_TRUE(result2.has_value(), "Should succeed to disable B now");
        FATP_ASSERT_FALSE(manager.isEnabled("B"), "B should be disabled");
    }

    // Test complex Implies chain
    {
        FeatureManager<> manager;
        (void)manager.addFeature("Top");
        (void)manager.addFeature("Middle");
        (void)manager.addFeature("Bottom");
        (void)manager.addRelationship("Top", FeatureRelationship::Implies, "Middle");
        (void)manager.addRelationship("Middle", FeatureRelationship::Implies, "Bottom");

        (void)manager.enable("Top");

        // Cannot disable Bottom while Middle is enabled (which implies it)
        auto r1 = manager.batchDisable({"Bottom"});
        FATP_ASSERT_FALSE(r1.has_value(), "Cannot disable Bottom: Middle implies it");

        // Cannot disable Middle while Top is enabled (which implies it)
        auto r2 = manager.batchDisable({"Middle"});
        FATP_ASSERT_FALSE(r2.has_value(), "Cannot disable Middle: Top implies it");

        // Can disable Top
        auto r3 = manager.disable("Top");
        FATP_ASSERT_TRUE(r3.has_value(), "Can disable Top");

        // Now can disable Middle
        auto r4 = manager.batchDisable({"Middle"});
        FATP_ASSERT_TRUE(r4.has_value(), "Can now disable Middle");

        // Now can disable Bottom
        auto r5 = manager.batchDisable({"Bottom"});
        FATP_ASSERT_TRUE(r5.has_value(), "Can now disable Bottom");
    }

    return true;
}


FATP_TEST_CASE(scoped_feature_change)
{
    // Test 1: Basic scoped enable and auto-restore
    {
        FeatureManager<> fm;
        (void)fm.addFeature("Alpha");
        FATP_ASSERT_FALSE(fm.isEnabled("Alpha"), "Alpha starts disabled");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "Alpha", true);
            FATP_ASSERT_TRUE(guard.valid(), "Scoped enable should succeed for simple feature");
            FATP_ASSERT_TRUE(fm.isEnabled("Alpha"), "Alpha should be enabled inside scope");
        }
        FATP_ASSERT_FALSE(fm.isEnabled("Alpha"), "Alpha should be restored to disabled after scope");
    }

    // Test 2: Scoped disable and auto-restore
    {
        FeatureManager<> fm;
        (void)fm.addFeature("Beta");
        (void)fm.enable("Beta");
        FATP_ASSERT_TRUE(fm.isEnabled("Beta"), "Beta starts enabled");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "Beta", false);
            FATP_ASSERT_TRUE(guard.valid(), "Scoped disable should succeed for simple feature");
            FATP_ASSERT_FALSE(fm.isEnabled("Beta"), "Beta should be disabled inside scope");
        }
        FATP_ASSERT_TRUE(fm.isEnabled("Beta"), "Beta should be restored to enabled after scope");
    }

    // Test 3: Scoped enable with dependencies — transitive enable is rolled back
    {
        FeatureManager<> fm;
        (void)fm.addFeature("Base");
        (void)fm.addFeature("Dependent");
        (void)fm.addRelationship("Dependent", FeatureRelationship::Requires, "Base");

        FATP_ASSERT_FALSE(fm.isEnabled("Base"), "Base starts disabled");
        FATP_ASSERT_FALSE(fm.isEnabled("Dependent"), "Dependent starts disabled");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "Dependent", true);
            FATP_ASSERT_TRUE(guard.valid(), "Scoped enable should succeed (Base auto-enabled)");
            FATP_ASSERT_TRUE(fm.isEnabled("Base"), "Base should be transitively enabled");
            FATP_ASSERT_TRUE(fm.isEnabled("Dependent"), "Dependent should be enabled");
        }
        FATP_ASSERT_FALSE(fm.isEnabled("Base"), "Base should be restored to disabled");
        FATP_ASSERT_FALSE(fm.isEnabled("Dependent"), "Dependent should be restored to disabled");
    }

    // Test 4: Scoped enable fails on conflict — valid() returns false, state unchanged
    {
        FeatureManager<> fm;
        (void)fm.addFeature("X");
        (void)fm.addFeature("Y");
        (void)fm.addRelationship("X", FeatureRelationship::Conflicts, "Y");
        (void)fm.enable("Y");

        FATP_ASSERT_TRUE(fm.isEnabled("Y"), "Y starts enabled");
        FATP_ASSERT_FALSE(fm.isEnabled("X"), "X starts disabled");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "X", true);
            FATP_ASSERT_FALSE(guard.valid(), "Scoped enable should fail due to conflict with Y");
            FATP_ASSERT_FALSE(fm.isEnabled("X"), "X should remain disabled after failed scoped enable");
            FATP_ASSERT_TRUE(fm.isEnabled("Y"), "Y should remain enabled after failed scoped enable");
        }
        // After scope: nothing changed, nothing to restore
        FATP_ASSERT_FALSE(fm.isEnabled("X"), "X still disabled after scope exit");
        FATP_ASSERT_TRUE(fm.isEnabled("Y"), "Y still enabled after scope exit");
    }

    // Test 5: operator bool() works
    {
        FeatureManager<> fm;
        (void)fm.addFeature("Z");
        FeatureManager<>::ScopedFeatureChange guard(fm, "Z", true);
        if (!guard)
        {
            FATP_ASSERT_TRUE(false, "operator bool() should return true for valid scoped change");
        }
    }

    // Test 6: Scoped enable on nonexistent feature — valid() returns false
    {
        FeatureManager<> fm;
        FeatureManager<>::ScopedFeatureChange guard(fm, "DoesNotExist", true);
        FATP_ASSERT_FALSE(guard.valid(), "Scoped enable of nonexistent feature should be invalid");
    }

    // Test 7: Nested scoped changes restore correctly
    {
        FeatureManager<> fm;
        (void)fm.addFeature("N1");
        (void)fm.addFeature("N2");
        FATP_ASSERT_FALSE(fm.isEnabled("N1"), "N1 starts disabled");
        FATP_ASSERT_FALSE(fm.isEnabled("N2"), "N2 starts disabled");

        {
            FeatureManager<>::ScopedFeatureChange g1(fm, "N1", true);
            FATP_ASSERT_TRUE(fm.isEnabled("N1"), "N1 enabled by outer scope");
            {
                FeatureManager<>::ScopedFeatureChange g2(fm, "N2", true);
                FATP_ASSERT_TRUE(fm.isEnabled("N1"), "N1 still enabled in inner scope");
                FATP_ASSERT_TRUE(fm.isEnabled("N2"), "N2 enabled by inner scope");
            }
            FATP_ASSERT_TRUE(fm.isEnabled("N1"), "N1 still enabled after inner scope exits");
            FATP_ASSERT_FALSE(fm.isEnabled("N2"), "N2 restored to disabled after inner scope exits");
        }
        FATP_ASSERT_FALSE(fm.isEnabled("N1"), "N1 restored to disabled after outer scope exits");
        FATP_ASSERT_FALSE(fm.isEnabled("N2"), "N2 still disabled after outer scope exits");
    }

    return true;
}

FATP_TEST_CASE(scoped_feature_change_graph_modified_while_alive)
{
    // Regression: ScopedFeatureChange destructor must not restore an invalid
    // graph state when the graph was validly modified while the guard was alive.
    //
    // Bug: the old destructor restored feature bits one-by-one without
    // validating invariants, producing a graph that failed validate().
    //
    // Repro sequence:
    //   A Requires B — both initially enabled.
    //   Guard disables A  (legal: A=false, B=true).
    //   Caller disables B (legal while A is off).
    //   Guard destructs:  old code restored A=true, left B=false => invalid.
    //   Fixed code:       validates rollback first; skips it if invalid.

    // Test 1: rollback skipped when it would violate Requires
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Requires, "B");

        (void)fm.enable("B");
        (void)fm.enable("A");
        FATP_ASSERT_TRUE(fm.isEnabled("A"), "A starts enabled");
        FATP_ASSERT_TRUE(fm.isEnabled("B"), "B starts enabled");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "A", false);
            FATP_ASSERT_TRUE(guard.valid(), "Disabling A should succeed");
            FATP_ASSERT_FALSE(fm.isEnabled("A"), "A is disabled inside guard");
            FATP_ASSERT_TRUE(fm.isEnabled("B"), "B still enabled inside guard");

            // External code disables B while A is already off — valid on its own.
            auto res = fm.disable("B");
            FATP_ASSERT_TRUE(res.has_value(), "Disabling B while A is off must succeed");
            FATP_ASSERT_FALSE(fm.isEnabled("B"), "B is now disabled");

            // Guard destructs here: restoring A=true would require B=true,
            // but B=false. The destructor must skip the rollback entirely.
        }

        // Graph must be valid after guard destruction.
        auto v = fm.validate();
        FATP_ASSERT_TRUE(v.has_value(),
            "validate() must succeed after guard destruction (rollback was skipped)");

        // A must NOT have been silently restored to true — that is the bug.
        FATP_ASSERT_FALSE(fm.isEnabled("A"),
            "A must remain false: restoring it with B=false would violate Requires");
        FATP_ASSERT_FALSE(fm.isEnabled("B"), "B remains false");
    }

    // Test 2: normal rollback still works when graph was not modified externally
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Requires, "B");

        (void)fm.enable("B");
        (void)fm.enable("A");

        {
            FeatureManager<>::ScopedFeatureChange guard(fm, "A", false);
            FATP_ASSERT_TRUE(guard.valid(), "Disabling A should succeed");
            FATP_ASSERT_FALSE(fm.isEnabled("A"), "A disabled inside guard");
            FATP_ASSERT_TRUE(fm.isEnabled("B"), "B still enabled inside guard");
            // No external modifications — rollback should proceed normally.
        }

        // Rollback succeeded: both features restored to their prior state.
        FATP_ASSERT_TRUE(fm.validate().has_value(), "Graph valid after normal rollback");
        FATP_ASSERT_TRUE(fm.isEnabled("A"), "A restored to enabled");
        FATP_ASSERT_TRUE(fm.isEnabled("B"), "B remains enabled");
    }

    return true;
}

FATP_TEST_CASE(scoped_feature_change_check_callback_blocks_rollback)
{
    // Regression: validateDesiredState() must run node.check() callbacks, not
    // only structural (Requires/Conflicts/...) checks.
    //
    // If external state changes while a ScopedFeatureChange guard is alive so
    // that a feature's check callback would now fail, the destructor must skip
    // rollback rather than restore that feature to enabled and produce a graph
    // that fails validate().

    bool checkShouldPass = true; // external state controlled by the test

    fat_p::feature::FeatureManager<> fm;
    (void)fm.addFeature("A", [&checkShouldPass]() -> fat_p::Expected<void, std::string> {
        if (!checkShouldPass)
        {
            return fat_p::unexpected(std::string("external condition failed"));
        }
        return {};
    });

    // Initially the check passes — enable A.
    FATP_ASSERT_TRUE(fm.enable("A").has_value(), "A enables when check passes");
    FATP_ASSERT_TRUE(fm.isEnabled("A"), "A is enabled");

    {
        // Guard disables A (check not run on disable path).
        fat_p::feature::FeatureManager<>::ScopedFeatureChange guard(fm, "A", false);
        FATP_ASSERT_TRUE(guard.valid(), "Disabling A should succeed");
        FATP_ASSERT_FALSE(fm.isEnabled("A"), "A disabled inside guard");

        // External state changes: A's check would now fail.
        checkShouldPass = false;

        // Guard destructs: rollback wants to restore A=true, but A's check
        // now fails. validateDesiredState must catch this and skip rollback.
    }

    // Graph must be valid: rollback was skipped.
    auto v = fm.validate();
    FATP_ASSERT_TRUE(v.has_value(),
        "validate() must succeed after guard destruction (check-blocked rollback)");

    // A must NOT have been restored to true — its check would fail.
    FATP_ASSERT_FALSE(fm.isEnabled("A"),
        "A must remain false: its check callback now fails, rollback must be skipped");

    return true;
}


// ============================================================================
// SECTION 1b: Preempts Relationship Tests
// ============================================================================

FATP_TEST_CASE(preempts_disables_active_target)
{
    // Enabling a preemptor while the target is enabled must disable the target.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("Manual");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "Manual");

    FATP_ASSERT_TRUE(fm.enable("Manual").has_value(), "Manual should enable");
    FATP_ASSERT_TRUE(fm.isEnabled("Manual"), "Manual is enabled");

    auto res = fm.enable("EmergencyStop");
    FATP_ASSERT_TRUE(res.has_value(), "EmergencyStop should enable successfully");
    FATP_ASSERT_TRUE(fm.isEnabled("EmergencyStop"), "EmergencyStop is enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("Manual"), "Manual must be preempted (disabled)");

    return true;
}

FATP_TEST_CASE(preempts_target_already_off)
{
    // Preempting a target that is already disabled succeeds with no state change to target.
    FeatureManager<> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");

    FATP_ASSERT_FALSE(fm.isEnabled("B"), "B starts disabled");
    auto res = fm.enable("A");
    FATP_ASSERT_TRUE(res.has_value(), "Enabling A succeeds (B already off)");
    FATP_ASSERT_TRUE(fm.isEnabled("A"), "A is enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("B"), "B remains disabled");

    return true;
}

FATP_TEST_CASE(preempts_disables_reverse_dependents)
{
    // Cascade: preempting ESC must also disable MotorMix (which Requires ESC),
    // and FlightControl (which Requires MotorMix). The entire reverse-dependency
    // closure is disabled, leaving the graph consistent.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("ESC");
    (void)fm.addFeature("MotorMix");
    (void)fm.addFeature("FlightControl");
    (void)fm.addRelationship("MotorMix",      FeatureRelationship::Requires, "ESC");
    (void)fm.addRelationship("FlightControl", FeatureRelationship::Requires, "MotorMix");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "ESC");

    // Bring up the full actuation chain.
    FATP_ASSERT_TRUE(fm.enable("FlightControl").has_value(), "FlightControl enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("ESC"), "ESC auto-enabled via chain");
    FATP_ASSERT_TRUE(fm.isEnabled("MotorMix"), "MotorMix auto-enabled via chain");
    FATP_ASSERT_TRUE(fm.isEnabled("FlightControl"), "FlightControl is enabled");

    // Assert e-stop.
    FATP_ASSERT_TRUE(fm.enable("EmergencyStop").has_value(), "EmergencyStop enables (cascade must succeed)");

    FATP_ASSERT_TRUE(fm.isEnabled("EmergencyStop"), "EmergencyStop is enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC disabled (direct preempt)");
    FATP_ASSERT_FALSE(fm.isEnabled("MotorMix"), "MotorMix cascade-disabled (Requires ESC)");
    FATP_ASSERT_FALSE(fm.isEnabled("FlightControl"), "FlightControl cascade-disabled (Requires MotorMix)");

    // Graph must validate cleanly after e-stop.
    FATP_ASSERT_TRUE(fm.validate().has_value(), "Graph is consistent after preemption cascade");

    return true;
}

FATP_TEST_CASE(active_preemptor_blocks_reenable)
{
    // While EmergencyStop is enabled, re-enabling ESC must fail.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("ESC");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "ESC");

    (void)fm.enable("ESC");
    (void)fm.enable("EmergencyStop");
    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC preempted");

    auto reEnable = fm.enable("ESC");
    FATP_ASSERT_FALSE(reEnable.has_value(), "Re-enabling ESC while EmergencyStop is on must fail");
    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC remains disabled");

    return true;
}

FATP_TEST_CASE(active_preemptor_blocks_dependency_enable)
{
    // While EmergencyStop is active (preempting ESC), attempting to enable
    // MotorMix (which Requires ESC) must also fail — ESC cannot be brought
    // back on transitively.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("ESC");
    (void)fm.addFeature("MotorMix");
    (void)fm.addRelationship("MotorMix",      FeatureRelationship::Requires, "ESC");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "ESC");

    (void)fm.enable("EmergencyStop");

    auto res = fm.enable("MotorMix");
    FATP_ASSERT_FALSE(res.has_value(), "Enabling MotorMix must fail (ESC is preempted)");
    FATP_ASSERT_FALSE(fm.isEnabled("MotorMix"), "MotorMix remains disabled");
    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC remains disabled");

    return true;
}

FATP_TEST_CASE(batch_enable_contradictory_roots_fails)
{
    // batchEnable({EmergencyStop, Manual}) where EmergencyStop Preempts Manual
    // must fail: the batch is contradictory because both roots cannot be enabled
    // simultaneously.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("Manual");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "Manual");

    auto res = fm.batchEnable({"EmergencyStop", "Manual"});
    FATP_ASSERT_FALSE(res.has_value(), "Contradictory batch must fail");

    // Neither feature should be enabled (no partial state).
    FATP_ASSERT_FALSE(fm.isEnabled("EmergencyStop"), "EmergencyStop not enabled after failed batch");
    FATP_ASSERT_FALSE(fm.isEnabled("Manual"), "Manual not enabled after failed batch");

    return true;
}

FATP_TEST_CASE(preempts_round_trip_json)
{
    // Preempts edges survive a JSON serialization round-trip.
    FeatureManager<> original;
    (void)original.addFeature("A");
    (void)original.addFeature("B");
    (void)original.addRelationship("A", FeatureRelationship::Preempts, "B");

    const std::string json = original.toJson();
    auto restoredRes = FeatureManager<>::fromJson(json);
    FATP_ASSERT_TRUE(restoredRes.has_value(), "fromJson should succeed");

    FeatureManager<>& restored = *restoredRes;

    // The restored manager must enforce the Preempts relationship.
    (void)restored.enable("B");
    FATP_ASSERT_TRUE(restored.isEnabled("B"), "B enabled in restored graph");

    auto enableA = restored.enable("A");
    FATP_ASSERT_TRUE(enableA.has_value(), "A enables after round-trip");
    FATP_ASSERT_TRUE(restored.isEnabled("A"), "A is enabled");
    FATP_ASSERT_FALSE(restored.isEnabled("B"), "B is preempted after round-trip");

    return true;
}

FATP_TEST_CASE(preempts_validates_loaded_state)
{
    // A JSON graph with A enabled + A Preempts B + B enabled must fail validate().
    const std::string badJson = R"({"features": {"A": {"enabled": true, "Preempts": ["B"]}, "B": {"enabled": true}}})";
    auto fmRes = FeatureManager<>::fromJson(badJson);

    // fromJson itself may reject this (running validateUnlocked at the end),
    // or it may load and validate() must catch it. Either is correct.
    if (fmRes.has_value())
    {
        auto validateRes = fmRes->validate();
        FATP_ASSERT_FALSE(validateRes.has_value(),
                          "validate() must detect A preempts B but both enabled");
    }
    // If fromJson rejected it, that is also correct — the test passes either way.

    return true;
}

FATP_TEST_CASE(fromjson_rejects_preempts_contradiction)
{
    // fromJson must reject a graph where a feature both Preempts and Requires
    // the same target — a contradiction the live API refuses at addRelationship time.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B"], "Requires": ["B"]},
            "B": {"enabled": false}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_FALSE(res.has_value(),
            "fromJson must reject Preempts+Requires on the same directed edge");
    }

    // Also reject Preempts + Implies contradiction.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B"], "Implies": ["B"]},
            "B": {"enabled": false}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_FALSE(res.has_value(),
            "fromJson must reject Preempts+Implies on the same directed edge");
    }

    return true;
}

FATP_TEST_CASE(fromjson_rejects_preempts_cycle)
{
    // fromJson must reject a Preempts cycle (A Preempts B Preempts A) that the
    // live addRelationship API would also refuse to construct.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B"]},
            "B": {"enabled": false, "Preempts": ["A"]}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_FALSE(res.has_value(),
            "fromJson must reject a two-node Preempts cycle");
    }

    // Three-node cycle: A -> B -> C -> A.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B"]},
            "B": {"enabled": false, "Preempts": ["C"]},
            "C": {"enabled": false, "Preempts": ["A"]}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_FALSE(res.has_value(),
            "fromJson must reject a three-node Preempts cycle");
    }

    // Non-cycle (A -> B -> C, no back-edge) must still load successfully.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B"]},
            "B": {"enabled": false, "Preempts": ["C"]},
            "C": {"enabled": false}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_TRUE(res.has_value(),
            "fromJson must accept a valid Preempts chain with no cycle");
    }

    // Diamond DAG (A->B->D, A->C->D) must load successfully — shared descendant
    // is not a cycle. The old iterative detector falsely rejected this because it
    // treated "already visited D" as a back-edge.
    {
        const std::string json = R"({"features": {
            "A": {"enabled": false, "Preempts": ["B", "C"]},
            "B": {"enabled": false, "Preempts": ["D"]},
            "C": {"enabled": false, "Preempts": ["D"]},
            "D": {"enabled": false}
        }})";
        auto res = FeatureManager<>::fromJson(json);
        FATP_ASSERT_TRUE(res.has_value(),
            "fromJson must accept a diamond-shaped Preempts DAG (shared descendant is not a cycle)");
    }

    return true;
}

FATP_TEST_CASE(preempts_in_dot_export)
{
    // Preempts edges must appear in DOT output.
    FeatureManager<> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");

    const std::string dot = fm.toDot();
    FATP_ASSERT_TRUE(dot.find("Preempts") != std::string::npos,
                     "DOT output must contain Preempts edge label");
    // Preempts edges are red bold with tee arrowhead.
    FATP_ASSERT_TRUE(dot.find("bold") != std::string::npos,
                     "Preempts edge must be bold");
    FATP_ASSERT_TRUE(dot.find("tee") != std::string::npos,
                     "Preempts edge must use tee arrowhead");
    FATP_ASSERT_TRUE(dot.find("red") != std::string::npos,
                     "Preempts edge must be red");

    return true;
}

FATP_TEST_CASE(scoped_feature_change_restores_preempted_features)
{
    // ScopedFeatureChange must restore preempted features to their prior state on scope exit.
    // This validates the per-feature FeatureChange rollback: B was true before the guard,
    // the guard enabled A (preempting B, setting B false), on scope exit A returns to false
    // and B returns to true — regardless of direction.
    FeatureManager<> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");

    (void)fm.enable("B");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B starts enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("A"), "A starts disabled");

    {
        FeatureManager<>::ScopedFeatureChange guard(fm, "A", true);
        FATP_ASSERT_TRUE(guard.valid(), "Scoped enable of A should succeed");
        FATP_ASSERT_TRUE(fm.isEnabled("A"), "A enabled inside scope");
        FATP_ASSERT_FALSE(fm.isEnabled("B"), "B preempted inside scope");
    }

    FATP_ASSERT_FALSE(fm.isEnabled("A"), "A restored to disabled after scope");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B restored to enabled after scope");

    return true;
}

FATP_TEST_CASE(preempts_contradiction_rejected_at_add_time)
{
    // Adding Preempts when Implies exists (same direction) must fail.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Implies, "B");
        auto res = fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject Preempts when Implies exists (contradictory)");
    }

    // Adding Preempts when Requires exists (same direction) must fail.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Requires, "B");
        auto res = fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject Preempts when Requires exists (contradictory)");
    }

    // Adding Implies when Preempts already exists must fail.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        auto res = fm.addRelationship("A", FeatureRelationship::Implies, "B");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject Implies when Preempts exists (contradictory)");
    }

    // Adding Requires when Preempts already exists must fail.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        auto res = fm.addRelationship("A", FeatureRelationship::Requires, "B");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject Requires when Preempts exists (contradictory)");
    }

    return true;
}

FATP_TEST_CASE(preempts_cycle_rejected_at_add_time)
{
    // A Preempts cycle (A Preempts B Preempts A) must be rejected.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        auto res = fm.addRelationship("B", FeatureRelationship::Preempts, "A");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject Preempts cycle A->B->A");
    }

    // Three-node Preempts cycle.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("X");
        (void)fm.addFeature("Y");
        (void)fm.addFeature("Z");
        (void)fm.addRelationship("X", FeatureRelationship::Preempts, "Y");
        (void)fm.addRelationship("Y", FeatureRelationship::Preempts, "Z");
        auto res = fm.addRelationship("Z", FeatureRelationship::Preempts, "X");
        FATP_ASSERT_FALSE(res.has_value(), "Must reject three-node Preempts cycle X->Y->Z->X");
    }

    // A->B->C chain (no cycle) must be accepted.
    {
        FeatureManager<> fm;
        (void)fm.addFeature("A");
        (void)fm.addFeature("B");
        (void)fm.addFeature("C");
        (void)fm.addRelationship("A", FeatureRelationship::Preempts, "B");
        auto res = fm.addRelationship("B", FeatureRelationship::Preempts, "C");
        FATP_ASSERT_TRUE(res.has_value(), "Non-cyclic A->B->C Preempts chain must be accepted");
    }

    return true;
}

FATP_TEST_CASE(preempts_release_does_not_auto_restore)
{
    // Disabling the preemptor does not automatically re-enable the preempted features.
    // The system must be explicitly brought back up by the operator.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("ESC");
    (void)fm.addFeature("MotorMix");
    (void)fm.addRelationship("MotorMix",      FeatureRelationship::Requires, "ESC");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "ESC");

    (void)fm.enable("MotorMix"); // brings up ESC and MotorMix
    (void)fm.enable("EmergencyStop"); // preempts ESC, cascades to MotorMix

    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC disabled by preemption");
    FATP_ASSERT_FALSE(fm.isEnabled("MotorMix"), "MotorMix cascade-disabled");

    // Release e-stop.
    (void)fm.disable("EmergencyStop");
    FATP_ASSERT_FALSE(fm.isEnabled("EmergencyStop"), "EmergencyStop released");

    // ESC and MotorMix must remain off — no auto-restore.
    FATP_ASSERT_FALSE(fm.isEnabled("ESC"), "ESC stays disabled after e-stop release (no auto-restore)");
    FATP_ASSERT_FALSE(fm.isEnabled("MotorMix"), "MotorMix stays disabled after e-stop release (no auto-restore)");

    // But now re-enable should be possible.
    auto reEnable = fm.enable("MotorMix");
    FATP_ASSERT_TRUE(reEnable.has_value(), "MotorMix can be re-enabled after e-stop is released");
    FATP_ASSERT_TRUE(fm.isEnabled("ESC"), "ESC brought back via chain");
    FATP_ASSERT_TRUE(fm.isEnabled("MotorMix"), "MotorMix re-enabled");

    return true;
}

FATP_TEST_CASE(preempts_batch_observer_reports_mixed_changes)
{
    // A batch observer must receive both the enable change (EmergencyStop: false->true)
    // and the disable change (Manual: true->false) in the same callback.
    FeatureManager<> fm;
    (void)fm.addFeature("EmergencyStop");
    (void)fm.addFeature("Manual");
    (void)fm.addRelationship("EmergencyStop", FeatureRelationship::Preempts, "Manual");

    (void)fm.enable("Manual");

    std::vector<FeatureChange> observedChanges;
    (void)fm.addBatchObserver([&](auto /*req*/, auto changes, auto /*ok*/) {
        observedChanges = changes;
    });

    (void)fm.enable("EmergencyStop");

    // Must have received changes for both features.
    bool sawEmergencyEnable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) {
            return c.name == "EmergencyStop" && !c.oldState && c.newState;
        });
    bool sawManualDisable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) {
            return c.name == "Manual" && c.oldState && !c.newState;
        });

    FATP_ASSERT_TRUE(sawEmergencyEnable, "Batch observer must report EmergencyStop enabled");
    FATP_ASSERT_TRUE(sawManualDisable, "Batch observer must report Manual disabled (preempted)");

    return true;
}

// ============================================================================
// replace() tests
// ============================================================================

FATP_TEST_CASE(replace_happy_path_mutually_exclusive)
{
    // Happy path: replace succeeds between two MutuallyExclusive features when 'from' is enabled.
    FeatureManager<> fm;
    (void)fm.addFeature("ModeA");
    (void)fm.addFeature("ModeB");
    (void)fm.addRelationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");

    (void)fm.enable("ModeA");
    FATP_ASSERT_TRUE(fm.isEnabled("ModeA"), "ModeA must be on before replace");
    FATP_ASSERT_FALSE(fm.isEnabled("ModeB"), "ModeB must be off before replace");

    auto res = fm.replace("ModeA", "ModeB");
    FATP_ASSERT_TRUE(res.has_value(), "replace ModeA->ModeB must succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("ModeA"), "ModeA must be disabled after replace");
    FATP_ASSERT_TRUE(fm.isEnabled("ModeB"), "ModeB must be enabled after replace");

    return true;
}

FATP_TEST_CASE(replace_disables_reverse_dependency_closure_of_from)
{
    // replace correctly disables the reverse-dependency closure of 'from':
    // features that Require or Imply 'from' are transitively disabled, not 'from's own deps.
    FeatureManager<> fm;
    (void)fm.addFeature("Sensor");
    (void)fm.addFeature("ModeA");   // Requires Sensor
    (void)fm.addFeature("ModeB");   // MutuallyExclusive with ModeA
    (void)fm.addFeature("ModeADep"); // Requires ModeA (reverse-dependency of ModeA)
    (void)fm.addRelationship("ModeA",    FeatureRelationship::Requires,         "Sensor");
    (void)fm.addRelationship("ModeADep", FeatureRelationship::Requires,         "ModeA");
    (void)fm.addRelationship("ModeA",    FeatureRelationship::MutuallyExclusive, "ModeB");

    (void)fm.enable("ModeADep"); // pulls up ModeA and Sensor transitively
    FATP_ASSERT_TRUE(fm.isEnabled("Sensor"),   "Sensor enabled via chain");
    FATP_ASSERT_TRUE(fm.isEnabled("ModeA"),    "ModeA enabled via chain");
    FATP_ASSERT_TRUE(fm.isEnabled("ModeADep"), "ModeADep enabled");

    auto res = fm.replace("ModeA", "ModeB");
    FATP_ASSERT_TRUE(res.has_value(), "replace must succeed");

    // ModeADep depends on ModeA and must be cascade-disabled.
    FATP_ASSERT_FALSE(fm.isEnabled("ModeA"),    "ModeA disabled by replace");
    FATP_ASSERT_FALSE(fm.isEnabled("ModeADep"), "ModeADep disabled (reverse-dep closure of ModeA)");

    // ModeB must now be on.
    FATP_ASSERT_TRUE(fm.isEnabled("ModeB"), "ModeB enabled by replace");

    // Sensor is a forward dependency of ModeA (ModeA Requires Sensor), not in ModeA's
    // reverse-dep closure.  planDisableClosure only disables things that depend ON the
    // named feature — it does not cascade downward to orphaned forward deps.
    // Sensor has no remaining consumers in this graph, but nothing explicitly disabled it,
    // so it remains enabled.
    FATP_ASSERT_TRUE(fm.isEnabled("Sensor"), "Sensor is a forward dep of ModeA, stays enabled");

    return true;
}

FATP_TEST_CASE(replace_from_not_enabled_returns_error)
{
    // replace must return an error when 'from' is not currently enabled.
    FeatureManager<> fm;
    (void)fm.addFeature("ModeA");
    (void)fm.addFeature("ModeB");
    (void)fm.addRelationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");

    // ModeA is NOT enabled.
    auto res = fm.replace("ModeA", "ModeB");
    FATP_ASSERT_FALSE(res.has_value(), "replace must fail when 'from' is not enabled");

    // No state must have changed.
    FATP_ASSERT_FALSE(fm.isEnabled("ModeA"), "ModeA still disabled");
    FATP_ASSERT_FALSE(fm.isEnabled("ModeB"), "ModeB still disabled");

    return true;
}

FATP_TEST_CASE(replace_same_feature_returns_error)
{
    // replace(X, X) must return an error — not a silent no-op.
    FeatureManager<> fm;
    (void)fm.addFeature("ModeA");
    (void)fm.enable("ModeA");

    auto res = fm.replace("ModeA", "ModeA");
    FATP_ASSERT_FALSE(res.has_value(), "replace(X, X) must return an error");
    FATP_ASSERT_TRUE(fm.isEnabled("ModeA"), "ModeA state unchanged after rejected self-replace");

    return true;
}

FATP_TEST_CASE(replace_non_me_pair_works)
{
    // replace works for a pair with no MutuallyExclusive relationship: plain disable-A then enable-B.
    FeatureManager<> fm;
    (void)fm.addFeature("Alpha");
    (void)fm.addFeature("Beta");
    // No relationship between them.

    (void)fm.enable("Alpha");
    FATP_ASSERT_TRUE(fm.isEnabled("Alpha"), "Alpha on before replace");

    auto res = fm.replace("Alpha", "Beta");
    FATP_ASSERT_TRUE(res.has_value(), "replace on non-ME pair must succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("Alpha"), "Alpha disabled");
    FATP_ASSERT_TRUE(fm.isEnabled("Beta"), "Beta enabled");

    return true;
}

FATP_TEST_CASE(replace_observer_fires_once_with_both_changes)
{
    // BatchObserver fires exactly once; requestedFeature == 'to'; changes contain both
    // the from-disable and the to-enable records.
    FeatureManager<> fm;
    (void)fm.addFeature("ModeA");
    (void)fm.addFeature("ModeB");
    (void)fm.addRelationship("ModeA", FeatureRelationship::MutuallyExclusive, "ModeB");
    (void)fm.enable("ModeA");

    int callCount = 0;
    std::string observedRequested;
    std::vector<FeatureChange> observedChanges;

    (void)fm.addBatchObserver([&](const std::string& req, const std::vector<FeatureChange>& changes, bool) {
        ++callCount;
        observedRequested = req;
        observedChanges = changes;
    });

    auto res = fm.replace("ModeA", "ModeB");
    FATP_ASSERT_TRUE(res.has_value(), "replace must succeed");

    FATP_ASSERT_EQ(callCount, 1, "BatchObserver must fire exactly once");
    FATP_ASSERT_EQ(observedRequested, std::string("ModeB"), "requestedFeature must be 'to'");

    bool sawModeADisable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) { return c.name == "ModeA" && c.oldState && !c.newState; });
    bool sawModeBEnable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) { return c.name == "ModeB" && !c.oldState && c.newState; });

    FATP_ASSERT_TRUE(sawModeADisable, "changes must contain ModeA disable record");
    FATP_ASSERT_TRUE(sawModeBEnable, "changes must contain ModeB enable record");

    return true;
}

// ============================================================================
// forceExclusive() tests
// ============================================================================

FATP_TEST_CASE(force_exclusive_estop_sequence)
{
    // Complex graph, multiple features active; forceExclusive(safe_mode) leaves only
    // safe_mode and its Requires chain; all unrelated features are disabled.
    FeatureManager<> fm;
    (void)fm.addFeature("battery_monitor");
    (void)fm.addFeature("motor_mix");
    (void)fm.addFeature("normal_mode");
    (void)fm.addFeature("network_stub");
    (void)fm.addFeature("safe_mode");
    (void)fm.addRelationship("motor_mix",   FeatureRelationship::Requires, "battery_monitor");
    (void)fm.addRelationship("normal_mode", FeatureRelationship::Requires, "motor_mix");
    (void)fm.addRelationship("safe_mode",   FeatureRelationship::Requires, "network_stub");

    (void)fm.enable("normal_mode"); // pulls up motor_mix and battery_monitor
    FATP_ASSERT_TRUE(fm.isEnabled("normal_mode"),    "normal_mode on");
    FATP_ASSERT_TRUE(fm.isEnabled("motor_mix"),      "motor_mix on");
    FATP_ASSERT_TRUE(fm.isEnabled("battery_monitor"),"battery_monitor on");

    auto res = fm.forceExclusive("safe_mode");
    FATP_ASSERT_TRUE(res.has_value(), "forceExclusive must succeed");

    // safe_mode and its Requires chain must be on.
    FATP_ASSERT_TRUE(fm.isEnabled("safe_mode"),    "safe_mode enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("network_stub"), "network_stub enabled (Requires chain of safe_mode)");

    // Everything unrelated must be off.
    FATP_ASSERT_FALSE(fm.isEnabled("normal_mode"),    "normal_mode disabled by forceExclusive");
    FATP_ASSERT_FALSE(fm.isEnabled("motor_mix"),      "motor_mix disabled by forceExclusive");
    FATP_ASSERT_FALSE(fm.isEnabled("battery_monitor"),"battery_monitor disabled by forceExclusive");

    return true;
}

FATP_TEST_CASE(force_exclusive_already_only_feature_is_noop)
{
    // forceExclusive on a feature that is already the only enabled feature is a no-op;
    // no observer fires.
    FeatureManager<> fm;
    (void)fm.addFeature("safe_mode");
    (void)fm.addFeature("idle");
    (void)fm.enable("safe_mode");

    int callCount = 0;
    (void)fm.addBatchObserver([&](auto, auto, auto) { ++callCount; });

    auto res = fm.forceExclusive("safe_mode");
    FATP_ASSERT_TRUE(res.has_value(), "forceExclusive on already-exclusive feature must succeed");
    FATP_ASSERT_EQ(callCount, 0, "No observer must fire when state is unchanged");
    FATP_ASSERT_TRUE(fm.isEnabled("safe_mode"), "safe_mode remains enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("idle"), "idle remains disabled");

    return true;
}

FATP_TEST_CASE(force_exclusive_feature_not_found_returns_error)
{
    FeatureManager<> fm;
    (void)fm.addFeature("safe_mode");

    auto res = fm.forceExclusive("does_not_exist");
    FATP_ASSERT_FALSE(res.has_value(), "forceExclusive with unknown feature must return error");

    return true;
}

FATP_TEST_CASE(force_exclusive_requires_chain_preserved)
{
    // If safe_mode Requires network_stub, network_stub remains enabled after
    // forceExclusive("safe_mode").
    FeatureManager<> fm;
    (void)fm.addFeature("network_stub");
    (void)fm.addFeature("safe_mode");
    (void)fm.addFeature("normal_mode");
    (void)fm.addRelationship("safe_mode", FeatureRelationship::Requires, "network_stub");

    (void)fm.enable("normal_mode");

    auto res = fm.forceExclusive("safe_mode");
    FATP_ASSERT_TRUE(res.has_value(), "forceExclusive must succeed");
    FATP_ASSERT_TRUE(fm.isEnabled("safe_mode"),    "safe_mode enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("network_stub"), "network_stub enabled via Requires chain");
    FATP_ASSERT_FALSE(fm.isEnabled("normal_mode"), "normal_mode disabled");

    return true;
}

FATP_TEST_CASE(force_exclusive_observer_reports_all_disabled_features)
{
    // All disabled features must appear in the changes vector; requestedFeature == safe_mode.
    FeatureManager<> fm;
    (void)fm.addFeature("safe_mode");
    (void)fm.addFeature("normal_mode");
    (void)fm.addFeature("aux_feature");

    (void)fm.enable("normal_mode");
    (void)fm.enable("aux_feature");

    std::string observedRequested;
    std::vector<FeatureChange> observedChanges;
    (void)fm.addBatchObserver([&](const std::string& req, const std::vector<FeatureChange>& changes, bool) {
        observedRequested = req;
        observedChanges = changes;
    });

    auto res = fm.forceExclusive("safe_mode");
    FATP_ASSERT_TRUE(res.has_value(), "forceExclusive must succeed");

    FATP_ASSERT_EQ(observedRequested, std::string("safe_mode"), "requestedFeature must be 'safe_mode'");

    bool sawSafeModeEnable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) { return c.name == "safe_mode" && !c.oldState && c.newState; });
    bool sawNormalDisable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) { return c.name == "normal_mode" && c.oldState && !c.newState; });
    bool sawAuxDisable = std::any_of(observedChanges.begin(), observedChanges.end(),
        [](const FeatureChange& c) { return c.name == "aux_feature" && c.oldState && !c.newState; });

    FATP_ASSERT_TRUE(sawSafeModeEnable, "changes must contain safe_mode enable record");
    FATP_ASSERT_TRUE(sawNormalDisable,  "changes must contain normal_mode disable record");
    FATP_ASSERT_TRUE(sawAuxDisable,     "changes must contain aux_feature disable record");

    return true;
}

// ============================================================================
// Entails relationship tests
// ============================================================================

// Test: enable(A) where A Entails B → B is cascade-enabled
FATP_TEST_CASE(entails_enable_cascades_to_target)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Entails, "B");

    auto res = fm.enable("A");
    FATP_ASSERT_TRUE(res.has_value(), "enable(A) should succeed");
    FATP_ASSERT_TRUE(fm.isEnabled("A"), "A should be enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B should be cascade-enabled via Entails");
    return true;
}

// Test: disable(A) where A Entails B and no other enabled feature Entails B → B cascade-disabled
FATP_TEST_CASE(entails_disable_cascades_when_no_other_entailer)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Entails, "B");

    (void)fm.enable("A");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B enabled via cascade");

    auto res = fm.disable("A");
    FATP_ASSERT_TRUE(res.has_value(), "disable(A) should succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("A"), "A should be disabled");
    FATP_ASSERT_FALSE(fm.isEnabled("B"), "B should be cascade-disabled (ref-count 0)");
    return true;
}

// Test: A Entails B, C Entails B; disable(A) while C still enabled → B stays enabled
FATP_TEST_CASE(entails_disable_does_not_cascade_when_second_entailer_active)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addFeature("C");
    (void)fm.addRelationship("A", FeatureRelationship::Entails, "B");
    (void)fm.addRelationship("C", FeatureRelationship::Entails, "B");

    (void)fm.enable("A");
    (void)fm.enable("C");

    auto res = fm.disable("A");
    FATP_ASSERT_TRUE(res.has_value(), "disable(A) should succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("A"), "A should be disabled");
    FATP_ASSERT_TRUE(fm.isEnabled("B"),  "B must remain enabled: C still entails it");
    FATP_ASSERT_TRUE(fm.isEnabled("C"),  "C should remain enabled");
    return true;
}

// Test: full lifecycle — A and C both Entail B; enable A, enable C, disable A (B stays), disable C (B disabled)
FATP_TEST_CASE(entails_shared_target_full_cycle)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addFeature("C");
    (void)fm.addRelationship("A", FeatureRelationship::Entails, "B");
    (void)fm.addRelationship("C", FeatureRelationship::Entails, "B");

    (void)fm.enable("A");
    (void)fm.enable("C");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B enabled via A and C");

    std::vector<std::string> disabledNames;
    (void)fm.addBatchObserver([&](const std::string&, const std::vector<FeatureChange>& changes, bool) {
        for (const auto& ch : changes) {
            if (!ch.newState) disabledNames.push_back(ch.name);
        }
    });

    (void)fm.disable("A");
    FATP_ASSERT_TRUE(fm.isEnabled("B"), "B stays enabled after A disabled (C still entails)");

    (void)fm.disable("C");
    FATP_ASSERT_FALSE(fm.isEnabled("B"), "B cascade-disabled after C disabled (ref-count 0)");

    bool bWasDisabled = std::find(disabledNames.begin(), disabledNames.end(), "B") != disabledNames.end();
    FATP_ASSERT_TRUE(bWasDisabled, "BatchObserver must have fired for B's disable");
    return true;
}

// Test: Entails with MutuallyExclusive policy group; replace fires correct cascades
FATP_TEST_CASE(entails_with_mutually_exclusive_policy_group)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("kAlertNone");
    (void)fm.addFeature("kAlertOverload");
    (void)fm.addFeature("kPolicyRoundRobin");
    (void)fm.addFeature("kPolicyWorkStealing");

    (void)fm.addRelationship("kAlertNone",    FeatureRelationship::MutuallyExclusive, "kAlertOverload");
    (void)fm.addRelationship("kPolicyRoundRobin", FeatureRelationship::MutuallyExclusive, "kPolicyWorkStealing");
    (void)fm.addRelationship("kAlertNone",    FeatureRelationship::Entails, "kPolicyRoundRobin");
    (void)fm.addRelationship("kAlertOverload",FeatureRelationship::Entails, "kPolicyWorkStealing");

    (void)fm.enable("kAlertNone");
    FATP_ASSERT_TRUE(fm.isEnabled("kAlertNone"),        "kAlertNone enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kPolicyRoundRobin"), "RoundRobin cascade-enabled via Entails");

    auto res = fm.replace("kAlertNone", "kAlertOverload");
    FATP_ASSERT_TRUE(res.has_value(), "replace should succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("kAlertNone"),         "kAlertNone disabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kAlertOverload"),      "kAlertOverload enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kPolicyWorkStealing"), "WorkStealing enabled via Entails");
    return true;
}

// Test: Entails cycle (A Entails B Entails A) rejected at addRelationship time
FATP_TEST_CASE(entails_cycle_rejected_at_addRelationship)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("A");
    (void)fm.addFeature("B");
    (void)fm.addRelationship("A", FeatureRelationship::Entails, "B");

    auto res = fm.addRelationship("B", FeatureRelationship::Entails, "A");
    FATP_ASSERT_FALSE(res.has_value(), "Entails cycle must be rejected");
    FATP_ASSERT_TRUE(res.error().find("cycle") != std::string::npos ||
                     res.error().find("Cycle") != std::string::npos,
                     "Error must mention cycle");
    return true;
}

// Test: fromJson rejects graph where A is enabled, A Entails B, but B is disabled
FATP_TEST_CASE(entails_fromjson_rejects_inconsistent_enabled_state)
{
    std::string json = R"({
        "features": {
            "A": { "enabled": true, "Entails": ["B"] },
            "B": { "enabled": false }
        }
    })";

    auto res = FeatureManager<SingleThreadedPolicy>::fromJson(json);
    FATP_ASSERT_FALSE(res.has_value(), "fromJson must reject: A entails B but B is disabled");
    return true;
}

// Test: JSON roundtrip preserves Entails edges and cascade behaviour
FATP_TEST_CASE(entails_tojson_fromjson_roundtrip)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("Alert");
    (void)fm.addFeature("Policy");
    (void)fm.addFeature("Admission");
    (void)fm.addRelationship("Alert", FeatureRelationship::Entails, "Policy");
    (void)fm.addRelationship("Alert", FeatureRelationship::Entails, "Admission");
    (void)fm.enable("Alert");

    std::string json = fm.toJson();
    FATP_ASSERT_TRUE(json.find("Entails") != std::string::npos, "toJson must include Entails key");

    auto fm2res = FeatureManager<SingleThreadedPolicy>::fromJson(json);
    FATP_ASSERT_TRUE(fm2res.has_value(), "fromJson must succeed on valid Entails JSON");
    auto& fm2 = *fm2res;
    FATP_ASSERT_TRUE(fm2.isEnabled("Alert"),     "Alert enabled after roundtrip");
    FATP_ASSERT_TRUE(fm2.isEnabled("Policy"),    "Policy enabled after roundtrip");
    FATP_ASSERT_TRUE(fm2.isEnabled("Admission"), "Admission enabled after roundtrip");

    (void)fm2.disable("Alert");
    FATP_ASSERT_FALSE(fm2.isEnabled("Policy"),    "Policy cascade-disabled after roundtrip");
    FATP_ASSERT_FALSE(fm2.isEnabled("Admission"), "Admission cascade-disabled after roundtrip");
    return true;
}

// Test: toDot() labels Entails edges with orange color
FATP_TEST_CASE(entails_in_dot_export)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("Source");
    (void)fm.addFeature("Target");
    (void)fm.addRelationship("Source", FeatureRelationship::Entails, "Target");

    std::string dot = fm.toDot();
    FATP_ASSERT_TRUE(dot.find("Entails") != std::string::npos, "DOT output must contain 'Entails' label");
    FATP_ASSERT_TRUE(dot.find("orange")  != std::string::npos, "DOT output must use orange color for Entails edges");
    return true;
}

// Test: BatchObserver fires for both entailer and cascade-disabled target
FATP_TEST_CASE(entails_observer_fires_on_cascade_disable)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("Alert");
    (void)fm.addFeature("Policy");
    (void)fm.addRelationship("Alert", FeatureRelationship::Entails, "Policy");
    (void)fm.enable("Alert");

    std::vector<FeatureChange> captured;
    (void)fm.addBatchObserver([&](const std::string&, const std::vector<FeatureChange>& changes, bool) {
        for (const auto& ch : changes) captured.push_back(ch);
    });

    (void)fm.disable("Alert");

    bool alertFired  = false;
    bool policyFired = false;
    for (const auto& ch : captured)
    {
        if (ch.name == "Alert"  && ch.oldState && !ch.newState) alertFired  = true;
        if (ch.name == "Policy" && ch.oldState && !ch.newState) policyFired = true;
    }
    FATP_ASSERT_TRUE(alertFired,  "Observer must fire for Alert disable");
    FATP_ASSERT_TRUE(policyFired, "Observer must fire for Policy cascade-disable");
    return true;
}

// Test: overload→latency transition; shared kAdmissionBulkShed stays enabled
FATP_TEST_CASE(entails_replace_shared_target_stays_enabled)
{
    FeatureManager<SingleThreadedPolicy> fm;
    (void)fm.addFeature("kAlertNone");
    (void)fm.addFeature("kAlertOverload");
    (void)fm.addFeature("kAlertLatency");
    (void)fm.addFeature("kPolicyWorkStealing");
    (void)fm.addFeature("kPolicyRoundRobin");
    (void)fm.addFeature("kAdmissionBulkShed");

    (void)fm.addRelationship("kAlertNone",    FeatureRelationship::MutuallyExclusive, "kAlertOverload");
    (void)fm.addRelationship("kAlertNone",    FeatureRelationship::MutuallyExclusive, "kAlertLatency");
    (void)fm.addRelationship("kAlertOverload",FeatureRelationship::MutuallyExclusive, "kAlertLatency");
    (void)fm.addRelationship("kPolicyWorkStealing", FeatureRelationship::MutuallyExclusive, "kPolicyRoundRobin");

    (void)fm.addRelationship("kAlertNone",     FeatureRelationship::Entails, "kPolicyRoundRobin");
    (void)fm.addRelationship("kAlertOverload", FeatureRelationship::Entails, "kPolicyWorkStealing");
    (void)fm.addRelationship("kAlertOverload", FeatureRelationship::Entails, "kAdmissionBulkShed");
    (void)fm.addRelationship("kAlertLatency",  FeatureRelationship::Entails, "kPolicyRoundRobin");
    (void)fm.addRelationship("kAlertLatency",  FeatureRelationship::Entails, "kAdmissionBulkShed");

    (void)fm.enable("kAlertOverload");
    FATP_ASSERT_TRUE(fm.isEnabled("kPolicyWorkStealing"), "WorkStealing active");
    FATP_ASSERT_TRUE(fm.isEnabled("kAdmissionBulkShed"),  "BulkShed active");

    auto res = fm.replace("kAlertOverload", "kAlertLatency");
    FATP_ASSERT_TRUE(res.has_value(), "replace overload→latency should succeed");
    FATP_ASSERT_FALSE(fm.isEnabled("kAlertOverload"),      "kAlertOverload disabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kAlertLatency"),        "kAlertLatency enabled");
    FATP_ASSERT_FALSE(fm.isEnabled("kPolicyWorkStealing"), "WorkStealing disabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kPolicyRoundRobin"),    "RoundRobin enabled");
    FATP_ASSERT_TRUE(fm.isEnabled("kAdmissionBulkShed"),   "BulkShed stays enabled: latency still entails it");
    return true;
}

} // namespace fat_p::testing::logic

// ============================================================================
// SECTION 2: Serialization & Factory Tests
// ============================================================================

namespace fat_p::testing::factory
{

using namespace fat_p::feature;

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
    (void)getFeatureCheckFactory().registerType("module_a.hardware", []() -> FeatureCheck {
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
    (void)getFeatureCheckFactory().registerType("module_b.license", []() -> FeatureCheck {
        return []() {
            return check_license();
        };
    });
}
} // namespace module_b

// --- Actual Tests ---

FATP_TEST_CASE(basic_factory_registration)
{
    auto& factory = getFeatureCheckFactory();
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

    auto checkResult = factory.make("test.simple");
    FATP_ASSERT_TRUE(checkResult.has_value(), "Should find registered check");

    auto check = *checkResult;
    auto result = check();
    FATP_ASSERT_TRUE(result.has_value(), "Check should pass");

    auto missing_result = factory.make("test.missing");
    FATP_ASSERT_FALSE(missing_result.has_value(), "Should not find non-existent check");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_serialization_roundtrip)
{
    auto& factory = getFeatureCheckFactory();
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
    (void)manager.addFeature("GPUAcceleration", "hardware.gpu");
    (void)manager.addFeature("PremiumFeature", "license.valid");
    (void)manager.addFeature("BasicFeature");
    (void)manager.addRelationship("PremiumFeature", FeatureRelationship::Requires, "BasicFeature");

    (void)manager.enable("BasicFeature");
    (void)manager.enable("GPUAcceleration");

    std::string json = manager.toJson();
    FATP_ASSERT_TRUE(!json.empty(), "Should produce JSON");
    FATP_ASSERT_TRUE(json.find("hardware.gpu") != std::string::npos, "Should contain check key");

    auto restored_result = FeatureManager<>::fromJson(json);
    FATP_ASSERT_TRUE(restored_result.has_value(), "Should deserialize successfully");

    auto& restored = *restored_result;
    FATP_ASSERT_TRUE(restored.isEnabled("GPUAcceleration"), "GPUAcceleration should be enabled");
    FATP_ASSERT_TRUE(restored.isEnabled("BasicFeature"), "BasicFeature should be enabled");
    FATP_ASSERT_FALSE(restored.isEnabled("PremiumFeature"), "PremiumFeature should not be enabled");

    factory.clear();
    return true;
}

FATP_TEST_CASE(raii_registration)
{
    auto& factory = getFeatureCheckFactory();
    factory.clear();

    {
        FeatureCheckRegistration reg1("test.raii1", []() -> FeatureCheck {
            return []() -> Expected<void, std::string> {
                return {};
            };
        });
        FATP_ASSERT_TRUE(factory.hasType("test.raii1"), "Should be registered");

        FeatureManager<> manager;
        auto add_result = manager.addFeature("Feature1", "test.raii1");
        FATP_ASSERT_TRUE(add_result.has_value(), "Should add feature");
    }

    FATP_ASSERT_FALSE(factory.hasType("test.raii1"), "Should be unregistered");
    factory.clear();
    return true;
}

FATP_TEST_CASE(module_independence)
{
    auto& factory = getFeatureCheckFactory();
    factory.clear();

    module_a::register_checks();
    module_b::register_checks();

    FeatureManager<> manager;
    (void)manager.addFeature("HardwareFeature", "module_a.hardware");
    (void)manager.addFeature("LicenseFeature", "module_b.license");

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
    auto& factory = getFeatureCheckFactory();
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
    (void)manager.addFeature("A", "check.a");
    (void)manager.addFeature("B", "check.b");
    (void)manager.addFeature("C");
    (void)manager.addFeature("D");

    (void)manager.addRelationship("B", FeatureRelationship::Requires, "A");
    (void)manager.addRelationship("C", FeatureRelationship::Implies, "D");
    (void)manager.addRelationship("A", FeatureRelationship::Conflicts, "D");

    (void)manager.enable("A");
    (void)manager.enable("B");

    std::string json = manager.toJson();
    auto restored_result = FeatureManager<>::fromJson(json);
    FATP_ASSERT_TRUE(restored_result.has_value(), "Should deserialize complex graph");

    auto& restored = *restored_result;
    FATP_ASSERT_TRUE(restored.isEnabled("A"), "A should be enabled");
    FATP_ASSERT_TRUE(restored.isEnabled("B"), "B should be enabled");
    FATP_ASSERT_FALSE(restored.isEnabled("C"), "C should not be enabled");

    auto enable_d = restored.enable("D");
    FATP_ASSERT_FALSE(enable_d.has_value(), "Should not enable D due to conflict");

    factory.clear();
    return true;
}


FATP_TEST_CASE(raii_duplicate_registration_does_not_unregister_original)
{
    auto& factory = getFeatureCheckFactory();
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
    auto& factory = getFeatureCheckFactory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": {
                "enabled": true,
                "check_key": "does_not_exist"
            }
        }
    })";

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "fromJson should fail when check_key is unknown");
    FATP_ASSERT_TRUE(fm_res.error().find("not found") != std::string::npos, "Error should mention missing check_key");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_group_with_missing_feature_fails)
{
    auto& factory = getFeatureCheckFactory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": { "enabled": false }
        },
        "groups": {
            "G": ["A", "MISSING"]
        }
    })";

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "fromJson should fail when a group references a missing feature");
    FATP_ASSERT_TRUE(fm_res.error().find("references missing feature") != std::string::npos,
                     "Error should mention missing group feature");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_relationship_to_missing_feature_fails)
{
    auto& factory = getFeatureCheckFactory();
    factory.clear();

    const std::string json = R"({
        "features": {
            "A": {
                "enabled": false,
                "Requires": ["B"]
            }
        }
    })";

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "fromJson should fail when relationships reference missing target features");
    FATP_ASSERT_TRUE(fm_res.error().find("to missing feature") != std::string::npos,
                     "Error should mention missing relationship target");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_symmetrizes_conflicts)
{
    auto& factory = getFeatureCheckFactory();
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

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_TRUE(fm_res.has_value(), "fromJson should succeed with asymmetric conflicts");

    auto& fm = *fm_res;

    // Enable A first
    FATP_ASSERT_TRUE(fm.enable("A").has_value(), "Should enable A");

    // Try to enable B - should fail because fromJson symmetrized the conflict
    auto b_res = fm.enable("B");
    FATP_ASSERT_FALSE(b_res.has_value(), "B should conflict with A after symmetrization");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_symmetrizes_mutually_exclusive)
{
    auto& factory = getFeatureCheckFactory();
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

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_TRUE(fm_res.has_value(), "fromJson should succeed");

    auto& fm = *fm_res;

    // Enable A first
    FATP_ASSERT_TRUE(fm.enable("A").has_value(), "Should enable A");

    // Try to enable B - should fail because fromJson symmetrized
    auto b_res = fm.enable("B");
    FATP_ASSERT_FALSE(b_res.has_value(), "B should be mutually exclusive with A after symmetrization");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_detects_cycles)
{
    auto& factory = getFeatureCheckFactory();
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

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "fromJson should fail when graph contains cycles");
    FATP_ASSERT_TRUE(fm_res.error().find("Circular") != std::string::npos ||
                         fm_res.error().find("cycle") != std::string::npos ||
                         fm_res.error().find("validation") != std::string::npos,
                     "Error should mention cycle or validation failure");

    factory.clear();
    return true;
}

FATP_TEST_CASE(json_deserialize_validates_enabled_state_invariants)
{
    auto& factory = getFeatureCheckFactory();
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

    auto fm_res = FeatureManager<>::fromJson(json);
    FATP_ASSERT_FALSE(fm_res.has_value(), "fromJson should fail when enabled state violates Requires");
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
    FATP_RUN_TEST_NS(runner, logic, batch_disable);
    FATP_RUN_TEST_NS(runner, logic, batch_enable_rollback);
    FATP_RUN_TEST_NS(runner, logic, enable_transactional);
    FATP_RUN_TEST_NS(runner, logic, remove_observer);
    FATP_RUN_TEST_NS(runner, logic, scoped_observer);
    FATP_RUN_TEST_NS(runner, logic, batch_observer);
    FATP_RUN_TEST_NS(runner, logic, implicit_notifications);
    FATP_RUN_TEST_NS(runner, logic, batch_disable_implies);
    FATP_RUN_TEST_NS(runner, logic, scoped_feature_change);
    FATP_RUN_TEST_NS(runner, logic, scoped_feature_change_graph_modified_while_alive);
    FATP_RUN_TEST_NS(runner, logic, scoped_feature_change_check_callback_blocks_rollback);
    // Preempts relationship tests
    FATP_RUN_TEST_NS(runner, logic, preempts_disables_active_target);
    FATP_RUN_TEST_NS(runner, logic, preempts_target_already_off);
    FATP_RUN_TEST_NS(runner, logic, preempts_disables_reverse_dependents);
    FATP_RUN_TEST_NS(runner, logic, active_preemptor_blocks_reenable);
    FATP_RUN_TEST_NS(runner, logic, active_preemptor_blocks_dependency_enable);
    FATP_RUN_TEST_NS(runner, logic, batch_enable_contradictory_roots_fails);
    FATP_RUN_TEST_NS(runner, logic, preempts_round_trip_json);
    FATP_RUN_TEST_NS(runner, logic, preempts_validates_loaded_state);
    FATP_RUN_TEST_NS(runner, logic, fromjson_rejects_preempts_contradiction);
    FATP_RUN_TEST_NS(runner, logic, fromjson_rejects_preempts_cycle);
    FATP_RUN_TEST_NS(runner, logic, preempts_in_dot_export);
    FATP_RUN_TEST_NS(runner, logic, scoped_feature_change_restores_preempted_features);
    FATP_RUN_TEST_NS(runner, logic, preempts_contradiction_rejected_at_add_time);
    FATP_RUN_TEST_NS(runner, logic, preempts_cycle_rejected_at_add_time);
    FATP_RUN_TEST_NS(runner, logic, preempts_release_does_not_auto_restore);
    FATP_RUN_TEST_NS(runner, logic, preempts_batch_observer_reports_mixed_changes);
    // replace() tests
    FATP_RUN_TEST_NS(runner, logic, replace_happy_path_mutually_exclusive);
    FATP_RUN_TEST_NS(runner, logic, replace_disables_reverse_dependency_closure_of_from);
    FATP_RUN_TEST_NS(runner, logic, replace_from_not_enabled_returns_error);
    FATP_RUN_TEST_NS(runner, logic, replace_same_feature_returns_error);
    FATP_RUN_TEST_NS(runner, logic, replace_non_me_pair_works);
    FATP_RUN_TEST_NS(runner, logic, replace_observer_fires_once_with_both_changes);
    // forceExclusive() tests
    FATP_RUN_TEST_NS(runner, logic, force_exclusive_estop_sequence);
    FATP_RUN_TEST_NS(runner, logic, force_exclusive_already_only_feature_is_noop);
    FATP_RUN_TEST_NS(runner, logic, force_exclusive_feature_not_found_returns_error);
    FATP_RUN_TEST_NS(runner, logic, force_exclusive_requires_chain_preserved);
    FATP_RUN_TEST_NS(runner, logic, force_exclusive_observer_reports_all_disabled_features);
    // Entails relationship tests
    FATP_RUN_TEST_NS(runner, logic, entails_enable_cascades_to_target);
    FATP_RUN_TEST_NS(runner, logic, entails_disable_cascades_when_no_other_entailer);
    FATP_RUN_TEST_NS(runner, logic, entails_disable_does_not_cascade_when_second_entailer_active);
    FATP_RUN_TEST_NS(runner, logic, entails_shared_target_full_cycle);
    FATP_RUN_TEST_NS(runner, logic, entails_with_mutually_exclusive_policy_group);
    FATP_RUN_TEST_NS(runner, logic, entails_cycle_rejected_at_addRelationship);
    FATP_RUN_TEST_NS(runner, logic, entails_fromjson_rejects_inconsistent_enabled_state);
    FATP_RUN_TEST_NS(runner, logic, entails_tojson_fromjson_roundtrip);
    FATP_RUN_TEST_NS(runner, logic, entails_in_dot_export);
    FATP_RUN_TEST_NS(runner, logic, entails_observer_fires_on_cascade_disable);
    FATP_RUN_TEST_NS(runner, logic, entails_replace_shared_target_stays_enabled);

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
        run_benchmarks();
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
