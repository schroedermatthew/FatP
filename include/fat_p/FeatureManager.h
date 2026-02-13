#pragma once

/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: public_header
  path: include/fat_p/FeatureManager.h
  namespace: fat_p::feature
  layer: Domain
  summary: "Runtime feature flag management with dependency resolution and conflict detection."
  api_stability: in_work
  related:
    docs_search: "FeatureManager"
    tests:
      - components/FeatureManager/tests/test_FeatureManager.cpp
    benchmarks:
      - components/FeatureManager/benchmarks/benchmark_FeatureManager.cpp
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

/**
 * @file FeatureManager.h
 * @brief Runtime feature flag management with compile-time optimization
 *
 * @details
 * A modern C++20 header-only library for managing feature flags with complex dependencies,
 * relationships, and validation. Designed for scenarios where features have interdependencies
 * (Requires, Implies, Conflicts, MutuallyExclusive) and need automatic resolution.
 * Key features:
 * - Cycle detection with detailed error messages showing full dependency path
 * - Pluggable thread-safety policies (single-threaded, mutex, spinlock, shared_mutex)
 * - Type-safe group states with custom enums
 * - Observer pattern with priority ordering and RAII lifetime management
 * - JSON serialization and GraphViz DOT export
 * - RAII helpers for scoped state changes
 * - Optimized with FlatSet for relationship storage (cache-friendly sorted vectors)
 * Performance characteristics:
 * - Add feature: O(log n)
 * - Enable/disable: O(d x log n) where d = dependency depth (limited to kMaxValidationDepth)
 * - Validate: O(n x d x log n)
 * - Memory: ~550 bytes per feature with 5 relationships (using FlatSet)
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ConcurrencyPolicies.h"
#include "EnumPlus.h"
#include "Expected.h"
#include "Factory.h"
#include "JsonLite.h"
#include "FastHashMap.h"
#include "FlatSet.h"
#include "Stringify.h"
#include "ValueGuard.h"

namespace fat_p
{
namespace feature
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

// Number of relationship types — used for array-based storage in FeatureNode
inline constexpr size_t kRelationshipCount = 4;

// Convert FeatureRelationship enum to array index (0–3)
constexpr size_t relIdx(FeatureRelationship r) noexcept
{
    return static_cast<size_t>(r);
}

// Default built-in group state enum
enum class FeatureGroupState
{
    Inactive, // No features enabled
    Partial,  // Some but not all features enabled/valid
    Active,   // All required features enabled, no conflicts, all checks pass
    Invalid   // Conflicts or failed checks
};

} // namespace feature

// ============================================================================
// EnumPlus Specializations (must be in fat_p::, same namespace as primary templates)
// ============================================================================

template <>
struct EnableOverloadedOperators<feature::FeatureRelationship>
{
    static constexpr bool value = true;
};

template <>
struct EnumStringPolicy<feature::FeatureRelationship>
{
    static constexpr std::array<std::string_view, 4> names = {"Requires", "Conflicts", "Implies", "MutuallyExclusive"};

    static std::string_view to_string(feature::FeatureRelationship e)
    {
        return names[static_cast<size_t>(e)];
    }

    static feature::FeatureRelationship from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid FeatureRelationship string");
        }
        return static_cast<feature::FeatureRelationship>(std::distance(names.begin(), it));
    }
};

template <>
struct EnumStringPolicy<feature::FeatureGroupState>
{
    static constexpr std::array<std::string_view, 4> names = {"Inactive", "Partial", "Active", "Invalid"};

    static std::string_view to_string(feature::FeatureGroupState e)
    {
        return names[static_cast<size_t>(e)];
    }

    static feature::FeatureGroupState from_string(std::string_view str)
    {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end())
        {
            throw std::invalid_argument("Invalid FeatureGroupState string");
        }
        return static_cast<feature::FeatureGroupState>(std::distance(names.begin(), it));
    }
};

namespace feature
{

// ============================================================================
// Type Aliases and Forward Declarations
// ============================================================================

template <typename SyncPolicy>
class FeatureManager;

// Function object for feature validation checks
//
// CONTRACT:
// - FeatureCheck callbacks are invoked while the FeatureManager holds its internal lock.
// - Callbacks MUST NOT call any FeatureManager methods (isEnabled, enable, disable, etc.)
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
    compute(const FlatSet<std::string>& group_features, size_t enabled_count, bool has_conflict, bool all_checks_pass)
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
using StateComputer = std::function<StateEnum(const FlatSet<std::string>&, size_t, bool, bool)>;

// ============================================================================
// FeatureCheck Callback Factory
// ============================================================================

// Global factory for FeatureCheck callbacks
// Key: string identifier, Product: FeatureCheck
using FeatureCheckFactory = SimpleFactory<std::string, FeatureCheck>;

// Singleton accessor for the global FeatureCheck factory
inline FeatureCheckFactory& getFeatureCheckFactory()
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
        mRegistered = getFeatureCheckFactory().registerType(mKey, std::move(creator));
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
            [[maybe_unused]] bool unregistered = getFeatureCheckFactory().unregisterType(mKey);
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
                (void)getFeatureCheckFactory().unregisterType(mKey);
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

    // Relationship storage: fixed-size array indexed by FeatureRelationship (0–3).
    // Each slot holds a FlatSet of target feature names. Empty slots indicate no
    // relationships of that type. Eliminates map overhead for a fixed 4-key domain.
    std::array<FlatSet<std::string>, kRelationshipCount> relationships;

    JsonValue toJson() const
    {
        JsonObject obj;
        obj["enabled"] = JsonValue{enabled};
        if (!check_key.empty())
        {
            obj["check_key"] = JsonValue{check_key};
        }
        for (size_t ri = 0; ri < kRelationshipCount; ++ri)
        {
            const auto& targets = relationships[ri];
            if (targets.empty())
            {
                continue;
            }
            JsonArray arr;
            for (const auto& target : targets)
            {
                arr.push_back(JsonValue{target});
            }
            auto type = static_cast<FeatureRelationship>(ri);
            std::string type_name(EnumStringPolicy<FeatureRelationship>::to_string(type));
            obj[type_name] = JsonValue{std::move(arr)};
        }
        return JsonValue{std::move(obj)};
    }

    static Expected<FeatureNode, std::string> fromJson(const JsonValue& value)
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
                auto check_result = getFeatureCheckFactory().make(node.check_key);
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
                    node.relationships[relIdx(type)].insert(std::get<std::string>(elem));
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
    virtual FlatSet<std::string> get_features() const = 0;
    virtual JsonValue toJson() const = 0;
    virtual std::string stateToString() const = 0;

    // Type-erased state computation: invokes the concrete StateComputer and
    // caches the result. Returns the computed state as an int ordinal so the
    // caller never needs to downcast.
    virtual int computeAndCache(size_t enabled_count, bool has_conflict, bool all_checks_pass) = 0;
};

// Concrete group info with type-safe state computer
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupInfo : public FeatureGroupInfoBase
{
    FlatSet<std::string> features;
    StateComputer<StateEnum> state_computer;
    mutable std::atomic<StateEnum> cached_state;

    FeatureGroupInfo(const std::vector<std::string>& f,
                     StateComputer<StateEnum> comp = FeatureGroupStatePolicy<StateEnum>::compute)
        : features(f.begin(), f.end())
        , state_computer(comp)
        , cached_state(static_cast<StateEnum>(FeatureGroupState::Inactive))
    {
    }

    FlatSet<std::string> get_features() const override
    {
        return features;
    }

    JsonValue toJson() const override
    {
        JsonArray arr;
        for (const auto& f : features)
        {
            arr.push_back(JsonValue{f});
        }
        return JsonValue{std::move(arr)};
    }

    std::string stateToString() const override
    {
        if constexpr (named_enum<StateEnum>)
        {
            return std::string(EnumStringPolicy<StateEnum>::to_string(
                cached_state.load(std::memory_order_relaxed)));
        }
        else
        {
            return toString(cached_state.load(std::memory_order_relaxed));
        }
    }

    int computeAndCache(size_t enabled_count, bool has_conflict, bool all_checks_pass) override
    {
        StateEnum state = state_computer(features, enabled_count, has_conflict, all_checks_pass);
        cached_state.store(state, std::memory_order_relaxed);
        return static_cast<int>(state);
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

    FastHashMap<std::string, FeatureNode> mFeatures;
    FastHashMap<std::string, std::unique_ptr<FeatureGroupInfoBase>> mGroups;
    std::vector<ObserverEntry> mObservers;
    std::vector<BatchObserverEntry> mBatchObservers;
    ObserverId mNextObserverId = 1;
    mutable SyncPolicy mSync;

    // Maximum dependency depth before aborting to prevent stack overflow
    //
    // Rationale for kMaxValidationDepth = 100:
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
    static constexpr size_t kMaxValidationDepth = 100;

    Expected<FeatureNode*, std::string> getNode(const std::string& name)
    {
        auto* ptr = mFeatures.find(name);
        if (!ptr)
        {
            return unexpected("Feature not found: " + name);
        }
        return ptr;
    }

    Expected<const FeatureNode*, std::string> getNode(const std::string& name) const
    {
        auto* ptr = mFeatures.find(name);
        if (!ptr)
        {
            return unexpected("Feature not found: " + name);
        }
        return ptr;
    }

    // Add a relationship between two features (lock must already be held)
    [[nodiscard]] Expected<void, std::string>
    addRelationshipUnlocked(const std::string& from, FeatureRelationship type, const std::string& to)
    {
        auto from_res = getNode(from);
        if (!from_res)
        {
            return unexpected("Feature not found: " + from + " in relationship setup");
        }
        auto to_res = getNode(to);
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
        from_node->relationships[relIdx(type)].insert(to);

        // Bidirectional for conflicts and mutually exclusive
        if (type == FeatureRelationship::Conflicts || type == FeatureRelationship::MutuallyExclusive)
        {
            FeatureNode* to_node = *to_res;
            to_node->relationships[relIdx(type)].insert(from);
        }
        return {};
    }

    // Build a human-readable cycle path from the enabling chain
    // The enabling_chain preserves insertion order (the actual traversal path)
    // Example: enabling_chain = {"A", "B", "C"}, target = "A"
    // Returns: "A -> B -> C -> A"
    std::string buildCyclePath(const std::vector<std::string>& enabling_chain, const std::string& target) const
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
    [[nodiscard]] Expected<void, std::string> detectCyclesUnlocked() const
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
            if (static_cast<size_t>(depth) > kMaxValidationDepth)
            {
                return unexpected("Maximum dependency depth exceeded at feature: " + name);
            }

            auto it = state.find(name);
            if (it != state.end())
            {
                if (it->second == VisitState::Visiting)
                {
                    return unexpected("Circular dependency detected: " + buildCyclePath(stack, name));
                }
                if (it->second == VisitState::Visited)
                {
                    return {};
                }
            }

            state[name] = VisitState::Visiting;
            stack.push_back(name);

            auto* node_ptr = mFeatures.find(name);
            if (!node_ptr)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return unexpected("Feature not found: " + name);
            }
            const FeatureNode& node = *node_ptr;

            auto visit_relationship = [&](FeatureRelationship rel) -> Expected<void, std::string> {
                const auto& targets = node.relationships[relIdx(rel)];
                if (targets.empty())
                {
                    return {};
                }
                for (const auto& dep : targets)
                {
                    auto dep_state = state.find(dep);
                    if (dep_state != state.end() && dep_state->second == VisitState::Visiting)
                    {
                        return unexpected("Circular dependency detected: " + buildCyclePath(stack, dep));
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
    [[nodiscard]] Expected<void, std::string> validateUnlocked() const
    {
        // --------------------------------------------------------------------
        // 1) Structural validation: every relationship target must exist.
        // --------------------------------------------------------------------
        for (const auto& [name, node] : mFeatures)
        {
            for (size_t ri = 0; ri < kRelationshipCount; ++ri)
            {
                const auto& targets = node.relationships[ri];
                auto rel = static_cast<FeatureRelationship>(ri);
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
        auto cycle_res = detectCyclesUnlocked();
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
            const auto& requires_targets = node.relationships[relIdx(FeatureRelationship::Requires)];
            if (!requires_targets.empty())
            {
                for (const auto& required : requires_targets)
                {
                    auto* req_ptr = mFeatures.find(required);
                    if (!req_ptr)
                    {
                        return unexpected("Required feature not found: " + required);
                    }
                    if (!req_ptr->enabled)
                    {
                        return unexpected("'" + name + "' requires '" + required + "' but it's disabled");
                    }
                }
            }

            // Implies: enabled feature must have all implied features enabled
            const auto& implies_targets = node.relationships[relIdx(FeatureRelationship::Implies)];
            if (!implies_targets.empty())
            {
                for (const auto& implied : implies_targets)
                {
                    auto* impl_ptr = mFeatures.find(implied);
                    if (!impl_ptr)
                    {
                        return unexpected("Implied feature not found: " + implied);
                    }
                    if (!impl_ptr->enabled)
                    {
                        return unexpected("'" + name + "' implies '" + implied + "' but it's disabled");
                    }
                }
            }

            // Conflicts and MutuallyExclusive
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                const auto& conflict_targets = node.relationships[relIdx(rel)];
                if (conflict_targets.empty())
                {
                    continue;
                }

                for (const auto& other : conflict_targets)
                {
                    auto* other_ptr = mFeatures.find(other);
                    if (!other_ptr)
                    {
                        if (rel == FeatureRelationship::Conflicts)
                        {
                            return unexpected("Conflicting feature not found: " + other);
                        }
                        return unexpected("Mutually exclusive feature not found: " + other);
                    }
                    if (other_ptr->enabled)
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

    Expected<void, std::string> enableFeature(const std::string& name,
                                               std::vector<std::string>& enabling_chain,
                                               std::unordered_set<std::string>& chain_set,
                                               std::vector<std::string>* changed_features,
                                               int depth = 0)
    {
        if (static_cast<size_t>(depth) > kMaxValidationDepth)
        {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }

        // O(1) membership test via hash set; vector is kept for path reconstruction only
        auto in_chain = [&chain_set](const std::string& n) {
            return chain_set.count(n) != 0;
        };

        // Check for circular dependencies first
        if (in_chain(name))
        {
            return unexpected("Circular dependency detected: " + buildCyclePath(enabling_chain, name));
        }

        auto node_res = getNode(name);
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
        chain_set.insert(name);

        // RAII guard to ensure enabling_chain and chain_set stay consistent on scope exit.
        struct ChainGuard
        {
            std::vector<std::string>& chain;
            std::unordered_set<std::string>& set;
            bool dismissed = false;
            ChainGuard(std::vector<std::string>& c, std::unordered_set<std::string>& s)
                : chain(c)
                , set(s)
            {
            }
            ~ChainGuard()
            {
                if (!dismissed)
                {
                    set.erase(chain.back());
                    chain.pop_back();
                }
            }
            void dismiss()
            {
                dismissed = true;
            }
        } chain_guard(enabling_chain, chain_set);

        // Enable this feature first (may be rolled back on error)
        bool was_enabled = node->enabled;
        node->enabled = true;

        // Process Required relationships (recursively enable dependencies)
        {
            const auto& targets = node->relationships[relIdx(FeatureRelationship::Requires)];
            for (const auto& required : targets)
            {
                // Check for circular dependency before recursing
                if (in_chain(required))
                {
                    node->enabled = was_enabled;
                    return unexpected("Circular dependency detected: " + buildCyclePath(enabling_chain, required));
                }

                auto req_node_res = getNode(required);
                if (!req_node_res)
                {
                    node->enabled = false;
                    return unexpected("Required feature not found: " + required);
                }
                FeatureNode* req_node = *req_node_res;
                if (!req_node->enabled)
                {
                    auto enable_res = enableFeature(required, enabling_chain, chain_set, changed_features, depth + 1);
                    if (!enable_res)
                    {
                        node->enabled = false;
                        return enable_res;
                    }
                }
            }
        }

        // Process Implies relationships
        {
            const auto& targets = node->relationships[relIdx(FeatureRelationship::Implies)];
            for (const auto& implied : targets)
            {
                // Check for circular dependency before checking if already enabled
                if (in_chain(implied))
                {
                    node->enabled = false;
                    return unexpected("Circular dependency detected: " + buildCyclePath(enabling_chain, implied));
                }

                auto impl_node_res = getNode(implied);
                if (!impl_node_res)
                {
                    node->enabled = false;
                    return unexpected("Implied feature not found: " + implied);
                }

                FeatureNode* impl_node = *impl_node_res;
                if (!impl_node->enabled)
                {
                    auto enable_res = enableFeature(implied, enabling_chain, chain_set, changed_features, depth + 1);
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
            {
                const auto& targets = node->relationships[relIdx(type)];
                for (const auto& conflicting : targets)
                {
                    auto conf_node_res = getNode(conflicting);
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

    static void sortObserversByPriority(std::vector<ObserverEntry>& entries)
    {
        // Stable sort preserves insertion order for observers with equal priority.
        std::stable_sort(entries.begin(), entries.end(), [](const ObserverEntry& a, const ObserverEntry& b) {
            return a.priority > b.priority;
        });
    }

    static void sortBatchObserversByPriority(std::vector<BatchObserverEntry>& entries)
    {
        std::stable_sort(entries.begin(), entries.end(), [](const BatchObserverEntry& a, const BatchObserverEntry& b) {
            return a.priority > b.priority;
        });
    }

    static void notifyObserversSorted(const std::vector<ObserverEntry>& sorted,
                                        const std::string& feature_name,
                                        bool new_state,
                                        bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(feature_name, new_state, success);
        }
    }

    static void notifyBatchObserversSorted(const std::vector<BatchObserverEntry>& sorted,
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
    Expected<StateEnum, std::string> computeGroupStateImpl(const std::string& group_name) const
    {
        auto* group_uptr = mGroups.find(group_name);
        if (!group_uptr)
        {
            return unexpected("Group not found: " + group_name);
        }
        FeatureGroupInfoBase* group = group_uptr->get();
        const auto group_features = group->get_features();
        size_t enabled_count = 0;
        bool has_conflict = false;
        bool all_checks_pass = true;
        for (const auto& feature_name : group_features)
        {
            auto node_res = getNode(feature_name);
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
                    if (node->relationships[relIdx(FeatureRelationship::Conflicts)].count(other) > 0)
                    {
                        auto other_node_res = getNode(other);
                        if (other_node_res && (*other_node_res)->enabled)
                        {
                            has_conflict = true;
                        }
                    }
                    if (node->relationships[relIdx(FeatureRelationship::MutuallyExclusive)].count(other) > 0)
                    {
                        auto other_node_res = getNode(other);
                        if (other_node_res && (*other_node_res)->enabled)
                        {
                            has_conflict = true;
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
        // Virtual dispatch: the concrete FeatureGroupInfo<StateEnum> invokes its
        // typed state_computer and caches the result. No dynamic_cast needed.
        int ordinal = group->computeAndCache(enabled_count, has_conflict, all_checks_pass);
        return static_cast<StateEnum>(ordinal);
    }

public:
    FeatureManager() = default;

    FeatureManager(FeatureManager&& other) noexcept
        : mFeatures(std::move(other.mFeatures))
        , mGroups(std::move(other.mGroups))
        , mObservers(std::move(other.mObservers))
        , mBatchObservers(std::move(other.mBatchObservers))
        , mNextObserverId(other.mNextObserverId)
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
            mBatchObservers = std::move(other.mBatchObservers);
            mNextObserverId = other.mNextObserverId;
            // mSync intentionally not assigned/moved: synchronization primitives (mutexes)
            // are not safely movable. The target keeps its existing mSync instance.
        }
        return *this;
    }

    // RAII helper for temporary feature changes.
    //
    // Construction routes through the validated enable/disable path (conflict checking,
    // dependency resolution, implies propagation). If validation fails, the object is
    // constructed but marked invalid — check via valid() or operator bool().
    //
    // Destruction restores only the features that this guard actually changed, and
    // only if they are still in the state the guard set them to. This prevents
    // concurrent legitimate state changes from being silently reverted by an
    // unrelated guard's destructor.
    //
    // Observer notifications fire on rollback (outside the lock) with the inverse
    // direction of the original operation.
    class ScopedFeatureChange
    {
    private:
        FeatureManager* mManager;
        std::string mRequestedFeature;          // the feature the caller asked to toggle
        std::vector<std::string> mChangedFeatures; // features this guard actually toggled
        bool mNewState;                         // direction: true = enabled, false = disabled
        bool mValid;

    public:
        ScopedFeatureChange(FeatureManager& manager, const std::string& feature_name, bool new_state)
            : mManager(&manager)
            , mRequestedFeature(feature_name)
            , mNewState(new_state)
            , mValid(false)
        {
            // Snapshot feature states before the operation to detect what changed.
            FastHashMap<std::string, bool> pre_states;
            {
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    pre_states[name] = node.enabled;
                }
            }

            // Use the validated enable/disable path: conflict checking, dependency
            // resolution, and implies propagation all apply. batchEnable/batchDisable
            // handles its own locking internally.
            Expected<void, std::string> result;
            if (new_state)
            {
                result = mManager->enable(feature_name);
            }
            else
            {
                result = mManager->disable(feature_name);
            }

            mValid = result.has_value();

            if (mValid)
            {
                // Determine which features actually changed state by diffing
                // pre-operation snapshot against current state.
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    auto* pre = pre_states.find(name);
                    if (pre && *pre != node.enabled)
                    {
                        mChangedFeatures.push_back(name);
                    }
                }
            }
        }

        ~ScopedFeatureChange()
        {
            if (!mValid || mChangedFeatures.empty())
            {
                return;
            }

            std::vector<std::string> restored_features;
            std::vector<ObserverEntry> observers_snapshot;
            std::vector<BatchObserverEntry> batch_observers_snapshot;

            {
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& name : mChangedFeatures)
                {
                    auto node_res = mManager->getNode(name);
                    if (!node_res)
                    {
                        continue;
                    }
                    FeatureNode* node = *node_res;

                    // Restore only if still in the state we set it to.
                    // If another thread changed it, respect their change.
                    if (node->enabled == mNewState)
                    {
                        node->enabled = !mNewState;
                        restored_features.push_back(name);
                    }
                }

                if (!restored_features.empty())
                {
                    observers_snapshot = mManager->mObservers;
                    batch_observers_snapshot = mManager->mBatchObservers;
                }
            } // Lock released before observer notification

            if (!restored_features.empty())
            {
                sortObserversByPriority(observers_snapshot);
                for (const auto& feature : restored_features)
                {
                    notifyObserversSorted(observers_snapshot, feature, !mNewState, true);
                }

                if (!batch_observers_snapshot.empty())
                {
                    sortBatchObserversByPriority(batch_observers_snapshot);
                    notifyBatchObserversSorted(batch_observers_snapshot,
                                                  mRequestedFeature,
                                                  restored_features,
                                                  !mNewState,
                                                  true);
                }
            }
        }

        /// Returns true if the scoped change was applied successfully.
        /// A false return means enable/disable failed validation (conflict, missing
        /// dependency, etc.) and the feature state is unchanged.
        bool valid() const
        {
            return mValid;
        }

        explicit operator bool() const
        {
            return mValid;
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
            , mId(manager.addObserver(std::move(callback), priority))
        {
        }

        ~ScopedObserver()
        {
            if (mManager && mId != 0)
            {
                mManager->removeObserver(mId);
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
                    mManager->removeObserver(mId);
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
            , mId(manager.addBatchObserver(std::move(callback), priority))
        {
        }

        ~ScopedBatchObserver()
        {
            if (mManager && mId != 0)
            {
                mManager->removeObserver(mId);
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
                    mManager->removeObserver(mId);
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
    [[nodiscard]] Expected<void, std::string> addFeature(const std::string& name, FeatureCheck check = nullptr)
    {
        [[maybe_unused]] auto guard = mSync.lock();
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
    [[nodiscard]] Expected<void, std::string> addFeature(const std::string& name, const std::string& check_key)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        if (mFeatures.count(name))
        {
            return unexpected("Feature already exists: " + name);
        }

        // Look up the check from factory
        auto check_result = getFeatureCheckFactory().make(check_key);
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
    addRelationship(const std::string& from, FeatureRelationship type, const std::string& to)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return addRelationshipUnlocked(from, type, to);
    }

    // Add a feature group with optional custom state computer
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string>
    addGroup(const std::string& group_name,
              const std::vector<std::string>& feature_names,
              StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        [[maybe_unused]] auto guard = mSync.lock();
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
    addMutuallyExclusiveGroup(const std::string& group_name,
                                 const std::vector<std::string>& feature_names,
                                 StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        [[maybe_unused]] auto guard = mSync.lock();

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
                auto res = addRelationshipUnlocked(feature_names[i],
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
    [[nodiscard]] Expected<StateEnum, std::string> getGroupState(const std::string& group_name) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return computeGroupStateImpl<StateEnum>(group_name);
    }

    // Get features in a group
    [[nodiscard]] Expected<FlatSet<std::string>, std::string> getGroupFeatures(const std::string& group_name) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        auto* group_uptr = mGroups.find(group_name);
        if (!group_uptr)
        {
            return unexpected("Group not found: " + group_name);
        }
        return (*group_uptr)->get_features();
    }

    // Enable a feature with full transactional semantics
    // If enabling fails (e.g., due to conflicts), no dependencies are left enabled
    [[nodiscard]] Expected<void, std::string> enable(const std::string& name)
    {
        return batchEnable({name});
    }

    // Disable a feature (Transactional)
    //
    // Delegates to batchDisable to ensure safety constraints (Requires/Implies)
    // are checked and all side effects are handled atomically.
    [[nodiscard]] Expected<void, std::string> disable(const std::string& name)
    {
        return batchDisable({name});
    }

    // Batch enable multiple features with transactional semantics
    // All features (including dependencies) succeed or all changes are rolled back.
    // NOTIFICATIONS ARE DEFERRED until after lock release to prevent deadlocks.
    [[nodiscard]] Expected<void, std::string> batchEnable(const std::vector<std::string>& names)
    {
        std::vector<std::string> all_changed;
        std::vector<ObserverEntry> observers_snapshot;
        std::vector<BatchObserverEntry> batch_observers_snapshot;

        { // Scope for LockGuard - Lock held only during state modification
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all features exist first
            for (const auto& name : names)
            {
                auto node_res = getNode(name);
                if (!node_res)
                {
                    return unexpected(node_res.error());
                }
            }

            // Snapshot ALL feature states before any modifications
            FastHashMap<std::string, bool> original_states;
            for (const auto& [name, node] : mFeatures)
            {
                original_states[name] = node.enabled;
            }

            // Attempt to enable each feature
            for (const auto& name : names)
            {
                std::vector<std::string> chain;
                std::unordered_set<std::string> chain_set;
                auto res = enableFeature(name, chain, chain_set, &all_changed);
                if (!res)
                {
                    // Rollback ALL features to original states
                    for (auto&& [feature_name, node] : mFeatures)
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
                batch_observers_snapshot = mBatchObservers;
            }
        } // Lock released here

        if (!all_changed.empty())
        {
            // Notify observers safely outside the lock
            sortObserversByPriority(observers_snapshot);
            for (const auto& feature : all_changed)
            {
                notifyObserversSorted(observers_snapshot, feature, true, true);
            }

            // Notify batch observers
            if (!batch_observers_snapshot.empty())
            {
                sortBatchObserversByPriority(batch_observers_snapshot);
                notifyBatchObserversSorted(batch_observers_snapshot,
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
    [[nodiscard]] Expected<void, std::string> batchDisable(const std::vector<std::string>& names)
    {
        std::vector<std::string> actually_changed;
        std::vector<ObserverEntry> observers_snapshot;
        std::vector<BatchObserverEntry> batch_observers_snapshot;

        // Deduplicate requested names while preserving first-seen order.
        // This prevents incorrect rollback when the same feature appears multiple times.
        std::vector<std::string> unique_names;
        unique_names.reserve(names.size());
        std::unordered_set<std::string> disabled_set;
        for (const auto& n : names)
        {
            if (disabled_set.insert(n).second)
            {
                unique_names.push_back(n);
            }
        }

        { // Scope for LockGuard - Lock held only during state modification
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all features exist first
            for (const auto& name : unique_names)
            {
                auto node_res = getNode(name);
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
                auto node_res = getNode(name);
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
                    auto n = getNode(unique_names[i]);
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
                for (const auto& required : node.relationships[relIdx(FeatureRelationship::Requires)])
                {
                    auto req_node = getNode(required);
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

                // Check if this enabled feature implies any of the disabled features
                // If A implies B and A is enabled, then B cannot be disabled
                for (const auto& implied : node.relationships[relIdx(FeatureRelationship::Implies)])
                {
                    if (disabled_set.count(implied))
                    {
                        rollback();
                        return unexpected("Cannot disable '" + implied + "': implied by enabled feature '" +
                                          feature_name + "'. Disable '" + feature_name + "' first.");
                    }
                }
            }

            if (!actually_changed.empty())
            {
                observers_snapshot = mObservers;
                batch_observers_snapshot = mBatchObservers;
            }
        } // Lock released here

        if (!actually_changed.empty())
        {
            // Notify observers safely outside the lock
            sortObserversByPriority(observers_snapshot);
            for (const auto& feature : actually_changed)
            {
                notifyObserversSorted(observers_snapshot, feature, false, true);
            }

            // Notify batch observers
            if (!batch_observers_snapshot.empty())
            {
                sortBatchObserversByPriority(batch_observers_snapshot);
                notifyBatchObserversSorted(batch_observers_snapshot,
                                              names.empty() ? "" : names[0],
                                              actually_changed,
                                              false,
                                              true);
            }
        }

        return {};
    }

    // Check if feature is enabled
    [[nodiscard]] bool isEnabled(const std::string& name) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        auto node_res = getNode(name);
        if (!node_res)
        {
            return false;
        }
        return (*node_res)->enabled;
    }

    // Validate entire feature set
    [[nodiscard]] Expected<void, std::string> validate()
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return validateUnlocked();
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
    ObserverId addObserver(FeatureObserver callback, int priority = 0)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        ObserverId id = mNextObserverId++;
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
    //   manager.addBatchObserver([](auto requested, auto all_changed, auto enabled, auto ok) {
    //       std::cout << "Requested: " << requested << "\n";
    //       std::cout << "All changed: ";
    //       for (const auto& f : all_changed) std::cout << f << " ";
    //       std::cout << "\n";
    //   });
    //
    // Returns: ObserverId that can be used to remove the observer later
    ObserverId addBatchObserver(BatchObserver callback, int priority = 0)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        ObserverId id = mNextObserverId++;
        mBatchObservers.push_back({id, priority, std::move(callback)});
        return id;
    }

    // Remove an observer by ID
    //
    // Returns true if an observer with the given ID was found and removed,
    // false if no observer with that ID exists.
    //
    // Works for both regular and batch observers.
    bool removeObserver(ObserverId id)
    {
        [[maybe_unused]] auto guard = mSync.lock();

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
            std::find_if(mBatchObservers.begin(), mBatchObservers.end(), [id](const BatchObserverEntry& entry) {
                return entry.id == id;
            });
        if (bit != mBatchObservers.end())
        {
            mBatchObservers.erase(bit);
            return true;
        }

        return false;
    }

    // Remove all observers
    void clearObservers()
    {
        [[maybe_unused]] auto guard = mSync.lock();
        mObservers.clear();
        mBatchObservers.clear();
    }

    // Get all enabled features
    std::vector<std::string> getEnabled() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
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
    [[nodiscard]] std::vector<std::string> getAllFeatures() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        std::vector<std::string> all_features;
        for (const auto& [name, _] : mFeatures)
        {
            all_features.push_back(name);
        }
        return all_features;
    }

    // Get all group names
    [[nodiscard]] std::vector<std::string> getAllGroups() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        std::vector<std::string> all_groups;
        for (const auto& [name, _] : mGroups)
        {
            all_groups.push_back(name);
        }
        return all_groups;
    }

    // Serialize to JSON
    [[nodiscard]] std::string toJson() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        JsonObject root;
        JsonObject features_json;
        for (const auto& [name, node] : mFeatures)
        {
            features_json[name] = node.toJson();
        }
        root["features"] = JsonValue{std::move(features_json)};
        JsonObject groups_json;
        for (const auto& [name, group] : mGroups)
        {
            groups_json[name] = group->toJson();
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
    [[nodiscard]] static Expected<FeatureManager, std::string> fromJson(const std::string& json_str)
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
                auto node_res = FeatureNode::fromJson(value);
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
            for (size_t ri = 0; ri < kRelationshipCount; ++ri)
            {
                const auto& targets = node.relationships[ri];
                auto rel = static_cast<FeatureRelationship>(ri);
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
        for (auto&& [from_name, from_node] : manager.mFeatures)
        {
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                const auto& targets = from_node.relationships[relIdx(rel)];
                if (targets.empty())
                {
                    continue;
                }
                for (const auto& to_name : targets)
                {
                    // Add reverse relationship if not already present.
                    // to_name is validated to exist by the relationship check above.
                    auto* to_node_ptr = manager.mFeatures.find(to_name);
                    (void)to_node_ptr->relationships[relIdx(rel)].insert(from_name);
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
        auto validate_res = manager.validateUnlocked();
        if (!validate_res)
        {
            return unexpected("Loaded graph fails validation: " + validate_res.error());
        }

        return manager;
    }

    // Export to GraphViz DOT format
    [[nodiscard]] std::string toDot() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
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
            for (size_t ri = 0; ri < kRelationshipCount; ++ri)
            {
                const auto& targets = node.relationships[ri];
                if (targets.empty())
                {
                    continue;
                }
                auto type = static_cast<FeatureRelationship>(ri);
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

    // Clear all features, groups, and observers
    void clear()
    {
        [[maybe_unused]] auto guard = mSync.lock();
        mFeatures.clear();
        mGroups.clear();
        mObservers.clear();
        mBatchObservers.clear();
    }
};

} // namespace feature
} // namespace fat_p
