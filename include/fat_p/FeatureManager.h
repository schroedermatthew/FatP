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
#include "FastHashMap.h"
#include "FlatSet.h"
#include "JsonLite.h"
#include "Stringify.h"
#include "ValueGuard.h"

namespace fat_p
{
namespace feature
{

// ============================================================================
// Enum Definitions
// ============================================================================

/// @brief Directed relationship types between features.
enum class FeatureRelationship
{
    Requires,         ///< Source feature requires target to be enabled.
    Conflicts,        ///< Source and target cannot both be enabled (symmetric).
    Implies,          ///< Enabling source automatically enables target.
    MutuallyExclusive ///< Group constraint: all members conflict with each other.
};

/// @brief Number of FeatureRelationship enumerators; used for array-based storage.
inline constexpr size_t kRelationshipCount = 4;

/// @brief Converts FeatureRelationship enum to array index (0–3).
constexpr size_t relIdx(FeatureRelationship r) noexcept
{
    return static_cast<size_t>(r);
}

/// @brief Default group state computed by FeatureGroupStatePolicy.
enum class FeatureGroupState
{
    Inactive, ///< No features in the group are enabled.
    Partial,  ///< Some but not all features are enabled.
    Active,   ///< All features enabled, no conflicts, all checks pass.
    Invalid   ///< Conflicts detected or check callbacks failed.
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

/**
 * @brief Validation callback invoked during feature validation.
 *
 * Called while the FeatureManager holds its internal lock. Must not call back
 * into the manager (deadlock risk) and should be fast and non-blocking.
 * Return Expected<void> on success, or an error string on failure.
 *
 * @warning Long-running operations (network, disk I/O) block all feature operations.
 * @see addFeature(), FeatureCheckRegistration
 */
using FeatureCheck = std::function<Expected<void, std::string>()>;

/// @brief Observer callback invoked on each individual feature state change.
using FeatureObserver = std::function<void(const std::string& featureName, bool newState, bool success)>;

/// @brief Unique identifier for registered observers; used with removeObserver().
using ObserverId = std::uint64_t;

/**
 * @brief Observer callback invoked once per enable/disable operation with all changed features.
 *
 * @param requestedFeature The feature the caller explicitly enabled/disabled.
 * @param allChanged       All features that changed state (includes implicit dependencies).
 * @param enabled          true if features were enabled, false if disabled.
 * @param success          true if the operation succeeded.
 */
using BatchObserver = std::function<void(const std::string& requestedFeature,
                                         const std::vector<std::string>& allChanged,
                                         bool enabled,
                                         bool success)>;

/**
 * @brief Default policy for computing feature group state from member feature states.
 *
 * @tparam StateEnum Enum type to return (default: FeatureGroupState).
 */
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupStatePolicy
{
    using state_type = StateEnum;
    static state_type
    compute(const FlatSet<std::string>& groupFeatures, size_t enabledCount, bool hasConflict, bool allChecksPass)
    {
        if (groupFeatures.empty())
        {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (hasConflict || !allChecksPass)
        {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (enabledCount == 0)
        {
            return static_cast<state_type>(FeatureGroupState::Inactive);
        }
        if (enabledCount < groupFeatures.size())
        {
            return static_cast<state_type>(FeatureGroupState::Partial);
        }
        return static_cast<state_type>(FeatureGroupState::Active);
    }
};

/// @brief Function type for custom group state computation.
template <typename StateEnum>
using StateComputer = std::function<StateEnum(const FlatSet<std::string>&, size_t, bool, bool)>;

// ============================================================================
// FeatureCheck Callback Factory
// ============================================================================

/// @brief Factory type mapping string keys to FeatureCheck constructors.
using FeatureCheckFactory = SimpleFactory<std::string, FeatureCheck>;

/// @brief Returns the process-wide singleton FeatureCheckFactory.
inline FeatureCheckFactory& getFeatureCheckFactory()
{
    static FeatureCheckFactory factory;
    return factory;
}

/**
 * @brief RAII guard for automatic factory registration/unregistration of FeatureCheck callbacks.
 *
 * Allows modules to register their checks independently and have them
 * automatically cleaned up when the registration object goes out of scope.
 *
 * @note Non-copyable, moveable.
 * @see getFeatureCheckFactory()
 */
class FeatureCheckRegistration
{
public:
    /**
     * @brief Registers a FeatureCheck creator function with the given key.
     *
     * If registration fails (e.g., key already registered), this object does
     * not take ownership and will not unregister on destruction.
     *
     * @param key     Factory key for lookup during deserialization.
     * @param creator Factory function returning a FeatureCheck.
     */
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

/// @brief Internal representation of a feature with its state, check, and relationships.
struct FeatureNode
{
    bool enabled = false;
    FeatureCheck check;
    std::string checkKey; // For serialization: the factory key to restore check on load

    // Relationship storage: fixed-size array indexed by FeatureRelationship (0–3).
    // Each slot holds a FlatSet of target feature names. Empty slots indicate no
    // relationships of that type. Eliminates map overhead for a fixed 4-key domain.
    std::array<FlatSet<std::string>, kRelationshipCount> relationships;

    JsonValue toJson() const
    {
        JsonObject obj;
        obj["enabled"] = JsonValue{enabled};
        if (!checkKey.empty())
        {
            obj["check_key"] = JsonValue{checkKey};
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
            std::string typeName(EnumStringPolicy<FeatureRelationship>::to_string(type));
            obj[typeName] = JsonValue{std::move(arr)};
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
            node.checkKey = std::get<std::string>(it->second);

            // Look up callback from factory (STRICT: keys must exist)
            if (!node.checkKey.empty())
            {
                auto checkResult = getFeatureCheckFactory().make(node.checkKey);
                if (!checkResult)
                {
                    return unexpected("check_key '" + node.checkKey + "' not found in FeatureCheckFactory");
                }
                node.check = *checkResult;
            }
            else
            {
                node.checkKey.clear();
            }
        }

        std::array<std::string_view, 4> types = {"Requires", "Conflicts", "Implies", "MutuallyExclusive"};
        for (const auto& typeStr : types)
        {
            std::string ts(typeStr);
            it = obj.find(ts);
            if (it != obj.end())
            {
                if (!it->second.is_array())
                {
                    return unexpected(ts + " must be array");
                }
                const auto& arr = std::get<JsonArray>(it->second);
                FeatureRelationship type = EnumStringPolicy<FeatureRelationship>::from_string(typeStr);
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

/// @brief Type-erased base for feature group storage.
struct FeatureGroupInfoBase
{
    virtual ~FeatureGroupInfoBase() = default;
    virtual FlatSet<std::string> getFeatures() const = 0;
    virtual JsonValue toJson() const = 0;
    virtual std::string stateToString() const = 0;

    // Type-erased state computation: invokes the concrete StateComputer and
    // caches the result. Returns the computed state as an int ordinal so the
    // caller never needs to downcast.
    virtual int computeAndCache(size_t enabledCount, bool hasConflict, bool allChecksPass) = 0;
};

/**
 * @brief Concrete group storage with type-safe state computation.
 *
 * @tparam StateEnum Enum type for the group's computed state.
 */
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupInfo : public FeatureGroupInfoBase
{
    FlatSet<std::string> features;
    StateComputer<StateEnum> stateComputer;
    mutable std::atomic<StateEnum> cachedState;

    FeatureGroupInfo(const std::vector<std::string>& f,
                     StateComputer<StateEnum> comp = FeatureGroupStatePolicy<StateEnum>::compute)
        : features(f.begin(), f.end())
        , stateComputer(comp)
        , cachedState(static_cast<StateEnum>(FeatureGroupState::Inactive))
    {
    }

    FlatSet<std::string> getFeatures() const override
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
                cachedState.load(std::memory_order_relaxed)));
        }
        else
        {
            return toString(cachedState.load(std::memory_order_relaxed));
        }
    }

    int computeAndCache(size_t enabledCount, bool hasConflict, bool allChecksPass) override
    {
        StateEnum state = stateComputer(features, enabledCount, hasConflict, allChecksPass);
        cachedState.store(state, std::memory_order_relaxed);
        return static_cast<int>(state);
    }
};

/**
 * @brief Runtime feature flag manager with dependency resolution and conflict detection.
 *
 * Manages a directed graph of features connected by Requires, Implies, Conflicts,
 * and MutuallyExclusive relationships. Enable/disable operations are transactional:
 * all changes succeed atomically or roll back completely.
 *
 * @tparam SyncPolicy Thread-safety policy (default: SingleThreadedPolicy).
 *                    Use MutexSynchronizationPolicy for multi-threaded access.
 *
 * @see FeatureRelationship, FeatureGroupState
 */
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
        auto fromRes = getNode(from);
        if (!fromRes)
        {
            return unexpected("Feature not found: " + from + " in relationship setup");
        }
        auto toRes = getNode(to);
        if (!toRes)
        {
            return unexpected("Feature not found: " + to + " in relationship setup");
        }

        // Prevent self-referencing relationships
        if (from == to)
        {
            return unexpected("Cannot add self-referencing relationship: " + from);
        }

        FeatureNode* fromNode = *fromRes;
        fromNode->relationships[relIdx(type)].insert(to);

        // Bidirectional for conflicts and mutually exclusive
        if (type == FeatureRelationship::Conflicts || type == FeatureRelationship::MutuallyExclusive)
        {
            FeatureNode* toNode = *toRes;
            toNode->relationships[relIdx(type)].insert(from);
        }
        return {};
    }

    // Build a human-readable cycle path from the enabling chain
    // The enablingChain preserves insertion order (the actual traversal path)
    // Example: enablingChain = {"A", "B", "C"}, target = "A"
    // Returns: "A -> B -> C -> A"
    std::string buildCyclePath(const std::vector<std::string>& enablingChain, const std::string& target) const
    {
        if (enablingChain.empty())
        {
            return target + " (self-referencing)";
        }

        // Build ordered path: the vector preserves actual traversal order
        std::string path;
        bool started = false;

        for (const auto& feature : enablingChain)
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

            auto* nodePtr = mFeatures.find(name);
            if (!nodePtr)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return unexpected("Feature not found: " + name);
            }
            const FeatureNode& node = *nodePtr;

            auto visitRelationship = [&](FeatureRelationship rel) -> Expected<void, std::string> {
                const auto& targets = node.relationships[relIdx(rel)];
                if (targets.empty())
                {
                    return {};
                }
                for (const auto& dep : targets)
                {
                    auto depState = state.find(dep);
                    if (depState != state.end() && depState->second == VisitState::Visiting)
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

            auto reqRes = visitRelationship(FeatureRelationship::Requires);
            if (!reqRes)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return reqRes;
            }

            auto implRes = visitRelationship(FeatureRelationship::Implies);
            if (!implRes)
            {
                stack.pop_back();
                state[name] = VisitState::Visited;
                return implRes;
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
        auto cycleRes = detectCyclesUnlocked();
        if (!cycleRes)
        {
            return cycleRes;
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
            const auto& requiresTargets = node.relationships[relIdx(FeatureRelationship::Requires)];
            if (!requiresTargets.empty())
            {
                for (const auto& required : requiresTargets)
                {
                    auto* reqPtr = mFeatures.find(required);
                    if (!reqPtr)
                    {
                        return unexpected("Required feature not found: " + required);
                    }
                    if (!reqPtr->enabled)
                    {
                        return unexpected("'" + name + "' requires '" + required + "' but it's disabled");
                    }
                }
            }

            // Implies: enabled feature must have all implied features enabled
            const auto& impliesTargets = node.relationships[relIdx(FeatureRelationship::Implies)];
            if (!impliesTargets.empty())
            {
                for (const auto& implied : impliesTargets)
                {
                    auto* implPtr = mFeatures.find(implied);
                    if (!implPtr)
                    {
                        return unexpected("Implied feature not found: " + implied);
                    }
                    if (!implPtr->enabled)
                    {
                        return unexpected("'" + name + "' implies '" + implied + "' but it's disabled");
                    }
                }
            }

            // Conflicts and MutuallyExclusive
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                const auto& conflictTargets = node.relationships[relIdx(rel)];
                if (conflictTargets.empty())
                {
                    continue;
                }

                for (const auto& other : conflictTargets)
                {
                    auto* otherPtr = mFeatures.find(other);
                    if (!otherPtr)
                    {
                        if (rel == FeatureRelationship::Conflicts)
                        {
                            return unexpected("Conflicting feature not found: " + other);
                        }
                        return unexpected("Mutually exclusive feature not found: " + other);
                    }
                    if (otherPtr->enabled)
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
                auto checkResult = node.check();
                if (!checkResult)
                {
                    return unexpected("Check failed for " + name + ": " + checkResult.error());
                }
            }
        }

        return {};
    }

    Expected<void, std::string> enableFeature(const std::string& name,
                                               std::vector<std::string>& enablingChain,
                                               std::unordered_set<std::string>& chainSet,
                                               std::vector<std::string>* changedFeatures,
                                               int depth = 0)
    {
        if (static_cast<size_t>(depth) > kMaxValidationDepth)
        {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }

        // O(1) membership test via hash set; vector is kept for path reconstruction only
        auto inChain = [&chainSet](const std::string& n) {
            return chainSet.count(n) != 0;
        };

        // Check for circular dependencies first
        if (inChain(name))
        {
            return unexpected("Circular dependency detected: " + buildCyclePath(enablingChain, name));
        }

        auto nodeRes = getNode(name);
        if (!nodeRes)
        {
            return unexpected(nodeRes.error());
        }
        FeatureNode* node = *nodeRes;

        if (node->enabled)
        {
            return {}; // Already enabled - nothing to do
        }

        // Track that we're in the process of enabling this feature
        enablingChain.push_back(name);
        chainSet.insert(name);

        // RAII guard to ensure enablingChain and chainSet stay consistent on scope exit.
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
        } chainGuard(enablingChain, chainSet);

        // Enable this feature first (may be rolled back on error)
        bool wasEnabled = node->enabled;
        node->enabled = true;

        // Process Required relationships (recursively enable dependencies)
        {
            const auto& targets = node->relationships[relIdx(FeatureRelationship::Requires)];
            for (const auto& required : targets)
            {
                // Check for circular dependency before recursing
                if (inChain(required))
                {
                    node->enabled = wasEnabled;
                    return unexpected("Circular dependency detected: " + buildCyclePath(enablingChain, required));
                }

                auto reqNodeRes = getNode(required);
                if (!reqNodeRes)
                {
                    node->enabled = false;
                    return unexpected("Required feature not found: " + required);
                }
                FeatureNode* reqNode = *reqNodeRes;
                if (!reqNode->enabled)
                {
                    auto enableRes = enableFeature(required, enablingChain, chainSet, changedFeatures, depth + 1);
                    if (!enableRes)
                    {
                        node->enabled = false;
                        return enableRes;
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
                if (inChain(implied))
                {
                    node->enabled = false;
                    return unexpected("Circular dependency detected: " + buildCyclePath(enablingChain, implied));
                }

                auto implNodeRes = getNode(implied);
                if (!implNodeRes)
                {
                    node->enabled = false;
                    return unexpected("Implied feature not found: " + implied);
                }

                FeatureNode* implNode = *implNodeRes;
                if (!implNode->enabled)
                {
                    auto enableRes = enableFeature(implied, enablingChain, chainSet, changedFeatures, depth + 1);
                    if (!enableRes)
                    {
                        node->enabled = false;
                        return enableRes;
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
                    auto confNodeRes = getNode(conflicting);
                    if (!confNodeRes)
                    {
                        node->enabled = false;
                        if (type == FeatureRelationship::Conflicts)
                        {
                            return unexpected("Conflicting feature not found: " + conflicting);
                        }
                        return unexpected("Mutually exclusive feature not found: " + conflicting);
                    }

                    FeatureNode* confNode = *confNodeRes;
                    if (confNode->enabled)
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
            auto checkResult = node->check();
            if (!checkResult)
            {
                node->enabled = false;
                return unexpected("Check failed for " + name + ": " + checkResult.error());
            }
        }

        // Success - chainGuard destructor will pop_back()
        // (we don't dismiss it because we want the pop to happen)

        // Track this feature as changed (for observer notification)
        if (changedFeatures && !wasEnabled)
        {
            changedFeatures->push_back(name);
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
                                        const std::string& featureName,
                                        bool newState,
                                        bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(featureName, newState, success);
        }
    }

    static void notifyBatchObserversSorted(const std::vector<BatchObserverEntry>& sorted,
                                              const std::string& requestedFeature,
                                              const std::vector<std::string>& allChanged,
                                              bool enabled,
                                              bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(requestedFeature, allChanged, enabled, success);
        }
    }

    template <typename StateEnum>
    Expected<StateEnum, std::string> computeGroupStateImpl(const std::string& groupName) const
    {
        auto* groupUptr = mGroups.find(groupName);
        if (!groupUptr)
        {
            return unexpected("Group not found: " + groupName);
        }
        FeatureGroupInfoBase* group = groupUptr->get();
        const auto groupFeatures = group->getFeatures();
        size_t enabledCount = 0;
        bool hasConflict = false;
        bool allChecksPass = true;
        for (const auto& featureName : groupFeatures)
        {
            auto nodeRes = getNode(featureName);
            if (!nodeRes)
            {
                continue;
            }
            const FeatureNode* node = *nodeRes;
            if (node->enabled)
            {
                ++enabledCount;
                // Check for conflicts within group
                for (const auto& other : groupFeatures)
                {
                    if (other == featureName)
                    {
                        continue;
                    }
                    if (node->relationships[relIdx(FeatureRelationship::Conflicts)].count(other) > 0)
                    {
                        auto otherNodeRes = getNode(other);
                        if (otherNodeRes && (*otherNodeRes)->enabled)
                        {
                            hasConflict = true;
                        }
                    }
                    if (node->relationships[relIdx(FeatureRelationship::MutuallyExclusive)].count(other) > 0)
                    {
                        auto otherNodeRes = getNode(other);
                        if (otherNodeRes && (*otherNodeRes)->enabled)
                        {
                            hasConflict = true;
                        }
                    }
                }
                if (node->check)
                {
                    auto checkResult = node->check();
                    if (!checkResult)
                    {
                        allChecksPass = false;
                    }
                }
            }
        }
        // Virtual dispatch: the concrete FeatureGroupInfo<StateEnum> invokes its
        // typed stateComputer and caches the result. No dynamic_cast needed.
        int ordinal = group->computeAndCache(enabledCount, hasConflict, allChecksPass);
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

    /**
     * @brief RAII guard for temporary feature state changes.
     *
     * Construction routes through the validated enable/disable path (conflict
     * checking, dependency resolution, implies propagation). If validation fails,
     * the object is constructed but marked invalid — check via valid() or operator bool().
     *
     * Destruction restores only the features this guard actually changed, and only
     * if they are still in the state the guard set them to. This prevents concurrent
     * legitimate state changes from being silently reverted.
     *
     * Observer notifications fire on rollback (outside the lock) with the inverse
     * direction of the original operation.
     *
     * @note Non-copyable, moveable.
     */
    class ScopedFeatureChange
    {
    private:
        FeatureManager* mManager;
        std::string mRequestedFeature;          // the feature the caller asked to toggle
        std::vector<std::string> mChangedFeatures; // features this guard actually toggled
        bool mNewState;                         // direction: true = enabled, false = disabled
        bool mValid;

    public:
        ScopedFeatureChange(FeatureManager& manager, const std::string& featureName, bool newState)
            : mManager(&manager)
            , mRequestedFeature(featureName)
            , mNewState(newState)
            , mValid(false)
        {
            // Snapshot feature states before the operation to detect what changed.
            FastHashMap<std::string, bool> preStates;
            {
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    preStates[name] = node.enabled;
                }
            }

            // Use the validated enable/disable path: conflict checking, dependency
            // resolution, and implies propagation all apply. batchEnable/batchDisable
            // handles its own locking internally.
            Expected<void, std::string> result;
            if (newState)
            {
                result = mManager->enable(featureName);
            }
            else
            {
                result = mManager->disable(featureName);
            }

            mValid = result.has_value();

            if (mValid)
            {
                // Determine which features actually changed state by diffing
                // pre-operation snapshot against current state.
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    auto* pre = preStates.find(name);
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

            std::vector<std::string> restoredFeatures;
            std::vector<ObserverEntry> observersSnapshot;
            std::vector<BatchObserverEntry> batchObserversSnapshot;

            {
                [[maybe_unused]] auto guard = mManager->mSync.lock();
                for (const auto& name : mChangedFeatures)
                {
                    auto nodeRes = mManager->getNode(name);
                    if (!nodeRes)
                    {
                        continue;
                    }
                    FeatureNode* node = *nodeRes;

                    // Restore only if still in the state we set it to.
                    // If another thread changed it, respect their change.
                    if (node->enabled == mNewState)
                    {
                        node->enabled = !mNewState;
                        restoredFeatures.push_back(name);
                    }
                }

                if (!restoredFeatures.empty())
                {
                    observersSnapshot = mManager->mObservers;
                    batchObserversSnapshot = mManager->mBatchObservers;
                }
            } // Lock released before observer notification

            if (!restoredFeatures.empty())
            {
                sortObserversByPriority(observersSnapshot);
                for (const auto& feature : restoredFeatures)
                {
                    notifyObserversSorted(observersSnapshot, feature, !mNewState, true);
                }

                if (!batchObserversSnapshot.empty())
                {
                    sortBatchObserversByPriority(batchObserversSnapshot);
                    notifyBatchObserversSorted(batchObserversSnapshot,
                                                  mRequestedFeature,
                                                  restoredFeatures,
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

    /**
     * @brief RAII guard for automatic observer registration/unregistration.
     *
     * Registers a FeatureObserver on construction and removes it on destruction.
     *
     * @note Non-copyable, moveable.
     * @see addObserver()
     */
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

        /// @brief Returns the observer ID (for manual removal if needed).
        ObserverId id() const
        {
            return mId;
        }

        /// @brief Releases ownership without unregistering; returns the ID.
        ObserverId release()
        {
            mManager = nullptr;
            return std::exchange(mId, 0);
        }
    };

    /**
     * @brief RAII guard for automatic batch observer registration/unregistration.
     *
     * @note Non-copyable, moveable.
     * @see addBatchObserver()
     */
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

        /// @brief Returns the observer ID.
        ObserverId id() const
        {
            return mId;
        }
        /// @brief Releases ownership without unregistering; returns the ID.
        ObserverId release()
        {
            mManager = nullptr;
            return std::exchange(mId, 0);
        }
    };

    /**
     * @brief Registers a new feature with an optional validation check.
     *
     * @param name  Unique feature name.
     * @param check Optional callback invoked during validation; nullptr for unconditional.
     * @return Expected<void> on success, or error if name already exists.
     *
     * @note Complexity: O(log n) for insertion into the feature map.
     * @note Thread-safety: Acquires internal lock.
     */
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
        node.checkKey = ""; // No key when added directly with callback
        mFeatures[name] = std::move(node);
        return {};
    }

    /**
     * @brief Registers a new feature using a factory-registered check key.
     *
     * The key is stored alongside the feature so that serialization (toJson/fromJson)
     * can reconstruct the check callback via the FeatureCheckFactory.
     *
     * @param name     Unique feature name.
     * @param checkKey Key previously registered with getFeatureCheckFactory().
     * @return Expected<void> on success, or error if name exists or key is not in factory.
     *
     * @note Complexity: O(log n).
     * @note Thread-safety: Acquires internal lock.
     * @see getFeatureCheckFactory(), FeatureCheckRegistration
     */
    [[nodiscard]] Expected<void, std::string> addFeature(const std::string& name, const std::string& checkKey)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        if (mFeatures.count(name))
        {
            return unexpected("Feature already exists: " + name);
        }

        // Look up the check from factory
        auto checkResult = getFeatureCheckFactory().make(checkKey);
        if (!checkResult)
        {
            return unexpected("Check key '" + checkKey + "' not found in factory");
        }

        FeatureNode node;
        node.enabled = false;
        node.check = *checkResult;
        node.checkKey = checkKey;
        mFeatures[name] = std::move(node);
        return {};
    }

    /**
     * @brief Adds a directed relationship between two features.
     *
     * Conflicts and MutuallyExclusive relationships are automatically symmetrized.
     *
     * @param from Source feature name.
     * @param type Relationship kind (Requires, Implies, Conflicts, MutuallyExclusive).
     * @param to   Target feature name.
     * @return Expected<void> on success, or error if either feature does not exist.
     *
     * @note Thread-safety: Acquires internal lock.
     */
    [[nodiscard]] Expected<void, std::string>
    addRelationship(const std::string& from, FeatureRelationship type, const std::string& to)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return addRelationshipUnlocked(from, type, to);
    }

    /**
     * @brief Creates a named feature group with optional custom state computation.
     *
     * Groups aggregate features and expose a computed state (e.g., Active, Partial).
     * All named features must already be registered.
     *
     * @tparam StateEnum Enum type for group state (default: FeatureGroupState).
     * @param groupName    Unique group name.
     * @param featureNames Features to include in the group.
     * @param computer     State computation function (default: FeatureGroupStatePolicy).
     * @return Expected<void> on success, or error if group name exists or any feature is missing.
     *
     * @note Thread-safety: Acquires internal lock.
     * @see getGroupState(), FeatureGroupStatePolicy
     */
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string>
    addGroup(const std::string& groupName,
              const std::vector<std::string>& featureNames,
              StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        if (mGroups.count(groupName))
        {
            return unexpected("Group already exists: " + groupName);
        }
        for (const auto& featureName : featureNames)
        {
            if (!mFeatures.count(featureName))
            {
                return unexpected("Feature not found: " + featureName);
            }
        }
        mGroups[groupName] = std::make_unique<FeatureGroupInfo<StateEnum>>(featureNames, computer);
        return {};
    }

    /**
     * @brief Creates a group where all features conflict with each other.
     *
     * Equivalent to addGroup() plus adding Conflicts relationships between every
     * pair of features in the group.
     *
     * @tparam StateEnum Enum type for group state (default: FeatureGroupState).
     * @param groupName    Unique group name.
     * @param featureNames Features to include (must already be registered).
     * @param computer     State computation function.
     * @return Expected<void> on success, or error if group exists or features missing.
     *
     * @note Thread-safety: Acquires internal lock.
     */
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string>
    addMutuallyExclusiveGroup(const std::string& groupName,
                                 const std::vector<std::string>& featureNames,
                                 StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute)
    {
        [[maybe_unused]] auto guard = mSync.lock();

        if (mGroups.count(groupName))
        {
            return unexpected("Group already exists: " + groupName);
        }
        for (const auto& featureName : featureNames)
        {
            if (!mFeatures.count(featureName))
            {
                return unexpected("Feature not found: " + featureName);
            }
        }

        // Add bidirectional mutual-exclusion relationships between every pair.
        for (size_t i = 0; i < featureNames.size(); ++i)
        {
            for (size_t j = i + 1; j < featureNames.size(); ++j)
            {
                auto res = addRelationshipUnlocked(featureNames[i],
                                                     FeatureRelationship::MutuallyExclusive,
                                                     featureNames[j]);
                if (!res)
                {
                    return res;
                }
            }
        }

        mGroups[groupName] = std::make_unique<FeatureGroupInfo<StateEnum>>(featureNames, computer);
        return {};
    }

    /**
     * @brief Computes and returns the current state of a feature group.
     *
     * @tparam StateEnum Enum type to cast the result to (default: FeatureGroupState).
     * @param groupName Name of the group.
     * @return Expected<StateEnum> with computed state, or error if group not found.
     *
     * @note Thread-safety: Acquires internal lock.
     */
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<StateEnum, std::string> getGroupState(const std::string& groupName) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return computeGroupStateImpl<StateEnum>(groupName);
    }

    /**
     * @brief Returns the set of feature names belonging to a group.
     *
     * @param groupName Name of the group.
     * @return Expected<FlatSet<std::string>> with the feature set, or error if not found.
     *
     * @note Thread-safety: Acquires internal lock.
     */
    [[nodiscard]] Expected<FlatSet<std::string>, std::string> getGroupFeatures(const std::string& groupName) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        auto* groupUptr = mGroups.find(groupName);
        if (!groupUptr)
        {
            return unexpected("Group not found: " + groupName);
        }
        return (*groupUptr)->getFeatures();
    }

    /**
     * @brief Enables a feature with full transactional semantics.
     *
     * Delegates to batchEnable(). If enabling fails (e.g., conflict), no
     * dependencies are left enabled.
     *
     * @param name Feature to enable.
     * @return Expected<void> on success, or error describing the failure.
     *
     * @note Thread-safety: Acquires internal lock.
     * @see batchEnable()
     */
    [[nodiscard]] Expected<void, std::string> enable(const std::string& name)
    {
        return batchEnable({name});
    }

    /**
     * @brief Disables a feature with transactional semantics.
     *
     * Delegates to batchDisable() to ensure Requires/Implies constraints are
     * checked and all side effects are handled atomically.
     *
     * @param name Feature to disable.
     * @return Expected<void> on success, or error if disabling would violate constraints.
     *
     * @note Thread-safety: Acquires internal lock.
     * @see batchDisable()
     */
    [[nodiscard]] Expected<void, std::string> disable(const std::string& name)
    {
        return batchDisable({name});
    }

    /**
     * @brief Enables multiple features atomically with dependency resolution.
     *
     * All features and their transitive Requires/Implies dependencies succeed
     * together, or all changes are rolled back. Observer notifications are
     * deferred until after the internal lock is released.
     *
     * @param names Features to enable.
     * @return Expected<void> on success, or error (with full rollback) on failure.
     *
     * @note Complexity: O(n * d * log n) where d = dependency depth.
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     */
    [[nodiscard]] Expected<void, std::string> batchEnable(const std::vector<std::string>& names)
    {
        std::vector<std::string> allChanged;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        { // Scope for LockGuard - Lock held only during state modification
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all features exist first
            for (const auto& name : names)
            {
                auto nodeRes = getNode(name);
                if (!nodeRes)
                {
                    return unexpected(nodeRes.error());
                }
            }

            // Snapshot ALL feature states before any modifications
            FastHashMap<std::string, bool> originalStates;
            for (const auto& [name, node] : mFeatures)
            {
                originalStates[name] = node.enabled;
            }

            // Attempt to enable each feature
            for (const auto& name : names)
            {
                std::vector<std::string> chain;
                std::unordered_set<std::string> chainSet;
                auto res = enableFeature(name, chain, chainSet, &allChanged);
                if (!res)
                {
                    // Rollback ALL features to original states
                    for (auto&& [featureName, node] : mFeatures)
                    {
                        node.enabled = originalStates[featureName];
                    }
                    return res;
                }
            }

            // Snapshot observers while holding the lock, then invoke callbacks after unlock.
            // This prevents data races and makes it safe for observers to add/remove observers
            // or call FeatureManager methods (reentrant use).
            if (!allChanged.empty())
            {
                observersSnapshot = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released here

        if (!allChanged.empty())
        {
            // Notify observers safely outside the lock
            sortObserversByPriority(observersSnapshot);
            for (const auto& feature : allChanged)
            {
                notifyObserversSorted(observersSnapshot, feature, true, true);
            }

            // Notify batch observers
            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot,
                                              names.empty() ? "" : names[0],
                                              allChanged,
                                              true,
                                              true);
            }
        }

        return {};
    }

    /**
     * @brief Disables multiple features atomically with constraint checking.
     *
     * Validates that no remaining enabled feature Requires or Implies any of the
     * features being disabled. All succeed or all changes are rolled back.
     * Observer notifications are deferred until after lock release.
     * Duplicate names in the input are automatically deduplicated.
     *
     * @param names Features to disable.
     * @return Expected<void> on success, or error (with full rollback) on failure.
     *
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     */
    [[nodiscard]] Expected<void, std::string> batchDisable(const std::vector<std::string>& names)
    {
        std::vector<std::string> actuallyChanged;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        // Deduplicate requested names while preserving first-seen order.
        // This prevents incorrect rollback when the same feature appears multiple times.
        std::vector<std::string> uniqueNames;
        uniqueNames.reserve(names.size());
        std::unordered_set<std::string> disabledSet;
        for (const auto& n : names)
        {
            if (disabledSet.insert(n).second)
            {
                uniqueNames.push_back(n);
            }
        }

        { // Scope for LockGuard - Lock held only during state modification
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all features exist first
            for (const auto& name : uniqueNames)
            {
                auto nodeRes = getNode(name);
                if (!nodeRes)
                {
                    return unexpected(nodeRes.error());
                }
            }

            // Record original states for rollback and track which actually changed
            std::vector<bool> originalStates;
            originalStates.reserve(uniqueNames.size());

            for (const auto& name : uniqueNames)
            {
                auto nodeRes = getNode(name);
                originalStates.push_back((*nodeRes)->enabled);
                if ((*nodeRes)->enabled)
                {
                    actuallyChanged.push_back(name);
                }
                (*nodeRes)->enabled = false;
            }

            // Rollback helper lambda
            auto rollback = [&]() {
                for (size_t i = 0; i < uniqueNames.size(); ++i)
                {
                    auto n = getNode(uniqueNames[i]);
                    if (n)
                    {
                        (*n)->enabled = originalStates[i];
                    }
                }
            };

            // Validate the resulting state
            for (const auto& [featureName, node] : mFeatures)
            {
                if (!node.enabled)
                {
                    continue;
                }

                // Check if this enabled feature requires any of the disabled features
                for (const auto& required : node.relationships[relIdx(FeatureRelationship::Requires)])
                {
                    auto reqNode = getNode(required);
                    if (!reqNode)
                    {
                        rollback();
                        return unexpected("Required feature not found: " + required);
                    }
                    if (!(*reqNode)->enabled)
                    {
                        rollback();
                        return unexpected("Cannot disable '" + required + "': required by enabled feature '" +
                                          featureName + "'");
                    }
                }

                // Check if this enabled feature implies any of the disabled features
                // If A implies B and A is enabled, then B cannot be disabled
                for (const auto& implied : node.relationships[relIdx(FeatureRelationship::Implies)])
                {
                    if (disabledSet.count(implied))
                    {
                        rollback();
                        return unexpected("Cannot disable '" + implied + "': implied by enabled feature '" +
                                          featureName + "'. Disable '" + featureName + "' first.");
                    }
                }
            }

            if (!actuallyChanged.empty())
            {
                observersSnapshot = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released here

        if (!actuallyChanged.empty())
        {
            // Notify observers safely outside the lock
            sortObserversByPriority(observersSnapshot);
            for (const auto& feature : actuallyChanged)
            {
                notifyObserversSorted(observersSnapshot, feature, false, true);
            }

            // Notify batch observers
            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot,
                                              names.empty() ? "" : names[0],
                                              actuallyChanged,
                                              false,
                                              true);
            }
        }

        return {};
    }

    /**
     * @brief Returns whether a feature is currently enabled.
     *
     * @param name Feature to query.
     * @return true if enabled, false if disabled or not found.
     *
     * @note Thread-safety: Acquires internal lock.
     */
    [[nodiscard]] bool isEnabled(const std::string& name) const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        auto nodeRes = getNode(name);
        if (!nodeRes)
        {
            return false;
        }
        return (*nodeRes)->enabled;
    }

    /**
     * @brief Validates the entire feature graph for consistency.
     *
     * Checks for cycles, dangling relationship targets, constraint violations
     * (enabled features missing required dependencies), and failed check callbacks.
     *
     * @return Expected<void> on success, or error describing the first violation found.
     *
     * @note Complexity: O(n * d * log n) where n = features, d = max depth.
     * @note Thread-safety: Acquires internal lock.
     */
    [[nodiscard]] Expected<void, std::string> validate()
    {
        [[maybe_unused]] auto guard = mSync.lock();
        return validateUnlocked();
    }

    /**
     * @brief Registers an observer called on each individual feature state change.
     *
     * Higher priority observers are called first. Observers are invoked after
     * the internal lock is released; the observer list is snapshotted before
     * invocation, so callbacks may safely add/remove observers or call back
     * into the manager (reentrant use).
     *
     * @param callback Function receiving (featureName, newState, success).
     * @param priority Ordering priority (higher = called first, default 0).
     * @return ObserverId for later removal via removeObserver().
     *
     * @warning Reentrant enable/disable from an observer can trigger nested
     *          notifications and may lead to cycles if not designed carefully.
     *          Observers should remain lightweight.
     *
     * @note Thread-safety: Acquires internal lock; callbacks called outside lock.
     * @see removeObserver(), ScopedObserver
     */
    ObserverId addObserver(FeatureObserver callback, int priority = 0)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        ObserverId id = mNextObserverId++;
        mObservers.push_back({id, priority, std::move(callback)});
        return id;
    }

    /**
     * @brief Registers a batch observer called once per enable/disable operation.
     *
     * Batch observers receive the complete set of features that changed,
     * including implicit dependencies resolved via Requires/Implies.
     *
     * @param callback Function receiving (requestedFeature, allChanged, enabled, success).
     * @param priority Ordering priority (higher = called first, default 0).
     * @return ObserverId for later removal via removeObserver().
     *
     * @note Thread-safety: Acquires internal lock; callbacks called outside lock.
     * @see removeObserver(), ScopedBatchObserver
     */
    ObserverId addBatchObserver(BatchObserver callback, int priority = 0)
    {
        [[maybe_unused]] auto guard = mSync.lock();
        ObserverId id = mNextObserverId++;
        mBatchObservers.push_back({id, priority, std::move(callback)});
        return id;
    }

    /**
     * @brief Removes a previously registered observer (regular or batch).
     *
     * @param id Observer ID returned by addObserver() or addBatchObserver().
     * @return true if found and removed, false if no observer with that ID exists.
     *
     * @note Thread-safety: Acquires internal lock.
     */
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

    /// @brief Removes all registered observers (both regular and batch).
    void clearObservers()
    {
        [[maybe_unused]] auto guard = mSync.lock();
        mObservers.clear();
        mBatchObservers.clear();
    }

    /// @brief Returns the names of all currently enabled features.
    [[nodiscard]] std::vector<std::string> getEnabled() const
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

    /// @brief Returns the names of all registered features.
    [[nodiscard]] std::vector<std::string> getAllFeatures() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        std::vector<std::string> allFeatures;
        for (const auto& [name, _] : mFeatures)
        {
            allFeatures.push_back(name);
        }
        return allFeatures;
    }

    /// @brief Returns the names of all registered groups.
    [[nodiscard]] std::vector<std::string> getAllGroups() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        std::vector<std::string> allGroups;
        for (const auto& [name, _] : mGroups)
        {
            allGroups.push_back(name);
        }
        return allGroups;
    }

    /**
     * @brief Serializes the entire feature graph to a JSON string.
     *
     * Includes feature states, check keys, relationships, and group membership.
     * Check callbacks themselves are not serialized; only their factory keys.
     *
     * @return JSON string representation of the feature graph.
     *
     * @note Thread-safety: Acquires internal lock.
     * @see fromJson()
     */
    [[nodiscard]] std::string toJson() const
    {
        [[maybe_unused]] auto guard = mSync.lock();
        JsonObject root;
        JsonObject featuresJson;
        for (const auto& [name, node] : mFeatures)
        {
            featuresJson[name] = node.toJson();
        }
        root["features"] = JsonValue{std::move(featuresJson)};
        JsonObject groupsJson;
        for (const auto& [name, group] : mGroups)
        {
            groupsJson[name] = group->toJson();
        }
        root["groups"] = JsonValue{std::move(groupsJson)};
        return to_json_string(root);
    }

    /**
     * @brief Reconstructs a FeatureManager from a JSON string.
     *
     * Performs structural validation (all relationship targets must exist),
     * symmetrizes Conflicts/MutuallyExclusive relationships, and runs full
     * cycle detection and enabled-state invariant checks.
     * If any validation fails, an error is returned and no partial state is created.
     *
     * All check_key values in the JSON must already be registered in the
     * FeatureCheckFactory before calling this method.
     *
     * @param jsonStr JSON string produced by toJson() or equivalent.
     * @return Expected<FeatureManager> on success, or error describing the failure.
     *
     * @see toJson(), getFeatureCheckFactory()
     */
    [[nodiscard]] static Expected<FeatureManager, std::string> fromJson(const std::string& jsonStr)
    {
        JsonValue root;
        try
        {
            root = parse_json(jsonStr);
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
        auto featuresIt = obj.find("features");
        if (featuresIt != obj.end())
        {
            if (!featuresIt->second.is_object())
            {
                return unexpected("'features' must be an object");
            }
            const auto& featuresObj = std::get<JsonObject>(featuresIt->second);
            for (const auto& [name, value] : featuresObj)
            {
                auto nodeRes = FeatureNode::fromJson(value);
                if (!nodeRes)
                {
                    return unexpected("Error parsing feature '" + name + "': " + nodeRes.error());
                }
                manager.mFeatures[name] = std::move(*nodeRes);
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
        for (auto&& [fromName, fromNode] : manager.mFeatures)
        {
            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                const auto& targets = fromNode.relationships[relIdx(rel)];
                if (targets.empty())
                {
                    continue;
                }
                for (const auto& toName : targets)
                {
                    // Add reverse relationship if not already present.
                    // toName is validated to exist by the relationship check above.
                    auto* toNodePtr = manager.mFeatures.find(toName);
                    (void)toNodePtr->relationships[relIdx(rel)].insert(fromName);
                }
            }
        }

        auto groupsIt = obj.find("groups");
        if (groupsIt != obj.end())
        {
            if (!groupsIt->second.is_object())
            {
                return unexpected("'groups' must be an object");
            }
            const auto& groupsObj = std::get<JsonObject>(groupsIt->second);
            for (const auto& [name, value] : groupsObj)
            {
                if (!value.is_array())
                {
                    return unexpected("Group '" + name + "' must be an array");
                }
                const auto& arr = std::get<JsonArray>(value);
                std::vector<std::string> featureNames;
                for (const auto& elem : arr)
                {
                    if (!elem.is_string())
                    {
                        return unexpected("Group feature must be string");
                    }
                    auto featureName = std::get<std::string>(elem);
                    if (!manager.mFeatures.count(featureName))
                    {
                        return unexpected("Group '" + name + "' references missing feature '" + featureName + "'");
                    }
                    featureNames.push_back(std::move(featureName));
                }
                manager.mGroups[name] = std::make_unique<FeatureGroupInfo<FeatureGroupState>>(featureNames);
            }
        }

        // Full validation: detect cycles and verify enabled-state invariants.
        // This catches invalid graphs that could cause problems during enable/disable.
        auto validateRes = manager.validateUnlocked();
        if (!validateRes)
        {
            return unexpected("Loaded graph fails validation: " + validateRes.error());
        }

        return manager;
    }

    /**
     * @brief Exports the feature graph in GraphViz DOT format.
     *
     * Enabled features are green, disabled are gray. Relationship types
     * are rendered as styled edges.
     *
     * @return DOT-format string suitable for rendering with graphviz.
     *
     * @note Thread-safety: Acquires internal lock.
     */
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
                    std::string_view typeStr = EnumStringPolicy<FeatureRelationship>::to_string(type);
                    ss << "    \"" << name << "\" -> \"" << target << "\" [style=" << style << ", arrowhead=" << arrow
                       << ", label=\"" << typeStr << "\"];\n";
                }
            }
        }
        ss << "}\n";
        return ss.str();
    }

    /// @brief Removes all features, groups, and observers, resetting to empty state.
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
