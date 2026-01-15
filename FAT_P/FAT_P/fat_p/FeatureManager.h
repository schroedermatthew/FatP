/**
 * @file FeatureManager.h
 * @brief Runtime feature flag management with compile-time optimization
 *
 * @layer Domain
 *
 * @details
 * A modern C++17 header-only library for managing feature flags with complex dependencies,
 * relationships, and validation. Designed for scenarios where features have interdependencies
 * (Requires, Implies, Conflicts, MutuallyExclusive) and need automatic resolution.
 * Key features:
 * - Cycle detection with detailed error messages showing full dependency path
 * - Pluggable thread-safety policies (single-threaded, mutex, spinlock, shared_mutex)
 * - Type-safe group states with custom enums
 * - Observer pattern with priority ordering and RAII lifetime management
 * - JSON and GraphViz DOT serialization
 * - RAII helpers for scoped state changes
 * - Optimized with SortedContainer for relationship storage (cache-friendly)
 * Performance characteristics:
 * - Add feature: O(log n)
 * - Enable/disable: O(d x log n) where d = dependency depth (limited to MAX_VALIDATION_DEPTH)
 * - Validate: O(n x d x log n)
 * - Memory: ~550 bytes per feature with 5 relationships (using SortedContainer)
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: public_header
  path: fat_p/FeatureManager.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FeatureManager."
  api_stability: in_work
  related:
    docs_search: "FeatureManager"
    tests:
      - tests/test_FeatureManager.cpp
    benchmarks:
      - benchmarks/benchmark_FeatureManager.cpp
  hygiene:
    pragma_once: true
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
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "EnumPlus.h"
#include "Expected.h"
#include "Factory.h"
#include "JsonLite.h"
#include "SortedContainer.h"
#include "Stringify.h"
#include "ValueGuard.h"

namespace fat_p
{

// ============================================================================
// Enum Definitions
// ============================================================================

// Relationship types between features
enum class FeatureRelationship
{
    Requires,         // This feature requires another to be enabled
    Conflicts,        // This feature conflicts with another (mutual)
    Implies,          // Enabling this implies enabling another
    MutuallyExclusive // For groups: all features in set conflict with each other
};

// Default built-in group state enum
enum class FeatureGroupState
{
    Inactive, // No features enabled
    Partial,  // Some but not all features enabled/valid
    Active,   // All required features enabled, no conflicts, all checks pass
    Invalid   // Conflicts or failed checks
};

// ============================================================================
// EnumPlus Specializations
// ============================================================================

template <>
struct EnableOverloadedOperators<FeatureRelationship>
{
    static constexpr bool value = true;
};

template <>
struct EnumStringPolicy<FeatureRelationship>
{
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {"Requires", "Conflicts", "Implies", "MutuallyExclusive"};

    static std::string_view to_string(FeatureRelationship e)
    {
        return names[static_cast<size_t>(e)];
    }

    static FeatureRelationship from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid FeatureRelationship string");
        }
        return static_cast<FeatureRelationship>(std::distance(names.begin(), it));
    }
};

template <>
struct EnumStringPolicy<FeatureGroupState>
{
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {"Inactive", "Partial", "Active", "Invalid"};

    static std::string_view to_string(FeatureGroupState e)
    {
        return names[static_cast<size_t>(e)];
    }

    static FeatureGroupState from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid FeatureGroupState string");
        }
        return static_cast<FeatureGroupState>(std::distance(names.begin(), it));
    }
};

// ============================================================================
// Type Aliases and Forward Declarations
// ============================================================================

template <typename SyncPolicy>
class FeatureManager;

// Function object for feature validation checks
//
// CONTRACT:
// - FeatureCheck callbacks are invoked while the FeatureManager holds its internal lock.
// - Callbacks MUST NOT call any FeatureManager methods (is_enabled, enable, disable, etc.)
//   as this will cause deadlock with MutexSynchronizationPolicy and similar policies.
// - Callbacks SHOULD be fast and non-blocking. Long-running operations (network, disk I/O,
//   license checks) will block all feature operations for the duration.
// - Callbacks MUST be exception-safe. Exceptions will propagate and may leave state inconsistent.
//
// If you need to perform operations that require FeatureManager access or are long-running,
// consider using observers instead (which are called outside the lock).
using FeatureCheck = std::function<Expected<void, std::string>()>;

// Observer callback type: Called on feature state changes
using FeatureObserver = std::function<void(const std::string& feature_name, bool new_state, bool success)>;

// Unique identifier for registered observers (enables removal)
using ObserverId = std::uint64_t;

// Batch observer callback type: Called with all features that changed in a single operation
// Parameters:
//   - requested_feature: The feature that was explicitly enabled/disabled
//   - all_changed: All features that changed state (includes implicit dependencies)
//   - enabled: true if features were enabled, false if disabled
//   - success: true if the operation succeeded
using BatchObserver = std::function<void(const std::string& requested_feature,
                                         const std::vector<std::string>& all_changed,
                                         bool enabled,
                                         bool success)>;

// Policy for computing group state
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupStatePolicy
{
    using state_type = StateEnum;
    static state_type
    compute(const std::set<std::string>& group_features, size_t enabled_count, bool has_conflict, bool all_checks_pass)
    {
        if (group_features.empty())
        {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (has_conflict || !all_checks_pass)
        {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (enabled_count == 0)
        {
            return static_cast<state_type>(FeatureGroupState::Inactive);
        }
        if (enabled_count < group_features.size())
        {
            return static_cast<state_type>(FeatureGroupState::Partial);
        }
        return static_cast<state_type>(FeatureGroupState::Active);
    }
};

// Function type for custom state computation
template <typename StateEnum>
using StateComputer = std::function<StateEnum(const std::set<std::string>&, size_t, bool, bool)>;

// ============================================================================
// FeatureCheck Callback Factory
// ============================================================================

// Global factory for FeatureCheck callbacks
// Key: string identifier, Product: FeatureCheck
using FeatureCheckFactory = SimpleFactory<std::string, FeatureCheck>;

// Singleton accessor for the global FeatureCheck factory
inline FeatureCheckFactory& get_feature_check_factory()
{
    static FeatureCheckFactory factory;
    return factory;
}

// RAII helper class for automatic registration/unregistration of FeatureCheck callbacks
// This allows modules to register their checks independently and have them automatically
// cleaned up when the registration object goes out of scope
class FeatureCheckRegistration
{
public:
    // Register a FeatureCheck creator function with the given key
    // The creator is a factory function that returns a FeatureCheck
    //
    // Notes:
    // - If registration fails (e.g., key already registered), this object does NOT
    //   take ownership of the key and will not unregister it on destruction.
    FeatureCheckRegistration(const std::string& key, std::function<FeatureCheck()> creator)
        : mKey(key)
    {
        mRegistered = get_feature_check_factory().registerType(mKey, std::move(creator));
        if (!mRegistered)
        {
            // Do not claim ownership of an existing registration.
            mKey.clear();
        }
    }

    // Automatically unregister on destruction (only if we successfully registered)
    ~FeatureCheckRegistration()
    {
        if (mRegistered && !mKey.empty())
        {
            [[maybe_unused]] bool unregistered = get_feature_check_factory().unregisterType(mKey);
        }
    }

    // Non-copyable
    FeatureCheckRegistration(const FeatureCheckRegistration&) = delete;
    FeatureCheckRegistration& operator=(const FeatureCheckRegistration&) = delete;

    // Moveable
    FeatureCheckRegistration(FeatureCheckRegistration&& other) noexcept
        : mKey(std::move(other.mKey))
        , mRegistered(std::exchange(other.mRegistered, false))
    {
        other.mKey.clear();
    }

    FeatureCheckRegistration& operator=(FeatureCheckRegistration&& other) noexcept
    {
        if (this != &other)
        {
            if (mRegistered && !mKey.empty())
            {
                (void)get_feature_check_factory().unregisterType(mKey);
            }
            mKey = std::move(other.mKey);
            mRegistered = std::exchange(other.mRegistered, false);
            other.mKey.clear();
        }
        return *this;
    }

private:
    std::string mKey;
    bool mRegistered = false;
};

// ============================================================================
// FeatureNode Structure
// ============================================================================

// Internal representation of a feature with its state and relationships
struct FeatureNode
{
    bool enabled = false;
    FeatureCheck check;
    std::string check_key; // For serialization: the factory key to restore check on load

    // Relationship storage using SortedContainer for cache efficiency
    // Use std::map for relationship types (only 4 possible keys)
    // Use SortedContainer for target sets (frequently iterated, rarely modified)
    std::map<FeatureRelationship, SortedContainer<std::string>> relationships;

    JsonValue to_json() const
    {
        JsonObject obj;
        obj["enabled"] = JsonValue{enabled};
        if (!check_key.empty())
        {
            obj["check_key"] = JsonValue{check_key};
        }
        for (const auto& [type, targets] : relationships)
        {
            JsonArray arr;
            for (const auto& target : targets)
            {
                arr.push_back(JsonValue{target});
            }
            std::string type_name(EnumStringPolicy<FeatureRelationship>::to_string(type));
            obj[type_name] = JsonValue{std::move(arr)};
        }
        return JsonValue{std::move(obj)};
    }

    static Expected<FeatureNode, std::string> from_json(const JsonValue& value)
    {
        if (!value.is_object())
        {
            return unexpected("FeatureNode JSON must be an object");
        }
        const auto& obj = std::get<JsonObject>(value);
        FeatureNode node;

        auto it = obj.find("enabled");
        if (it != obj.end())
        {
            if (!it->second.is_bool())
            {
                return unexpected("enabled must be boolean");
            }
            node.enabled = std::get<bool>(it->second);
        }

        it = obj.find("check_key");
        if (it != obj.end())
        {
            if (!it->second.is_string())
            {
                return unexpected("check_key must be string");
            }
            node.check_key = std::get<std::string>(it->second);

            // Look up callback from factory (STRICT: keys must exist)
            if (!node.check_key.empty())
            {
                auto check_result = get_feature_check_factory().make(node.check_key);
                if (!check_result)
                {
                    return unexpected("check_key '" + node.check_key + "' not found in FeatureCheckFactory");
                }
                node.check = *check_result;
            }
            else
            {
                node.check_key.clear();
            }
        }

        std::array<std::string_view, 4> types = {"Requires", "Conflicts", "Implies", "MutuallyExclusive"};
        for (const auto& type_str : types)
        {
            std::string ts(type_str);
            it = obj.find(ts);
            if (it != obj.end())
            {
                if (!it->second.is_array())
                {
                    return unexpected(ts + " must be array");
                }
                const auto& arr = std::get<JsonArray>(it->second);
                FeatureRelationship type = EnumStringPolicy<FeatureRelationship>::from_string(type_str);
                for (const auto& elem : arr)
                {
                    if (!elem.is_string())
                    {
                        return unexpected("Element in " + ts + " must be string");
                    }
                    auto insert_res = node.relationships[type].insert(std::get<std::string>(elem));
                    if (!insert_res)
                    {
                        return unexpected("Failed to insert relationship: " + insert_res.error());
                    }
                }
            }
        }
        return node;
    }
};

// Base class for type-erased group storage
struct FeatureGroupInfoBase
{
    virtual ~FeatureGroupInfoBase() = default;
    virtual std::set<std::string> get_features() const = 0;
    virtual JsonValue to_json() const = 0;
    virtual std::string state_to_string() const = 0;
};

// Concrete group info with type-safe state computer
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupInfo : public FeatureGroupInfoBase
{
    std::set<std::string> features;
    StateComputer<StateEnum> state_computer;
    mutable StateEnum cached_state;

    FeatureGroupInfo(const std::vector<std::string>& f,
                     StateComputer<StateEnum> comp = FeatureGroupStatePolicy<StateEnum>::compute)
        : features(f.begin(), f.end())
        , state_computer(comp)
        , cached_state(static_cast<StateEnum>(FeatureGroupState::Inactive))
    {
    }

    std::set<std::string> get_features() const override
    {
        return features;
    }

    JsonValue to_json() const override
    {
        JsonArray arr;
        for (const auto& f : features)
        {
            arr.push_back(JsonValue{f});
        }
        return JsonValue{std::move(arr)};
    }

    std::string state_to_string() const override
    {
        if constexpr (EnumStringPolicy<StateEnum>::has_names)
        {
            return std::string(EnumStringPolicy<StateEnum>::to_string(cached_state));
        }
        else
        {
            return toString(cached_state);
        }
    }

    void update_cached_state(StateEnum state) const
    {
        cached_state = state;
    }
};

// ============================================================================
// FeatureManager Class
// ============================================================================

template <typename SyncPolicy = SingleThreadedPolicy>
class FeatureManager
{
private:
    // Internal observer storage with unique ID for removal support
    struct ObserverEntry
    {
        ObserverId id;
        int priority;
        FeatureObserver callback;
    };

    struct BatchObserverEntry
    {
        ObserverId id;
        int priority;
        BatchObserver callback;
    };

    std::map<std::string, FeatureNode> mFeatures;
    std::map<std::string, std::unique_ptr<FeatureGroupInfoBase>> mGroups;
    std::vector<ObserverEntry> mObservers;
    std::vector<BatchObserverEntry> batch_observers_;
    ObserverId next_observer_id_ = 1;
    mutable SyncPolicy mSync;

    // Maximum dependency depth before aborting to prevent stack overflow
    //
    // Rationale for MAX_VALIDATION_DEPTH = 100:
    // 1. Prevents infinite recursion from undetected cycles (defense-in-depth)
    // 2. Protects against stack overflow (typical stack ~8MB, each frame ~100 bytes)
    // 3. 100 levels of dependency is unrealistic in practice (most systems have d < 10)
    // 4. If legitimate use case exceeds 100, consider:
    //    - Refactoring feature graph to reduce depth
    //    - Making enable/validate iterative instead of recursive
    //    - Increasing this constant (test with your stack size)
    //
    // Performance: Each level adds ~50-100ns overhead for function call + map lookups
    // At depth 100: ~5-10us total, which is acceptable for enable operations
    //
    // To measure actual depth in your system:
    //   - Enable verbose logging or use depth parameter in error messages
    //   - Profile with realistic feature graphs
    //   - Adjust this constant if needed (powers of 2 are not required)
    static constexpr size_t MAX_VALIDATION_DEPTH = 100;

    Expected<FeatureNode*, std::string> get_node(const std::string& name)
    {
        auto it = mFeatures.find(name);
        if (it == mFeatures.end())
        {
            return unexpected("Feature not found: " + name);
        }
        return &(it->second);
    }

    Expected<const FeatureNode*, std::string> get_node(const std::string& name) const
    {
        auto it = mFeatures.find(name);
        if (it == mFeatures.end())
        {
            return unexpected("Feature not found: " + name);
        }
        return &(it->second);
    }

    // Add a relationship between two features (lock must already be held)
    [[nodiscard]] Expected<void, std::string>
    add_relationship_unlocked(const std::string& from, FeatureRelationship type, const std::string& to)
    {
        auto from_res = get_node(from);
        if (!from_res)
        {
            return unexpected("Feature not found: " + from + " in relationship setup");
        }
        auto to_res = get_node(to);
        if (!to_res)
        {
            return unexpected("Feature not found: " + to + " in relationship setup");
        }

        // Prevent self-referencing relationships
        if (from == to)
        {
            return unexpected("Cannot add self-referencing relationship: " + from);
        }

        FeatureNode* from_node = *from_res;
        auto insert_res = from_node->relationships[type].insert(to);
        if (!insert_res)
        {
            return unexpected("Failed to insert relationship: " + insert_res.error());
        }

        // Bidirectional for conflicts and mutually exclusive
        if (type == FeatureRelationship::Conflicts || type == FeatureRelationship::MutuallyExclusive)
        {
            FeatureNode* to_node = *to_res;
            auto rev_insert_res = to_node->relationships[type].insert(from);
            if (!rev_insert_res)
            {
                return unexpected("Failed to insert reverse relationship: " + rev_insert_res.error());
            }
        }
        return {};
    }

    // Build a human-readable cycle path from the enabling chain
    // The enabling_chain preserves insertion order (the actual traversal path)
    // Example: enabling_chain = {"A", "B", "C"}, target = "A"
    // Returns: "A -> B -> C -> A"
    std::string build_cycle_path(const std::vector<std::string>& enabling_chain, const std::string& target) const
    {
        if (enabling_chain.empty())
        {
            return target + " (self-referencing)";
        }

        // Build ordered path: the vector preserves actual traversal order
        std::string path;
        bool started = false;

        for (const auto& feature : enabling_chain)
        {
            if (!started)
            {
                path = feature;
                started = true;
            }
            else
            {
                path += " -> " + feature;
            }
        }
        path += " -> " + target;
        return path;
    }

    // Detect cycles in the dependency graph (Requires + Implies).
    // Lock must already be held.
    [[nodiscard]] Expected<void, std::string> detect_cycles_unlocked() const
    {
        enum class VisitState
        {
            Unvisited,
            Visiting,
            Visited
        };

        std::unordered_map<std::string, VisitState> state;
        state.reserve(mFeatures.size());

        std::vector<std::string> stack;
        stack.reserve(mFeatures.size());

        auto dfs = [&](auto&& self, const std::string& name, int depth) -> Expected<void, std::string> {
            if (static_cast<size_t>(depth) > MAX_VALIDATION_DEPTH)
            {
                return unexpected("Maximum dependency depth exceeded at feature: " + name);
            }

            auto it = state.find(name);
            if (it != state.end())
            {
                if (it->second == VisitState::Visiting)
                {
                    return unexpected("Circular dependency detected: " + build_cycle_path(stack, name));
                }
                if (it->second == VisitState::Visited)
                {
                    return {};
                }
            }

            state[name] = VisitState::Visiting;
            stack.push_back(name);

            auto node_it = mFeatures.find(name);
            if (node_it == mFeatures.end())
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return unexpected("Feature not found: " + name);
            }
            const FeatureNode& node = node_it->second;

            auto visit_relationship = [&](FeatureRelationship rel) -> Expected<void, std::string> {
                auto rel_it = node.relationships.find(rel);
                if (rel_it == node.relationships.end())
                {
                    return {};
                }
                for (const auto& dep : rel_it->second)
                {
                    auto dep_state = state.find(dep);
                    if (dep_state != state.end() && dep_state->second == VisitState::Visiting)
                    {
                        return unexpected("Circular dependency detected: " + build_cycle_path(stack, dep));
                    }
                    auto res = self(self, dep, depth + 1);
                    if (!res)
                    {
                        return res;
                    }
                }
                return {};
            };

            auto req_res = visit_relationship(FeatureRelationship::Requires);
            if (!req_res)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return req_res;
            }

            auto impl_res = visit_relationship(FeatureRelationship::Implies);
            if (!impl_res)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return impl_res;
            }

            stack.pop_back();
            state[name] = VisitState::Visited;
            return {};
        };

        for (const auto& [name, _] : mFeatures)
        {
            auto it = state.find(name);
            if (it == state.end() || it->second == VisitState::Unvisited)
            {
                auto res = dfs(dfs, name, 0);
                if (!res)
                {
                    return res;
                }
            }
        }

        return {};
    }

    // Validate the entire feature set for consistency.
    // Lock must already be held.
    [[nodiscard]] Expected<void, std::string> validate_unlocked() const
    {
        // --------------------------------------------------------------------
        // 1) Structural validation: every relationship target must exist.
        // --------------------------------------------------------------------
        for (const auto& [name, node] : mFeatures)
        {
            for (const auto& [rel, targets] : node.relationships)
            {
                for (const auto& target : targets)
                {
                    if (!mFeatures.count(target))
                    {
                        return unexpected("Feature '" + name + "' has relationship '" +
                                          std::string(EnumStringPolicy<FeatureRelationship>::to_string(rel)) +
                                          "' to missing feature '" + target + "'");
                    }
                }
            }
        }

        // --------------------------------------------------------------------
        // 2) Graph validation: detect cycles in the dependency graph.
        // --------------------------------------------------------------------
        auto cycle_res = detect_cycles_unlocked();
        if (!cycle_res)
        {
            return cycle_res;
        }

        // --------------------------------------------------------------------
        // 3) Enabled-state validation: requires/implies/conflicts/checks.
        // --------------------------------------------------------------------
        for (const auto& [name, node] : mFeatures)
        {
            if (!node.enabled)
            {
                continue;
            }

            // Requires: enabled feature must have all required features enabled
            auto req_it = node.relationships.find(FeatureRelationship::Requires);
            if (req_it != node.relationships.end())
            {
                for (const auto& required : req_it->second)
                {
                    auto it = mFeatures.find(required);
                    if (it == mFeatures.end())
                    {
                        return unexpected("Required feature not found: " + required);
                    }
                    if (!it->second.enabled)
                    {
                        return unexpected("'" + name + "' requires '" + required + "' but it's disabled");
                    }
                }
            }

            // Implies: enabled feature must have all implied features enabled
            auto impl_it = node.relationships.find(FeatureRelationship::Implies);
            if (impl_it != node.relationships.end())
            {
                for (const auto& implied : impl_it->second)
                {
                    auto it = mFeatures.find(implied);
                    if (it == mFeatures.end())
                    {
                        return unexpected("Implied feature not found: " + implied);
                    }
                    if (!it->second.enabled)
                    {
                        return unexpected("'" + name + "' implies '" + implied + "' but it's disabled");
                    }
                }
            }

            // Conflicts and MutuallyExclusive
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                auto rel_it = node.relationships.find(rel);
                if (rel_it == node.relationships.end())
                {
                    continue;
                }

                for (const auto& other : rel_it->second)
                {
                    auto it = mFeatures.find(other);
                    if (it == mFeatures.end())
                    {
                        if (rel == FeatureRelationship::Conflicts)
                        {
                            return unexpected("Conflicting feature not found: " + other);
                        }
                        return unexpected("Mutually exclusive feature not found: " + other);
                    }
                    if (it->second.enabled)
                    {
                        if (rel == FeatureRelationship::Conflicts)
                        {
                            return unexpected(name + " conflicts with " + other);
                        }
                        return unexpected(name + " is mutually exclusive with " + other);
                    }
                }
            }

            // Run validation check for enabled features
            if (node.check)
            {
                auto check_result = node.check();
                if (!check_result)
                {
                    return unexpected("Check failed for " + name + ": " + check_result.error());
                }
            }
        }

        return {};
    }

    Expected<void, std::string> enable_feature(const std::string& name,
                                               std::vector<std::string>& enabling_chain,
                                               std::vector<std::string>* changed_features,
                                               int depth = 0)
    {
        if (static_cast<size_t>(depth) > MAX_VALIDATION_DEPTH)
        {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }

        // Helper to check if name is in the chain (preserves order for cycle path reporting)
        auto in_chain = [&enabling_chain](const std::string& n) {
            return std::find(enabling_chain.begin(), enabling_chain.end(), n) != enabling_chain.end();
        };

        // Check for circular dependencies first
        if (in_chain(name))
        {
            return unexpected("Circular dependency detected: " + build_cycle_path(enabling_chain, name));
        }

        auto node_res = get_node(name);
        if (!node_res)
        {
            return unexpected(node_res.error());
        }
        FeatureNode* node = *node_res;

        if (node->enabled)
        {
            return {}; // Already enabled - nothing to do
        }

        // Track that we're in the process of enabling this feature
        enabling_chain.push_back(name);

        // RAII guard to ensure enabling_chain.pop_back() is always called on scope exit.
        // This maintains chain consistency even on early returns from error paths.
        struct ChainGuard
        {
            std::vector<std::string>& chain;
            bool dismissed = false;
            explicit ChainGuard(std::vector<std::string>& c)
                : chain(c)
            {
            }
            ~ChainGuard()
            {
                if (!dismissed)
                {
                    chain.pop_back();
                }
            }
            void dismiss()
            {
                dismissed = true;
            }
        } chain_guard(enabling_chain);

        // Enable this feature first (may be rolled back on error)
        bool was_enabled = node->enabled;
        node->enabled = true;

        // Process Required relationships (recursively enable dependencies)
        if (node->relationships.count(FeatureRelationship::Requires))
        {
            const auto& targets = node->relationships[FeatureRelationship::Requires];
            for (const auto& required : targets)
            {
                // Check for circular dependency before recursing
                if (in_chain(required))
                {
                    node->enabled = was_enabled;
                    return unexpected("Circular dependency detected: " + build_cycle_path(enabling_chain, required));
                }

                auto req_node_res = get_node(required);
                if (!req_node_res)
                {
                    node->enabled = false;
                    return unexpected("Required feature not found: " + required);
                }
                FeatureNode* req_node = *req_node_res;
                if (!req_node->enabled)
                {
                    auto enable_res = enable_feature(required, enabling_chain, changed_features, depth + 1);
                    if (!enable_res)
                    {
                        node->enabled = false;
                        return enable_res;
                    }
                }
            }
        }

        // Process Implies relationships
        if (node->relationships.count(FeatureRelationship::Implies))
        {
            const auto& targets = node->relationships[FeatureRelationship::Implies];
            for (const auto& implied : targets)
            {
                // Check for circular dependency before checking if already enabled
                if (in_chain(implied))
                {
                    node->enabled = false;
                    return unexpected("Circular dependency detected: " + build_cycle_path(enabling_chain, implied));
                }

                auto impl_node_res = get_node(implied);
                if (!impl_node_res)
                {
                    node->enabled = false;
                    return unexpected("Implied feature not found: " + implied);
                }

                FeatureNode* impl_node = *impl_node_res;
                if (!impl_node->enabled)
                {
                    auto enable_res = enable_feature(implied, enabling_chain, changed_features, depth + 1);
                    if (!enable_res)
                    {
                        node->enabled = false;
                        return enable_res;
                    }
                }
            }
        }

        // Check for conflicts
        for (auto type : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
        {
            if (node->relationships.count(type))
            {
                const auto& targets = node->relationships[type];
                for (const auto& conflicting : targets)
                {
                    auto conf_node_res = get_node(conflicting);
                    if (!conf_node_res)
                    {
                        node->enabled = false;
                        if (type == FeatureRelationship::Conflicts)
                        {
                            return unexpected("Conflicting feature not found: " + conflicting);
                        }
                        return unexpected("Mutually exclusive feature not found: " + conflicting);
                    }

                    FeatureNode* conf_node = *conf_node_res;
                    if (conf_node->enabled)
                    {
                        node->enabled = false;
                        if (type == FeatureRelationship::Conflicts)
                        {
                            return unexpected(name + " conflicts with " + conflicting);
                        }
                        return unexpected(name + " is mutually exclusive with " + conflicting);
                    }
                }
            }
        }

        // Run validation check
        if (node->check)
        {
            auto check_result = node->check();
            if (!check_result)
            {
                node->enabled = false;
                return unexpected("Check failed for " + name + ": " + check_result.error());
            }
        }

        // Success - chain_guard destructor will pop_back()
        // (we don't dismiss it because we want the pop to happen)

        // Track this feature as changed (for observer notification)
        if (changed_features && !was_enabled)
        {
            changed_features->push_back(name);
        }

        return {};
    }

    static void sort_observers_by_priority(std::vector<ObserverEntry>& entries)
    {
        // Stable sort preserves insertion order for observers with equal priority.
        std::stable_sort(entries.begin(), entries.end(), [](const ObserverEntry& a, const ObserverEntry& b) {
            return a.priority > b.priority;
        });
    }

    static void sort_batch_observers_by_priority(std::vector<BatchObserverEntry>& entries)
    {
        std::stable_sort(entries.begin(), entries.end(), [](const BatchObserverEntry& a, const BatchObserverEntry& b) {
            return a.priority > b.priority;
        });
    }

    static void notify_observers_sorted(const std::vector<ObserverEntry>& sorted,
                                        const std::string& feature_name,
                                        bool new_state,
                                        bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(feature_name, new_state, success);
        }
    }

    static void notify_batch_observers_sorted(const std::vector<BatchObserverEntry>& sorted,
                                              const std::string& requested_feature,
                                              const std::vector<std::string>& all_changed,
                                              bool enabled,
                                              bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(requested_feature, all_changed, enabled, success);
        }
    }


    template <typename StateEnum>
    Expected<StateEnum, std::string> compute_group_state_impl(const std::string& group_name) const
    {
        auto git = mGroups.find(group_name);
        if (git == mGroups.end())
        {
            return unexpected("Group not found: " + group_name);
        }
        auto* group_ptr = dynamic_cast<FeatureGroupInfo<StateEnum>*>(git->second.get());
        if (!group_ptr)
        {
            return unexpected("Type mismatch: group '" + group_name + "' is not of the requested state type");
        }
        const auto& group_features = group_ptr->features;
        size_t enabled_count = 0;
        bool has_conflict = false;
        bool all_checks_pass = true;
        for (const auto& feature_name : group_features)
        {
            auto node_res = get_node(feature_name);
            if (!node_res)
            {
                continue;
            }
            const FeatureNode* node = *node_res;
            if (node->enabled)
            {
                ++enabled_count;
                // Check for conflicts within group
                for (const auto& other : group_features)
                {
                    if (other == feature_name)
                    {
                        continue;
                    }
                    if (node->relationships.count(FeatureRelationship::Conflicts))
                    {
                        const auto& targets = node->relationships.at(FeatureRelationship::Conflicts);
                        if (targets.count(other) > 0)
                        {
                            auto other_node_res = get_node(other);
                            if (other_node_res && (*other_node_res)->enabled)
                            {
                                has_conflict = true;
                            }
                        }
                    }
                    if (node->relationships.count(FeatureRelationship::MutuallyExclusive))
                    {
                        const auto& targets = node->relationships.at(FeatureRelationship::MutuallyExclusive);
                        if (targets.count(other) > 0)
                        {
                            auto other_node_res = get_node(other);
                            if (other_node_res && (*other_node_res)->enabled)
                            {
                                has_conflict = true;
                            }
                        }
                    }
                }
                if (node->check)
                {
                    auto check_result = node->check();
                    if (!check_result)
                    {
                        all_checks_pass = false;
                    }
                }
            }
        }
        StateEnum state = group_ptr->state_computer(group_features, enabled_count, has_conflict, all_checks_pass);
        group_ptr->update_cached_state(state);
        return state;
    }

public:
    FeatureManager() = default;

    FeatureManager(FeatureManager&& other) noexcept
        : mFeatures(std::move(other.mFeatures))
        , mGroups(std::move(other.mGroups))
        , mObservers(std::move(other.mObservers))
        , batch_observers_(std::move(other.batch_observers_))
        , next_observer_id_(other.next_observer_id_)
        , mSync()
    {
    }

    FeatureManager& operator=(FeatureManager&& other) noexcept
    {
        if (this != &other)
        {
            mFeatures = std::move(other.mFeatures);
            mGroups = std::move(other.mGroups);
            mObservers = std::move(other.mObservers);
            batch_observers_ = std::move(other.batch_observers_);
            next_observer_id_ = other.next_observer_id_;
            // mSync intentionally not assigned/moved: synchronization primitives (mutexes)
            // are not safely movable. The target keeps its existing mSync instance.
        }
        return *this;
    }

    // RAII helper for temporary feature changes
    class ScopedFeatureChange
    {
    private:
        FeatureManager* mManager;
        std::string feature_name_;
        bool original_state_;
        bool mValid;

    public:
        ScopedFeatureChange(FeatureManager& manager, const std::string& feature_name, bool new_state)
            : mManager(&manager)
            , feature_name_(feature_name)
            , mValid(false)
        {
            typename SyncPolicy::LockGuard guard(mManager->mSync.getLock());
            auto node_res = mManager->get_node(feature_name);
            if (node_res)
            {
                FeatureNode* node = *node_res;
                original_state_ = node->enabled;
                mValid = true;
                if (new_state && !node->enabled)
                {
                    node->enabled = true;
                }
                else if (!new_state && node->enabled)
                {
                    node->enabled = false;
                }
            }
        }

        ~ScopedFeatureChange()
        {
            if (mValid)
            {
                {
                    typename SyncPolicy::LockGuard guard(mManager->mSync.getLock());
                    auto node = mManager->get_node(feature_name_);
                    if (node)
                    {
                        (*node)->enabled = original_state_;
                    }
                }
                // Best-effort validation during cleanup (cannot throw from destructor)
                try
                {
                    auto validate_res = mManager->validate();
                    (void)validate_res;
                }
                catch (...)
                {
                    // Swallow all exceptions - destructors must not throw.
                }
            }
        }

        ScopedFeatureChange(const ScopedFeatureChange&) = delete;
        ScopedFeatureChange& operator=(const ScopedFeatureChange&) = delete;
        ScopedFeatureChange(ScopedFeatureChange&&) = delete;
        ScopedFeatureChange& operator=(ScopedFeatureChange&&) = delete;
    };

    // RAII helper for automatic observer registration/unregistration
    // Ensures observers are properly cleaned up when the scope ends
    class ScopedObserver
    {
    private:
        FeatureManager* mManager;
        ObserverId mId;

    public:
        ScopedObserver(FeatureManager& manager, FeatureObserver callback, int priority = 0)
            : mManager(&manager)
            , mId(manager.add_observer(std::move(callback), priority))
        {
        }

        ~ScopedObserver()
        {
            if (mManager && mId != 0)
            {
                mManager->remove_observer(mId);
            }
        }

        // Non-copyable
        ScopedObserver(const ScopedObserver&) = delete;
        ScopedObserver& operator=(const ScopedObserver&) = delete;

        // Moveable
        ScopedObserver(ScopedObserver&& other) noexcept
            : mManager(std::exchange(other.mManager, nullptr))
            , mId(std::exchange(other.mId, 0))
        {
        }

        ScopedObserver& operator=(ScopedObserver&& other) noexcept
        {
            if (this != &other)
            {
                if (mManager && mId != 0)
                {
                    mManager->remove_observer(mId);
                }
                mManager = std::exchange(other.mManager, nullptr);
                mId = std::exchange(other.mId, 0);
            }
            return *this;
        }

        // Get the observer ID (for manual removal if needed)
        ObserverId id() const
        {
            return mId;
        }

        // Release ownership without unregistering
        ObserverId release()
        {
            mManager = nullptr;
            return std::exchange(mId, 0);
        }
    };

    // RAII helper for batch observers
    class ScopedBatchObserver
    {
    private:
        FeatureManager* mManager;
        ObserverId mId;

    public:
        ScopedBatchObserver(FeatureManager& manager, BatchObserver callback, int priority = 0)
            : mManager(&manager)
            , mId(manager.add_batch_observer(std::move(callback), priority))
        {
        }

        ~ScopedBatchObserver()
        {
            if (mManager && mId != 0)
            {
                mManager->remove_observer(mId);
            }
        }

        ScopedBatchObserver(const ScopedBatchObserver&) = delete;
        ScopedBatchObserver& operator=(const ScopedBatchObserver&) = delete;

        ScopedBatchObserver(ScopedBatchObserver&& other) noexcept
            : mManager(std::exchange(other.mManager, nullptr))
            , mId(std::exchange(other.mId, 0))
        {
        }

        ScopedBatchObserver& operator=(ScopedBatchObserver&& other) noexcept
        {
            if (this != &other)
            {
                if (mManager && mId != 0)
                {
                    mManager->remove_observer(mId);
                }
                mManager = std::exchange(other.mManager, nullptr);
                mId = std::exchange(other.mId, 0);
            }
            return *this;
        }

        ObserverId id() const
        {
            return mId;
        }
        ObserverId release()
        {
            mManager = nullptr;
            return std::exchange(mId, 0);
        }
    };

    // Add a feature with an optional validation check
    [[nodiscard]] Expected<void, std::string> add_feature(const std::string& name, FeatureCheck check = nullptr)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        if (mFeatures.count(name))
        {
            return unexpected("Feature already exists: " + name);
        }
        FeatureNode node;
        node.enabled = false;
        node.check = check;
        node.check_key = ""; // No key when added directly with callback
        mFeatures[name] = std::move(node);
        return {};
    }

    // Add a feature using a registered callback key from the factory
    // This allows the feature to be fully serialized and deserialized
    [[nodiscard]] Expected<void, std::string> add_feature(const std::string& name, const std::string& check_key)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        if (mFeatures.count(name))
        {
            return unexpected("Feature already exists: " + name);
        }

        // Look up the check from factory
        auto check_result = get_feature_check_factory().make(check_key);
        if (!check_result)
        {
            return unexpected("Check key '" + check_key + "' not found in factory");
        }

        FeatureNode node;
        node.enabled = false;
        node.check = *check_result;
        node.check_key = check_key;
        mFeatures[name] = std::move(node);
        return {};
    }

    // Add a relationship between two features
    [[nodiscard]] Expected<void, std::string>
    add_relationship(const std::string& from, FeatureRelationship type, const std::string& to)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        return add_relationship_unlocked(from, type, to);
    }

    // Add a feature group with optional custom state computer
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string>
    add_group(const std::string& group_name,
              const std::vector<std::string>& feature_names,
              StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        if (mGroups.count(group_name))
        {
            return unexpected("Group already exists: " + group_name);
        }
        for (const auto& feature_name : feature_names)
        {
            if (!mFeatures.count(feature_name))
            {
                return unexpected("Feature not found: " + feature_name);
            }
        }
        mGroups[group_name] = std::make_unique<FeatureGroupInfo<StateEnum>>(feature_names, computer);
        return {};
    }

    // Add a mutually exclusive group
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string>
    add_mutually_exclusive_group(const std::string& group_name,
                                 const std::vector<std::string>& feature_names,
                                 StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());

        if (mGroups.count(group_name))
        {
            return unexpected("Group already exists: " + group_name);
        }
        for (const auto& feature_name : feature_names)
        {
            if (!mFeatures.count(feature_name))
            {
                return unexpected("Feature not found: " + feature_name);
            }
        }

        // Add bidirectional mutual-exclusion relationships between every pair.
        for (size_t i = 0; i < feature_names.size(); ++i)
        {
            for (size_t j = i + 1; j < feature_names.size(); ++j)
            {
                auto res = add_relationship_unlocked(feature_names[i],
                                                     FeatureRelationship::MutuallyExclusive,
                                                     feature_names[j]);
                if (!res)
                {
                    return res;
                }
            }
        }

        mGroups[group_name] = std::make_unique<FeatureGroupInfo<StateEnum>>(feature_names, computer);
        return {};
    }

    // Get group state with type safety
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<StateEnum, std::string> get_group_state(const std::string& group_name) const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        return compute_group_state_impl<StateEnum>(group_name);
    }

    // Get features in a group
    [[nodiscard]] Expected<std::set<std::string>, std::string> get_group_features(const std::string& group_name) const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        auto git = mGroups.find(group_name);
        if (git == mGroups.end())
        {
            return unexpected("Group not found: " + group_name);
        }
        return git->second->get_features();
    }

    // Enable a feature with full transactional semantics
    // If enabling fails (e.g., due to conflicts), no dependencies are left enabled
    [[nodiscard]] Expected<void, std::string> enable(const std::string& name)
    {
        return batch_enable({name});
    }

    // Disable a feature (Transactional)
    //
    // Delegates to batch_disable to ensure safety constraints (Requires/Implies)
    // are checked and all side effects are handled atomically.
    [[nodiscard]] Expected<void, std::string> disable(const std::string& name)
    {
        return batch_disable({name});
    }

    // Batch enable multiple features with transactional semantics
    // All features (including dependencies) succeed or all changes are rolled back.
    // NOTIFICATIONS ARE DEFERRED until after lock release to prevent deadlocks.
    [[nodiscard]] Expected<void, std::string> batch_enable(const std::vector<std::string>& names)
    {
        std::vector<std::string> all_changed;
        std::vector<ObserverEntry> observers_snapshot;
        std::vector<BatchObserverEntry> batch_observers_snapshot;

        { // Scope for LockGuard - Lock held only during state modification
            typename SyncPolicy::LockGuard guard(mSync.getLock());

            // Validate all features exist first
            for (const auto& name : names)
            {
                auto node_res = get_node(name);
                if (!node_res)
                {
                    return unexpected(node_res.error());
                }
            }

            // Snapshot ALL feature states before any modifications
            std::map<std::string, bool> original_states;
            for (const auto& [name, node] : mFeatures)
            {
                original_states[name] = node.enabled;
            }

            // Attempt to enable each feature
            for (const auto& name : names)
            {
                std::vector<std::string> chain;
                auto res = enable_feature(name, chain, &all_changed);
                if (!res)
                {
                    // Rollback ALL features to original states
                    for (auto& [feature_name, node] : mFeatures)
                    {
                        node.enabled = original_states[feature_name];
                    }
                    return res;
                }
            }

            // Snapshot observers while holding the lock, then invoke callbacks after unlock.
            // This prevents data races and makes it safe for observers to add/remove observers
            // or call FeatureManager methods (reentrant use).
            if (!all_changed.empty())
            {
                observers_snapshot = mObservers;
                batch_observers_snapshot = batch_observers_;
            }
        } // Lock released here

        if (!all_changed.empty())
        {
            // Notify observers safely outside the lock
            sort_observers_by_priority(observers_snapshot);
            for (const auto& feature : all_changed)
            {
                notify_observers_sorted(observers_snapshot, feature, true, true);
            }

            // Notify batch observers
            if (!batch_observers_snapshot.empty())
            {
                sort_batch_observers_by_priority(batch_observers_snapshot);
                notify_batch_observers_sorted(batch_observers_snapshot,
                                              names.empty() ? "" : names[0],
                                              all_changed,
                                              true,
                                              true);
            }
        }

        return {};
    }

    // Batch disable multiple features with transactional semantics
    // All disables succeed or all changes are rolled back.
    // Validates that no enabled feature requires any disabled feature.
    // Also validates Implies relationships: cannot disable a feature that is implied by
    // an enabled feature.
    // NOTIFICATIONS ARE DEFERRED until after lock release to prevent deadlocks.
    // NOTE: Duplicate names in the input are automatically deduplicated (first occurrence wins).
    [[nodiscard]] Expected<void, std::string> batch_disable(const std::vector<std::string>& names)
    {
        std::vector<std::string> actually_changed;
        std::vector<ObserverEntry> observers_snapshot;
        std::vector<BatchObserverEntry> batch_observers_snapshot;

        // Deduplicate requested names while preserving first-seen order.
        // This prevents incorrect rollback when the same feature appears multiple times.
        std::vector<std::string> unique_names;
        unique_names.reserve(names.size());
        std::set<std::string> disabled_set;
        for (const auto& n : names)
        {
            if (disabled_set.insert(n).second)
            {
                unique_names.push_back(n);
            }
        }

        { // Scope for LockGuard - Lock held only during state modification
            typename SyncPolicy::LockGuard guard(mSync.getLock());

            // Validate all features exist first
            for (const auto& name : unique_names)
            {
                auto node_res = get_node(name);
                if (!node_res)
                {
                    return unexpected(node_res.error());
                }
            }

            // Record original states for rollback and track which actually changed
            std::vector<bool> original_states;
            original_states.reserve(unique_names.size());

            for (const auto& name : unique_names)
            {
                auto node_res = get_node(name);
                original_states.push_back((*node_res)->enabled);
                if ((*node_res)->enabled)
                {
                    actually_changed.push_back(name);
                }
                (*node_res)->enabled = false;
            }

            // Rollback helper lambda
            auto rollback = [&]() {
                for (size_t i = 0; i < unique_names.size(); ++i)
                {
                    auto n = get_node(unique_names[i]);
                    if (n)
                    {
                        (*n)->enabled = original_states[i];
                    }
                }
            };

            // Validate the resulting state
            for (const auto& [feature_name, node] : mFeatures)
            {
                if (!node.enabled)
                {
                    continue;
                }

                // Check if this enabled feature requires any of the disabled features
                if (node.relationships.count(FeatureRelationship::Requires))
                {
                    for (const auto& required : node.relationships.at(FeatureRelationship::Requires))
                    {
                        auto req_node = get_node(required);
                        if (!req_node)
                        {
                            rollback();
                            return unexpected("Required feature not found: " + required);
                        }
                        if (!(*req_node)->enabled)
                        {
                            rollback();
                            return unexpected("Cannot disable '" + required + "': required by enabled feature '" +
                                              feature_name + "'");
                        }
                    }
                }

                // Check if this enabled feature implies any of the disabled features
                // If A implies B and A is enabled, then B cannot be disabled
                if (node.relationships.count(FeatureRelationship::Implies))
                {
                    for (const auto& implied : node.relationships.at(FeatureRelationship::Implies))
                    {
                        if (disabled_set.count(implied))
                        {
                            rollback();
                            return unexpected("Cannot disable '" + implied + "': implied by enabled feature '" +
                                              feature_name + "'. Disable '" + feature_name + "' first.");
                        }
                    }
                }
            }

            if (!actually_changed.empty())
            {
                observers_snapshot = mObservers;
                batch_observers_snapshot = batch_observers_;
            }
        } // Lock released here

        if (!actually_changed.empty())
        {
            // Notify observers safely outside the lock
            sort_observers_by_priority(observers_snapshot);
            for (const auto& feature : actually_changed)
            {
                notify_observers_sorted(observers_snapshot, feature, false, true);
            }

            // Notify batch observers
            if (!batch_observers_snapshot.empty())
            {
                sort_batch_observers_by_priority(batch_observers_snapshot);
                notify_batch_observers_sorted(batch_observers_snapshot,
                                              names.empty() ? "" : names[0],
                                              actually_changed,
                                              false,
                                              true);
            }
        }

        return {};
    }

    // Check if feature is enabled
    bool is_enabled(const std::string& name) const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        auto node_res = get_node(name);
        if (!node_res)
        {
            return false;
        }
        return (*node_res)->enabled;
    }

    // Validate entire feature set
    [[nodiscard]] Expected<void, std::string> validate()
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        return validate_unlocked();
    }


    // Add observer with priority
    //
    // Observers are called when features are enabled or disabled. They receive:
    //   - feature_name: The name of the feature that changed
    //   - new_state: true if enabled, false if disabled
    //   - success: true if the operation succeeded
    //
    // Priority ordering: Higher priority observers are called first (e.g., 100 before 10)
    //
    // Returns: ObserverId that can be used to remove the observer later
    //
    // REENTRANCY & THREAD-SAFETY NOTES:
    // - Observers are invoked AFTER the FeatureManager releases its internal lock.
    // - The observer list is snapshotted before invocation, so callbacks may safely
    //   add/remove observers or call FeatureManager methods (reentrant use).
    //
    // Be careful with reentrant modifications: enabling/disabling features from an observer
    // can trigger nested notifications and may lead to cycles if not designed carefully.
    //
    // Observers should remain lightweight and avoid long blocking work to keep feature
    // operations fast.
    //
    ObserverId add_observer(FeatureObserver callback, int priority = 0)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        ObserverId id = next_observer_id_++;
        mObservers.push_back({id, priority, std::move(callback)});
        return id;
    }

    // Add batch observer with priority
    //
    // Batch observers are called once per enable/disable operation with information
    // about ALL features that changed, including implicit dependencies.
    //
    // This is useful when you need to know:
    //   - Which features were implicitly enabled via Requires/Implies relationships
    //   - The complete set of state changes for a single user action
    //
    // Example:
    //   manager.add_batch_observer([](auto requested, auto all_changed, auto enabled, auto ok) {
    //       std::cout << "Requested: " << requested << "\n";
    //       std::cout << "All changed: ";
    //       for (const auto& f : all_changed) std::cout << f << " ";
    //       std::cout << "\n";
    //   });
    //
    // Returns: ObserverId that can be used to remove the observer later
    ObserverId add_batch_observer(BatchObserver callback, int priority = 0)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        ObserverId id = next_observer_id_++;
        batch_observers_.push_back({id, priority, std::move(callback)});
        return id;
    }

    // Remove an observer by ID
    //
    // Returns true if an observer with the given ID was found and removed,
    // false if no observer with that ID exists.
    //
    // Works for both regular and batch observers.
    bool remove_observer(ObserverId id)
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());

        // Check regular observers
        auto it = std::find_if(mObservers.begin(), mObservers.end(), [id](const ObserverEntry& entry) {
            return entry.id == id;
        });
        if (it != mObservers.end())
        {
            mObservers.erase(it);
            return true;
        }

        // Check batch observers
        auto bit =
            std::find_if(batch_observers_.begin(), batch_observers_.end(), [id](const BatchObserverEntry& entry) {
                return entry.id == id;
            });
        if (bit != batch_observers_.end())
        {
            batch_observers_.erase(bit);
            return true;
        }

        return false;
    }

    // Remove all observers
    void clear_observers()
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        mObservers.clear();
        batch_observers_.clear();
    }

    // Get all enabled features
    std::vector<std::string> get_enabled() const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        std::vector<std::string> enabled;
        for (const auto& [name, node] : mFeatures)
        {
            if (node.enabled)
            {
                enabled.push_back(name);
            }
        }
        return enabled;
    }

    // Get all feature names
    std::vector<std::string> get_all_features() const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        std::vector<std::string> all_features;
        for (const auto& [name, _] : mFeatures)
        {
            all_features.push_back(name);
        }
        return all_features;
    }

    // Get all group names
    std::vector<std::string> get_all_groups() const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        std::vector<std::string> all_groups;
        for (const auto& [name, _] : mGroups)
        {
            all_groups.push_back(name);
        }
        return all_groups;
    }

    // Serialize to JSON
    std::string to_json() const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        JsonObject root;
        JsonObject features_json;
        for (const auto& [name, node] : mFeatures)
        {
            features_json[name] = node.to_json();
        }
        root["features"] = JsonValue{std::move(features_json)};
        JsonObject groups_json;
        for (const auto& [name, group] : mGroups)
        {
            groups_json[name] = group->to_json();
        }
        root["groups"] = JsonValue{std::move(groups_json)};
        return to_json_string(root);
    }

    // Deserialize from JSON
    //
    // This function parses JSON and reconstructs the feature graph. It performs:
    // 1. Structural validation: all relationship targets must exist
    // 2. Symmetrization: Conflicts and MutuallyExclusive relationships are made bidirectional
    // 3. Full validation: cycles are detected and enabled-state invariants are checked
    //
    // If any validation fails, an error is returned and no partial state is created.
    [[nodiscard]] static Expected<FeatureManager, std::string> from_json(const std::string& json_str)
    {
        JsonValue root;
        try
        {
            root = parse_json(json_str);
        }
        catch (const std::exception& e)
        {
            return unexpected(std::string("JSON parse error: ") + e.what());
        }
        if (!root.is_object())
        {
            return unexpected("Root JSON must be an object");
        }
        const auto& obj = std::get<JsonObject>(root);
        FeatureManager manager;
        auto features_it = obj.find("features");
        if (features_it != obj.end())
        {
            if (!features_it->second.is_object())
            {
                return unexpected("'features' must be an object");
            }
            const auto& features_obj = std::get<JsonObject>(features_it->second);
            for (const auto& [name, value] : features_obj)
            {
                auto node_res = FeatureNode::from_json(value);
                if (!node_res)
                {
                    return unexpected("Error parsing feature '" + name + "': " + node_res.error());
                }
                manager.mFeatures[name] = std::move(*node_res);
            }
        }

        // Structural validation: all relationship targets must exist.
        for (const auto& [name, node] : manager.mFeatures)
        {
            for (const auto& [rel, targets] : node.relationships)
            {
                for (const auto& target : targets)
                {
                    if (!manager.mFeatures.count(target))
                    {
                        return unexpected("Feature '" + name + "' has relationship '" +
                                          std::string(EnumStringPolicy<FeatureRelationship>::to_string(rel)) +
                                          "' to missing feature '" + target + "'");
                    }
                }
            }
        }

        // Symmetrization: Ensure Conflicts and MutuallyExclusive relationships are bidirectional.
        // This handles hand-edited JSON where only one direction was specified.
        for (auto& [from_name, from_node] : manager.mFeatures)
        {
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                auto it = from_node.relationships.find(rel);
                if (it == from_node.relationships.end())
                {
                    continue;
                }
                for (const auto& to_name : it->second)
                {
                    // Add reverse relationship if not already present
                    auto& to_node = manager.mFeatures[to_name];
                    (void)to_node.relationships[rel].insert(from_name);
                }
            }
        }

        auto groups_it = obj.find("groups");
        if (groups_it != obj.end())
        {
            if (!groups_it->second.is_object())
            {
                return unexpected("'groups' must be an object");
            }
            const auto& groups_obj = std::get<JsonObject>(groups_it->second);
            for (const auto& [name, value] : groups_obj)
            {
                if (!value.is_array())
                {
                    return unexpected("Group '" + name + "' must be an array");
                }
                const auto& arr = std::get<JsonArray>(value);
                std::vector<std::string> feature_names;
                for (const auto& elem : arr)
                {
                    if (!elem.is_string())
                    {
                        return unexpected("Group feature must be string");
                    }
                    auto feature_name = std::get<std::string>(elem);
                    if (!manager.mFeatures.count(feature_name))
                    {
                        return unexpected("Group '" + name + "' references missing feature '" + feature_name + "'");
                    }
                    feature_names.push_back(std::move(feature_name));
                }
                manager.mGroups[name] = std::make_unique<FeatureGroupInfo<FeatureGroupState>>(feature_names);
            }
        }

        // Full validation: detect cycles and verify enabled-state invariants.
        // This catches invalid graphs that could cause problems during enable/disable.
        auto validate_res = manager.validate_unlocked();
        if (!validate_res)
        {
            return unexpected("Loaded graph fails validation: " + validate_res.error());
        }

        return manager;
    }

    // Export to GraphViz DOT format
    std::string to_dot() const
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        std::ostringstream ss;
        ss << "digraph FeatureGraph {\n";
        ss << "    rankdir=LR;\n";
        ss << "    node [shape=box];\n";
        for (const auto& [name, node] : mFeatures)
        {
            std::string color = node.enabled ? "green" : "gray";
            ss << "    \"" << name << "\" [style=filled, fillcolor=" << color << "];\n";
        }
        for (const auto& [name, node] : mFeatures)
        {
            for (const auto& [type, targets] : node.relationships)
            {
                std::string style;
                std::string arrow;
                switch (type)
                {
                    case FeatureRelationship::Requires:
                        style = "solid";
                        arrow = "normal";
                        break;
                    case FeatureRelationship::Implies:
                        style = "dashed";
                        arrow = "open";
                        break;
                    case FeatureRelationship::Conflicts:
                    case FeatureRelationship::MutuallyExclusive:
                        style = "dotted";
                        arrow = "none";
                        break;
                }
                for (const auto& target : targets)
                {
                    std::string_view type_str = EnumStringPolicy<FeatureRelationship>::to_string(type);
                    ss << "    \"" << name << "\" -> \"" << target << "\" [style=" << style << ", arrowhead=" << arrow
                       << ", label=\"" << type_str << "\"];\n";
                }
            }
        }
        ss << "}\n";
        return ss.str();
    }

    // Parse from DOT format (basic support)
    [[nodiscard]] static Expected<FeatureManager, std::string> from_dot(const std::string& dot_str)
    {
        FeatureManager manager;
        std::regex node_regex(R"(\"([^\"]+)\"\s*\[|([a-zA-Z_][a-zA-Z0-9_]*)\s*\[)");
        std::sregex_iterator nodes_begin(dot_str.begin(), dot_str.end(), node_regex);
        std::sregex_iterator nodes_end;
        for (auto it = nodes_begin; it != nodes_end; ++it)
        {
            std::string node_name = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();

            // Ignore DOT global attribute statements like: node [shape=box];
            if (!(*it)[1].matched)
            {
                if (node_name == "node" || node_name == "edge" || node_name == "graph")
                {
                    continue;
                }
            }

            auto res = manager.add_feature(node_name);
            if (!res && !manager.mFeatures.count(node_name))
            {
                return unexpected(res.error());
            }
        }
        std::regex edge_regex(R"(\"([^\"]+)\"\s*(?:->|--)\s*\"([^\"]+)\"\s*\[[^\]]*label\s*=\s*\"([^\"]+)\")");
        std::sregex_iterator edges_begin(dot_str.begin(), dot_str.end(), edge_regex);
        std::sregex_iterator edges_end;
        for (auto it = edges_begin; it != edges_end; ++it)
        {
            std::string from = (*it)[1].str();
            std::string to = (*it)[2].str();
            std::string label = (*it)[3].str();
            FeatureRelationship type;
            try
            {
                type = EnumStringPolicy<FeatureRelationship>::from_string(label);
            }
            catch (const std::invalid_argument&)
            {
                continue;
            }
            auto from_add = manager.add_feature(from);
            if (!from_add && !manager.mFeatures.count(from))
            {
                return unexpected(from_add.error());
            }
            auto to_add = manager.add_feature(to);
            if (!to_add && !manager.mFeatures.count(to))
            {
                return unexpected(to_add.error());
            }

            auto res = manager.add_relationship(from, type, to);
            if (!res)
            {
                return unexpected(res.error());
            }
        }
        return manager;
    }

    // Clear all features, groups, and observers
    void clear()
    {
        typename SyncPolicy::LockGuard guard(mSync.getLock());
        mFeatures.clear();
        mGroups.clear();
        mObservers.clear();
        batch_observers_.clear();
    }
};

} // namespace fat_p
