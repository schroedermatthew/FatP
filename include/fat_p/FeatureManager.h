#pragma once

/*
FATP_META:
  meta_version: 1
  component: FeatureManager
  file_role: public_header
  path: include/fat_p/FeatureManager.h
  namespace: fat_p::feature
  layer: Domain
  summary: "Runtime feature flag management with dependency resolution, conflict detection, and atomic feature substitution (replace, forceExclusive)."
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
 * (Requires, Implies, Conflicts, MutuallyExclusive, Preempts) and need automatic resolution.
 * Key features:
 * - Cycle detection with detailed error messages showing full dependency path
 * - Pluggable thread-safety policies (single-threaded, mutex, spinlock, shared_mutex)
 * - Type-safe group states with custom enums
 * - Observer pattern with priority ordering and RAII lifetime management
 * - JSON serialization and GraphViz DOT export
 * - RAII helpers for scoped state changes
 * - Optimized with FlatSet for relationship storage (cache-friendly sorted vectors)
 * - Preempts relationship: authoritative shutdown + cascade + latched inhibit
 * Performance characteristics:
 * - Add feature: O(1) average, O(n) worst case
 * - Enable/disable: O(d x log n) where d = dependency depth (limited to kMaxValidationDepth)
 * - Validate: O(n x d x log n)
 * - Memory: Per-feature storage uses FlatSet for relationship sets; size is compiler- and platform-dependent
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
#include "Stringify.h"

#include "FastHashMap.h"
#include "FlatSet.h"

#include "JsonLite.h"
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
    MutuallyExclusive,///< Group constraint: all members conflict with each other.
    Preempts          ///< Enabling source forcibly disables target and its reverse-dependency
                      ///< closure; blocks re-enable while source remains enabled. Directional.
};

/// @brief Number of FeatureRelationship enumerators; used for array-based storage.
inline constexpr size_t kRelationshipCount = 5;

/// @brief Converts FeatureRelationship enum to array index (0–4).
constexpr size_t relIdx(FeatureRelationship r) noexcept
{
    return static_cast<size_t>(r);
}

/**
 * @brief Records a single feature state transition.
 *
 * Used for observer notifications and rollback in mixed-direction transactions
 * (e.g. an enable that also disables preempted features and their dependents).
 */
struct FeatureChange
{
    std::string name;
    bool oldState = false;
    bool newState = false;
};

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
    static constexpr std::array<std::string_view, 5> names = {
        "Requires", "Conflicts", "Implies", "MutuallyExclusive", "Preempts"};

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
 * Receives a per-feature change record for every feature whose state changed during the
 * operation, including implicit dependencies (Requires/Implies) and preempted features.
 * The `changes` vector accurately represents mixed-direction transactions where some features
 * are enabled and others disabled within the same operation (e.g. Preempts).
 *
 * @param requestedFeature The feature the caller explicitly enabled/disabled.
 * @param changes          All feature state changes (name, oldState, newState).
 * @param success          true if the operation succeeded.
 */
using BatchObserver = std::function<void(const std::string& requestedFeature,
                                         const std::vector<FeatureChange>& changes,
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

    // Relationship storage: fixed-size array indexed by FeatureRelationship (0–4).
    // Each slot holds a FlatSet of target feature names. Empty slots indicate no
    // relationships of that type. Eliminates map overhead for a fixed 5-key domain.
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

        // Drive relationship parsing off the canonical enum name list so the
        // parser cannot drift from the enum. Adding a new FeatureRelationship
        // only requires updating EnumStringPolicy::names — not this function.
        for (std::string_view typeName : EnumStringPolicy<FeatureRelationship>::names)
        {
            std::string ts(typeName);
            it = obj.find(ts);
            if (it != obj.end())
            {
                if (!it->second.is_array())
                {
                    return unexpected(ts + " must be array");
                }
                const auto& arr = std::get<JsonArray>(it->second);
                FeatureRelationship type = EnumStringPolicy<FeatureRelationship>::from_string(typeName);
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
    // Performance: Each level adds overhead for function call + map lookups
    // Deep dependency chains add cumulative overhead but are acceptable for enable operations
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

        // -----------------------------------------------------------------------
        // Contradiction guards for Preempts
        // -----------------------------------------------------------------------
        // Preempts and Requires/Implies on the same directed edge are logically
        // contradictory: enabling the source would simultaneously force the target
        // ON (via Requires/Implies) and OFF (via Preempts).
        if (type == FeatureRelationship::Preempts)
        {
            if (fromNode->relationships[relIdx(FeatureRelationship::Requires)].count(to))
            {
                return unexpected("Cannot add Preempts from '" + from + "' to '" + to +
                                  "': '" + from + "' already Requires '" + to + "' (contradictory).");
            }
            if (fromNode->relationships[relIdx(FeatureRelationship::Implies)].count(to))
            {
                return unexpected("Cannot add Preempts from '" + from + "' to '" + to +
                                  "': '" + from + "' already Implies '" + to + "' (contradictory).");
            }
        }
        // Guard the reverse: adding Requires/Implies when Preempts already exists.
        if (type == FeatureRelationship::Requires || type == FeatureRelationship::Implies)
        {
            if (fromNode->relationships[relIdx(FeatureRelationship::Preempts)].count(to))
            {
                return unexpected("Cannot add " +
                                  std::string(EnumStringPolicy<FeatureRelationship>::to_string(type)) +
                                  " from '" + from + "' to '" + to +
                                  "': '" + from + "' already Preempts '" + to + "' (contradictory).");
            }
        }

        // -----------------------------------------------------------------------
        // Preempts cycle guard
        // -----------------------------------------------------------------------
        // Cycles in the Preempts subgraph (A Preempts B Preempts ... Preempts A)
        // are ambiguous without a priority lattice and are rejected at construction
        // time. Walk the Preempts forward edges from 'to' and fail if we reach 'from'.
        if (type == FeatureRelationship::Preempts)
        {
            // DFS over the existing Preempts edges starting at 'to'.
            std::unordered_set<std::string> visited;
            std::vector<std::string> stack;
            stack.push_back(to);
            while (!stack.empty())
            {
                std::string cur = std::move(stack.back());
                stack.pop_back();
                if (cur == from)
                {
                    return unexpected("Cannot add Preempts from '" + from + "' to '" + to +
                                      "': would create a Preempts cycle.");
                }
                if (!visited.insert(cur).second)
                {
                    continue;
                }
                auto* curNode = mFeatures.find(cur);
                if (!curNode)
                {
                    continue;
                }
                for (const auto& next : curNode->relationships[relIdx(FeatureRelationship::Preempts)])
                {
                    stack.push_back(next);
                }
            }
        }

        fromNode->relationships[relIdx(type)].insert(to);

        // Bidirectional for Conflicts and MutuallyExclusive only.
        // Requires, Implies, and Preempts are strictly directional (source → target only).
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

            // Preempts: if source is enabled, all preempt targets must be disabled
            const auto& preemptsTargets = node.relationships[relIdx(FeatureRelationship::Preempts)];
            if (!preemptsTargets.empty())
            {
                for (const auto& preempted : preemptsTargets)
                {
                    auto* preemptedPtr = mFeatures.find(preempted);
                    if (!preemptedPtr)
                    {
                        return unexpected("Preempts target not found: " + preempted);
                    }
                    if (preemptedPtr->enabled)
                    {
                        return unexpected("'" + name + "' preempts '" + preempted +
                                          "' but both are enabled (invariant violation).");
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

    // =========================================================================
    // Transaction Plan/Commit Infrastructure
    // =========================================================================
    //
    // Preempts introduces mixed-direction transactions: a single enable() call
    // can turn some features ON (the requested feature and its Requires/Implies
    // dependencies) and some features OFF (Preempts targets and their reverse
    // dependency closures). The old live-mutate/rollback model cannot represent
    // this cleanly. The plan/commit model solves it:
    //
    //   1. Snapshot live state into originalStates
    //   2. Compute desiredStates without touching live nodes (planning phase)
    //   3. Validate desiredStates completely before committing
    //   4. Apply desiredStates to live nodes (commit phase)
    //   5. Build FeatureChange list and notify observers
    //
    // No live state is mutated until step 4, so there are no partial commits
    // and no rollback logic needed.

    /// @brief Internal plan computed before any live state is mutated.
    struct TransactionPlan
    {
        FastHashMap<std::string, bool> originalStates; ///< Snapshot at transaction start.
        FastHashMap<std::string, bool> desiredStates;  ///< Target state being planned.
        std::vector<std::string> disableOrder;         ///< Features to disable (in closure order).
        std::vector<std::string> enableOrder;          ///< Features to enable (in dep order).
    };

    /// @brief Returns the name of any currently-desired-enabled feature that Preempts @p name,
    ///        or empty string if none. Used to enforce the latched-inhibit property.
    [[nodiscard]] std::string
    findEnabledPreemptorUnlocked(const std::string& name,
                                  const FastHashMap<std::string, bool>& desiredStates) const
    {
        for (const auto& [featureName, node] : mFeatures)
        {
            // Is this feature desired-enabled?
            auto* desired = desiredStates.find(featureName);
            if (!desired || !*desired)
            {
                continue;
            }
            // Does it Preempt the target?
            if (node.relationships[relIdx(FeatureRelationship::Preempts)].count(name))
            {
                return featureName;
            }
        }
        return {};
    }

    /// @brief Plans enabling @p name (and its Requires/Implies closure) into @p plan.
    ///        Recursively calls planDisableClosure() for each Preempts target.
    [[nodiscard]] Expected<void, std::string>
    planEnableRecursive(const std::string& name,
                        TransactionPlan& plan,
                        std::vector<std::string>& enablingChain,
                        std::unordered_set<std::string>& chainSet,
                        int depth = 0)
    {
        if (static_cast<size_t>(depth) > kMaxValidationDepth)
        {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }

        auto nodeRes = getNode(name);
        if (!nodeRes)
        {
            return unexpected(nodeRes.error());
        }
        const FeatureNode* node = *nodeRes;

        // Already planned to enable — nothing to do.
        auto* desired = plan.desiredStates.find(name);
        if (desired && *desired)
        {
            return {};
        }

        // Latched-inhibit check: fail if any desired-enabled feature Preempts this one.
        std::string blocker = findEnabledPreemptorUnlocked(name, plan.desiredStates);
        if (!blocker.empty())
        {
            return unexpected("Cannot enable '" + name + "': preempted by enabled feature '" +
                              blocker + "'.");
        }

        // Cycle detection in the Requires/Implies enable chain.
        if (chainSet.count(name))
        {
            return unexpected("Circular dependency detected: " + buildCyclePath(enablingChain, name));
        }

        enablingChain.push_back(name);
        chainSet.insert(name);

        struct ChainGuard
        {
            std::vector<std::string>& chain;
            std::unordered_set<std::string>& set;
            ~ChainGuard()
            {
                set.erase(chain.back());
                chain.pop_back();
            }
        } guard{enablingChain, chainSet};

        // Plan Requires dependencies first.
        for (const auto& required : node->relationships[relIdx(FeatureRelationship::Requires)])
        {
            auto res = planEnableRecursive(required, plan, enablingChain, chainSet, depth + 1);
            if (!res)
            {
                return res;
            }
        }

        // Plan Implies targets.
        for (const auto& implied : node->relationships[relIdx(FeatureRelationship::Implies)])
        {
            auto res = planEnableRecursive(implied, plan, enablingChain, chainSet, depth + 1);
            if (!res)
            {
                return res;
            }
        }

        // Plan Preempts targets: disable them and their entire reverse-dependency closure.
        for (const auto& preempted : node->relationships[relIdx(FeatureRelationship::Preempts)])
        {
            auto res = planDisableClosure(preempted, plan, depth + 1);
            if (!res)
            {
                return res;
            }
        }

        // Check Conflicts and MutuallyExclusive against desired state.
        for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
        {
            for (const auto& conflicting : node->relationships[relIdx(rel)])
            {
                auto* conflictDesired = plan.desiredStates.find(conflicting);
                if (conflictDesired && *conflictDesired)
                {
                    if (rel == FeatureRelationship::Conflicts)
                    {
                        return unexpected(name + " conflicts with " + conflicting);
                    }
                    return unexpected(name + " is mutually exclusive with " + conflicting);
                }
            }
        }

        // Run per-feature check in the planning phase.
        if (node->check)
        {
            auto checkResult = node->check();
            if (!checkResult)
            {
                return unexpected("Check failed for " + name + ": " + checkResult.error());
            }
        }

        plan.desiredStates[name] = true;
        plan.enableOrder.push_back(name);
        return {};
    }

    /// @brief Plans disabling @p name and recursively disables everything that
    ///        Requires or Implies it (the reverse-dependency closure).
    ///        This is the cascade that makes Preempts safe for e-stop use.
    [[nodiscard]] Expected<void, std::string>
    planDisableClosure(const std::string& name, TransactionPlan& plan, int depth = 0)
    {
        if (static_cast<size_t>(depth) > kMaxValidationDepth)
        {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }

        // Already planned to disable — nothing to do.
        auto* desired = plan.desiredStates.find(name);
        if (!desired || !*desired)
        {
            return {};
        }

        // Walk all features currently desired-enabled and recursively disable any
        // that Require or Imply this one (reverse-dependency closure).
        for (const auto& [otherName, otherNode] : mFeatures)
        {
            auto* otherDesired = plan.desiredStates.find(otherName);
            if (!otherDesired || !*otherDesired || otherName == name)
            {
                continue;
            }

            const bool requiresTarget =
                otherNode.relationships[relIdx(FeatureRelationship::Requires)].count(name) != 0;
            const bool impliesTarget =
                otherNode.relationships[relIdx(FeatureRelationship::Implies)].count(name) != 0;

            if (requiresTarget || impliesTarget)
            {
                auto res = planDisableClosure(otherName, plan, depth + 1);
                if (!res)
                {
                    return res;
                }
            }
        }

        plan.desiredStates[name] = false;
        plan.disableOrder.push_back(name);
        return {};
    }

    /// @brief Builds the ordered list of FeatureChange records from a committed plan.
    [[nodiscard]] std::vector<FeatureChange>
    buildTransactionChanges(const TransactionPlan& plan) const
    {
        std::vector<FeatureChange> changes;
        changes.reserve(plan.disableOrder.size() + plan.enableOrder.size());

        auto appendIfChanged = [&](const std::string& name) {
            auto* orig = plan.originalStates.find(name);
            auto* desired = plan.desiredStates.find(name);
            if (!orig || !desired)
            {
                return;
            }
            if (*orig != *desired)
            {
                changes.push_back({name, *orig, *desired});
            }
        };

        for (const auto& n : plan.disableOrder)
        {
            appendIfChanged(n);
        }
        for (const auto& n : plan.enableOrder)
        {
            appendIfChanged(n);
        }
        return changes;
    }

    /// @brief Validates a desired state snapshot (used by batchEnable before commit).
    ///        Checks all invariants: Requires, Implies, Preempts, Conflicts, MutuallyExclusive.
    [[nodiscard]] Expected<void, std::string>
    validateDesiredState(const FastHashMap<std::string, bool>& desiredStates) const
    {
        for (const auto& [name, node] : mFeatures)
        {
            auto* desired = desiredStates.find(name);
            if (!desired || !*desired)
            {
                continue;
            }

            for (const auto& required : node.relationships[relIdx(FeatureRelationship::Requires)])
            {
                auto* reqDesired = desiredStates.find(required);
                if (!reqDesired || !*reqDesired)
                {
                    return unexpected("'" + name + "' requires '" + required + "' but it's disabled");
                }
            }

            for (const auto& implied : node.relationships[relIdx(FeatureRelationship::Implies)])
            {
                auto* implDesired = desiredStates.find(implied);
                if (!implDesired || !*implDesired)
                {
                    return unexpected("'" + name + "' implies '" + implied + "' but it's disabled");
                }
            }

            for (const auto& preempted : node.relationships[relIdx(FeatureRelationship::Preempts)])
            {
                auto* preemptDesired = desiredStates.find(preempted);
                if (preemptDesired && *preemptDesired)
                {
                    return unexpected("'" + name + "' preempts '" + preempted +
                                      "' but both are desired enabled.");
                }
            }

            for (auto rel : {FeatureRelationship::Conflicts, FeatureRelationship::MutuallyExclusive})
            {
                for (const auto& other : node.relationships[relIdx(rel)])
                {
                    auto* otherDesired = desiredStates.find(other);
                    if (otherDesired && *otherDesired)
                    {
                        if (rel == FeatureRelationship::Conflicts)
                        {
                            return unexpected(name + " conflicts with " + other);
                        }
                        return unexpected(name + " is mutually exclusive with " + other);
                    }
                }
            }
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
                                              const std::vector<FeatureChange>& changes,
                                              bool success)
    {
        for (const auto& entry : sorted)
        {
            entry.callback(requestedFeature, changes, success);
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
     * checking, dependency resolution, Preempts cascade, implies propagation).
     * If validation fails, the object is constructed but marked invalid — check
     * via valid() or operator bool().
     *
     * Destruction attempts to restore each feature to its individual
     * pre-operation state. The intended rollback state is first validated
     * against all graph invariants (Requires, Implies, Conflicts,
     * MutuallyExclusive, Preempts). Only if the full rollback is valid is
     * it committed; otherwise the rollback is skipped entirely, leaving the
     * graph in its current valid state.
     *
     * This means that if external code modifies the graph while the guard is
     * alive in a way that makes the pre-operation state unreachable (e.g. a
     * required dependency is disabled), the guard will not produce an invalid
     * graph on destruction — it simply will not roll back.
     *
     * Rollback only considers features that are still in the state this guard
     * set them to. Features changed by another party after this guard set them
     * are left alone (their new state is respected in the desired-rollback map).
     *
     * Observer notifications fire on rollback (outside the lock) with the correct
     * per-feature direction for each restored feature.
     *
     * @note Non-copyable, non-moveable.
     */
    class ScopedFeatureChange
    {
    private:
        FeatureManager* mManager;
        std::string mRequestedFeature;
        std::vector<FeatureChange> mAppliedChanges; // per-feature: {name, oldState, newState}
        bool mValid;

    public:
        ScopedFeatureChange(FeatureManager& manager, const std::string& featureName, bool enable)
            : mManager(&manager)
            , mRequestedFeature(featureName)
            , mValid(false)
        {
            // Snapshot all states before the operation.
            FastHashMap<std::string, bool> preStates;
            {
                [[maybe_unused]] auto lockGuard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    preStates[name] = node.enabled;
                }
            }

            // Run the validated enable/disable path (handles locking internally).
            Expected<void, std::string> result;
            if (enable)
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
                // Diff pre-op snapshot against current state to build change records.
                [[maybe_unused]] auto lockGuard = mManager->mSync.lock();
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    auto* pre = preStates.find(name);
                    if (pre && *pre != node.enabled)
                    {
                        mAppliedChanges.push_back({name, *pre, node.enabled});
                    }
                }
            }
        }

        ~ScopedFeatureChange()
        {
            if (!mValid || mAppliedChanges.empty())
            {
                return;
            }

            std::vector<FeatureChange> restoredChanges;
            std::vector<ObserverEntry> observersSnapshot;
            std::vector<BatchObserverEntry> batchObserversSnapshot;

            {
                [[maybe_unused]] auto lockGuard = mManager->mSync.lock();

                // Build the desired rollback state from the current live state.
                // Start with every feature at its current (post-external-change) value,
                // then apply rollback only for features that are still in the state this
                // guard set them to (respect independent changes made by other parties).
                FastHashMap<std::string, bool> desiredStates;
                for (const auto& [name, node] : mManager->mFeatures)
                {
                    desiredStates[name] = node.enabled;
                }

                for (const auto& change : mAppliedChanges)
                {
                    auto* current = desiredStates.find(change.name);
                    if (current && *current == change.newState)
                    {
                        desiredStates[change.name] = change.oldState;
                    }
                }

                // Validate the full desired rollback state before committing anything.
                // If external modifications made the pre-operation state unreachable
                // (e.g. a required dependency was disabled while this guard was alive),
                // skip the rollback entirely to avoid producing an invalid graph.
                auto validRes = mManager->validateDesiredState(desiredStates);
                if (!validRes)
                {
                    return;
                }

                // Commit: apply desiredStates to live nodes and record what changed.
                for (auto& [name, node] : mManager->mFeatures)
                {
                    auto* desired = desiredStates.find(name);
                    if (desired && node.enabled != *desired)
                    {
                        restoredChanges.push_back({name, node.enabled, *desired});
                        node.enabled = *desired;
                    }
                }

                if (!restoredChanges.empty())
                {
                    observersSnapshot      = mManager->mObservers;
                    batchObserversSnapshot = mManager->mBatchObservers;
                }
            } // Lock released before observer notification.

            if (!restoredChanges.empty())
            {
                sortObserversByPriority(observersSnapshot);
                for (const auto& change : restoredChanges)
                {
                    notifyObserversSorted(observersSnapshot, change.name, change.newState, true);
                }

                if (!batchObserversSnapshot.empty())
                {
                    sortBatchObserversByPriority(batchObserversSnapshot);
                    notifyBatchObserversSorted(batchObserversSnapshot,
                                               mRequestedFeature,
                                               restoredChanges,
                                               true);
                }
            }
        }

        /// @brief Returns true if the scoped change was applied successfully.
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
     * @note Complexity: O(1) average, O(n) worst case.
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
     * @note Complexity: O(1) average, O(n) worst case.
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
     * Requires, Implies, and Preempts are strictly directional.
     *
     * Adding Preempts is rejected if the source already Requires or Implies the
     * target (contradictory), or if the addition would form a Preempts cycle.
     *
     * @param from Source feature name.
     * @param type Relationship kind (Requires, Implies, Conflicts, MutuallyExclusive, Preempts).
     * @param to   Target feature name.
     * @return Expected<void> on success, or error if either feature does not exist
     *         or a contradiction/cycle would result.
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
     * Uses a plan/commit model: the full desired state (including Preempts cascade
     * disables and Requires/Implies enables) is computed without touching live state,
     * validated completely, then committed atomically. If planning or validation fails,
     * live state is never modified and no rollback is needed.
     *
     * A contradictory batch (e.g. {A, B} where A Preempts B) fails with an error:
     * no API silently leaves a requested root disabled.
     *
     * @param names Features to enable.
     * @return Expected<void> on success, or error on failure (no state change).
     *
     * @note Complexity: O(n * d * log n) where d = dependency depth.
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     */
    [[nodiscard]] Expected<void, std::string> batchEnable(const std::vector<std::string>& names)
    {
        std::vector<FeatureChange> allChanges;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        {
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all root features exist before touching the plan.
            for (const auto& name : names)
            {
                auto nodeRes = getNode(name);
                if (!nodeRes)
                {
                    return unexpected(nodeRes.error());
                }
            }

            // Initialize plan: snapshot live state into both originalStates and desiredStates.
            TransactionPlan plan;
            for (const auto& [name, node] : mFeatures)
            {
                plan.originalStates[name] = node.enabled;
                plan.desiredStates[name]  = node.enabled;
            }

            // Plan each requested root (planning is pure: no live mutation).
            for (const auto& name : names)
            {
                std::vector<std::string> chain;
                std::unordered_set<std::string> chainSet;
                auto res = planEnableRecursive(name, plan, chain, chainSet);
                if (!res)
                {
                    return res;
                }
            }

            // Batch root policy: every explicitly requested feature must be enabled in
            // the final desired state. If a root ended up disabled (e.g. it was preempted
            // by another root in the same batch), the request is contradictory and fails.
            for (const auto& name : names)
            {
                auto* desired = plan.desiredStates.find(name);
                if (!desired || !*desired)
                {
                    return unexpected("Contradictory batch: '" + name +
                                      "' was requested but is disabled in the planned state "
                                      "(preempted by another root in the same batch).");
                }
            }

            // Validate the full desired state before committing anything.
            auto validRes = validateDesiredState(plan.desiredStates);
            if (!validRes)
            {
                return validRes;
            }

            // Commit: apply desiredStates to live nodes.
            for (auto&& [name, node] : mFeatures)
            {
                auto* desired = plan.desiredStates.find(name);
                if (desired)
                {
                    node.enabled = *desired;
                }
            }

            allChanges = buildTransactionChanges(plan);

            if (!allChanges.empty())
            {
                observersSnapshot      = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released.

        // Notify observers outside the lock.
        if (!allChanges.empty())
        {
            sortObserversByPriority(observersSnapshot);
            for (const auto& change : allChanges)
            {
                notifyObserversSorted(observersSnapshot, change.name, change.newState, true);
            }

            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot,
                                           names.empty() ? "" : names[0],
                                           allChanges,
                                           true);
            }
        }

        return {};
    }

    /**
     * @brief Disables multiple features atomically with constraint checking.
     *
     * Validates that no remaining enabled feature Requires or Implies any of the
     * features being disabled. Uses a plan/commit model consistent with batchEnable:
     * desired state is computed, validated, then committed. No partial state is
     * ever visible to observers.
     * Duplicate names in the input are automatically deduplicated.
     *
     * @param names Features to disable.
     * @return Expected<void> on success, or error (no state change) on failure.
     *
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     */
    [[nodiscard]] Expected<void, std::string> batchDisable(const std::vector<std::string>& names)
    {
        // Deduplicate requested names while preserving first-seen order.
        std::vector<std::string> uniqueNames;
        uniqueNames.reserve(names.size());
        {
            std::unordered_set<std::string> seen;
            for (const auto& n : names)
            {
                if (seen.insert(n).second)
                {
                    uniqueNames.push_back(n);
                }
            }
        }

        std::vector<FeatureChange> allChanges;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        {
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate all features exist.
            for (const auto& name : uniqueNames)
            {
                auto nodeRes = getNode(name);
                if (!nodeRes)
                {
                    return unexpected(nodeRes.error());
                }
            }

            // Initialize plan.
            TransactionPlan plan;
            for (const auto& [name, node] : mFeatures)
            {
                plan.originalStates[name] = node.enabled;
                plan.desiredStates[name]  = node.enabled;
            }

            // Mark each requested feature disabled in the desired state.
            for (const auto& name : uniqueNames)
            {
                auto* desired = plan.desiredStates.find(name);
                if (desired && *desired)
                {
                    plan.desiredStates[name] = false;
                    plan.disableOrder.push_back(name);
                }
            }

            // Validate: no remaining desired-enabled feature may Require or Imply
            // any of the features we are disabling.
            for (const auto& [featureName, node] : mFeatures)
            {
                auto* featureDesired = plan.desiredStates.find(featureName);
                if (!featureDesired || !*featureDesired)
                {
                    continue;
                }

                for (const auto& required : node.relationships[relIdx(FeatureRelationship::Requires)])
                {
                    auto* reqDesired = plan.desiredStates.find(required);
                    if (reqDesired && !*reqDesired)
                    {
                        return unexpected("Cannot disable '" + required +
                                          "': required by enabled feature '" + featureName + "'");
                    }
                }

                for (const auto& implied : node.relationships[relIdx(FeatureRelationship::Implies)])
                {
                    auto* implDesired = plan.desiredStates.find(implied);
                    if (implDesired && !*implDesired)
                    {
                        // Check if this implied feature was in our disable set.
                        bool inDisableSet = false;
                        for (const auto& n : uniqueNames)
                        {
                            if (n == implied)
                            {
                                inDisableSet = true;
                                break;
                            }
                        }
                        if (inDisableSet)
                        {
                            return unexpected("Cannot disable '" + implied + "': implied by enabled feature '" +
                                              featureName + "'. Disable '" + featureName + "' first.");
                        }
                    }
                }
            }

            // Commit.
            for (auto&& [name, node] : mFeatures)
            {
                auto* desired = plan.desiredStates.find(name);
                if (desired)
                {
                    node.enabled = *desired;
                }
            }

            allChanges = buildTransactionChanges(plan);

            if (!allChanges.empty())
            {
                observersSnapshot      = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released.

        if (!allChanges.empty())
        {
            sortObserversByPriority(observersSnapshot);
            for (const auto& change : allChanges)
            {
                notifyObserversSorted(observersSnapshot, change.name, change.newState, true);
            }

            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot,
                                           uniqueNames.empty() ? "" : uniqueNames[0],
                                           allChanges,
                                           true);
            }
        }

        return {};
    }

    /**
     * @brief Atomically swaps one enabled feature for another within a MutuallyExclusive group.
     *
     * Disables @p from (and its reverse-dependency closure) and enables @p to (and its
     * Requires/Implies closure) in a single plan/commit transaction. Because @p from is
     * marked disabled in the plan before the MutuallyExclusive constraint is evaluated,
     * the constraint check sees the correct end-state and the substitution succeeds.
     *
     * @p from and @p to do not need to share a MutuallyExclusive relationship; the method
     * works for any pair. The MutuallyExclusive constraint is the common motivation.
     *
     * @param from Feature that must currently be enabled. Returns an error if it is not.
     * @param to   Feature to enable.
     * @return Expected<void> on success, or error on failure (no state change).
     *
     * @note Error policy: returns an error if @p from is not currently enabled. Callers
     *       should check isEnabled(from) before calling if conditional behaviour is needed.
     *       Returns an error if @p from == @p to.
     *
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     * @see forceExclusive() for e-stop / clear-everything semantics.
     */
    [[nodiscard]] Expected<void, std::string>
    replace(const std::string& from, const std::string& to)
    {
        std::vector<FeatureChange> allChanges;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        {
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate both features exist.
            auto fromRes = getNode(from);
            if (!fromRes)
            {
                return unexpected(fromRes.error());
            }
            auto toRes = getNode(to);
            if (!toRes)
            {
                return unexpected(toRes.error());
            }

            // Reject self-replace: no observable effect, and likely a caller bug.
            if (from == to)
            {
                return unexpected("replace: 'from' and 'to' are the same feature ('" + from + "').");
            }

            // Seed plan from live state (identical to every other transactional method).
            TransactionPlan plan;
            for (const auto& [name, node] : mFeatures)
            {
                plan.originalStates[name] = node.enabled;
                plan.desiredStates[name]  = node.enabled;
            }

            // Require that 'from' is currently enabled. Silent no-op would hide caller bugs.
            auto* fromState = plan.desiredStates.find(from);
            if (!fromState || !*fromState)
            {
                return unexpected("replace: '" + from + "' is not currently enabled. "
                                  "Check isEnabled(from) before calling replace() if conditional "
                                  "behaviour is needed.");
            }

            // Step 1: Disable 'from' and its reverse-dependency closure in the plan.
            // After this, from is false in desiredStates, so the MutuallyExclusive check
            // in planEnableRecursive will not see it as a conflicting enabled feature.
            auto disableRes = planDisableClosure(from, plan);
            if (!disableRes)
            {
                return disableRes;
            }

            // Step 2: Enable 'to' and its Requires/Implies closure.
            // The MutuallyExclusive check evaluates desiredStates, where 'from' is now false.
            std::vector<std::string> chain;
            std::unordered_set<std::string> chainSet;
            auto enableRes = planEnableRecursive(to, plan, chain, chainSet);
            if (!enableRes)
            {
                return enableRes;
            }

            // Guard: 'to' must be enabled in the final plan (same as batchEnable root policy).
            auto* toDesired = plan.desiredStates.find(to);
            if (!toDesired || !*toDesired)
            {
                return unexpected("replace: '" + to + "' is disabled in the planned state "
                                  "(contradictory request).");
            }

            // Final consistency check across the full desired state.
            auto validRes = validateDesiredState(plan.desiredStates);
            if (!validRes)
            {
                return validRes;
            }

            // Commit.
            for (auto&& [name, node] : mFeatures)
            {
                auto* desired = plan.desiredStates.find(name);
                if (desired)
                {
                    node.enabled = *desired;
                }
            }

            allChanges = buildTransactionChanges(plan);

            if (!allChanges.empty())
            {
                observersSnapshot      = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released.

        // Notify observers outside the lock. requestedFeature = 'to' per plan §4.
        if (!allChanges.empty())
        {
            sortObserversByPriority(observersSnapshot);
            for (const auto& change : allChanges)
            {
                notifyObserversSorted(observersSnapshot, change.name, change.newState, true);
            }

            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot, to, allChanges, true);
            }
        }

        return {};
    }

    /**
     * @brief Disables every feature not in @p feature's Requires/Implies closure, atomically.
     *
     * Implements "e-stop" or "exclusive activation" semantics: all currently enabled features
     * are cleared from the plan before @p feature and its Requires/Implies closure are planned.
     * Because desiredStates starts all-false, no MutuallyExclusive or Conflicts constraint can
     * fire against another enabled feature — the only possible failure is an internal Conflicts
     * edge within @p feature's own Requires closure, which would indicate a broken graph.
     *
     * If @p feature is already the only enabled feature (or is disabled with nothing else
     * enabled), buildTransactionChanges produces an empty change vector and no observers fire.
     *
     * @param feature Feature to activate exclusively. Its full Requires/Implies closure
     *                is preserved; everything else is disabled.
     * @return Expected<void> on success, or error on failure (no state change).
     *
     * @note Thread-safety: Acquires internal lock; observers called outside lock.
     * @see replace() for targeted A→B substitution within a MutuallyExclusive group.
     */
    [[nodiscard]] Expected<void, std::string>
    forceExclusive(const std::string& feature)
    {
        std::vector<FeatureChange> allChanges;
        std::vector<ObserverEntry> observersSnapshot;
        std::vector<BatchObserverEntry> batchObserversSnapshot;

        {
            [[maybe_unused]] auto guard = mSync.lock();

            // Validate the target feature exists.
            auto nodeRes = getNode(feature);
            if (!nodeRes)
            {
                return unexpected(nodeRes.error());
            }

            // Seed plan: snapshot live state into originalStates.
            TransactionPlan plan;
            for (const auto& [name, node] : mFeatures)
            {
                plan.originalStates[name] = node.enabled;
            }

            // Zero desiredStates completely: every feature starts as disabled.
            // Simultaneously populate disableOrder with all currently live-enabled features
            // so that buildTransactionChanges produces correct disable records.
            // Features subsequently re-enabled by planEnableRecursive will be removed from
            // net-change consideration by the originalStates diff in appendIfChanged.
            for (const auto& [name, node] : mFeatures)
            {
                plan.desiredStates[name] = false;
                if (node.enabled)
                {
                    plan.disableOrder.push_back(name);
                }
            }

            // Plan the target feature from a blank slate.
            // No MutuallyExclusive or Conflicts check can find a conflicting enabled feature
            // because every entry in desiredStates is currently false.
            std::vector<std::string> chain;
            std::unordered_set<std::string> chainSet;
            auto enableRes = planEnableRecursive(feature, plan, chain, chainSet);
            if (!enableRes)
            {
                return enableRes;
            }

            // Final consistency check.
            auto validRes = validateDesiredState(plan.desiredStates);
            if (!validRes)
            {
                return validRes;
            }

            // Commit.
            for (auto&& [name, node] : mFeatures)
            {
                auto* desired = plan.desiredStates.find(name);
                if (desired)
                {
                    node.enabled = *desired;
                }
            }

            allChanges = buildTransactionChanges(plan);

            if (!allChanges.empty())
            {
                observersSnapshot      = mObservers;
                batchObserversSnapshot = mBatchObservers;
            }
        } // Lock released.

        // Notify observers outside the lock. requestedFeature = 'feature' per plan §4.
        if (!allChanges.empty())
        {
            sortObserversByPriority(observersSnapshot);
            for (const auto& change : allChanges)
            {
                notifyObserversSorted(observersSnapshot, change.name, change.newState, true);
            }

            if (!batchObserversSnapshot.empty())
            {
                sortBatchObserversByPriority(batchObserversSnapshot);
                notifyBatchObserversSorted(batchObserversSnapshot, feature, allChanges, true);
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
    [[nodiscard]] ObserverId addObserver(FeatureObserver callback, int priority = 0)
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
     * @param callback Function receiving (requestedFeature, changes, success).
     * @param priority Ordering priority (higher = called first, default 0).
     * @return ObserverId for later removal via removeObserver().
     *
     * @note Thread-safety: Acquires internal lock; callbacks called outside lock.
     * @see removeObserver(), ScopedBatchObserver
     */
    [[nodiscard]] ObserverId addBatchObserver(BatchObserver callback, int priority = 0)
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
        // Requires, Implies, and Preempts are strictly directional and must NOT be symmetrized:
        // "A Preempts B" must never imply "B Preempts A".
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
                    case FeatureRelationship::Preempts:
                        style = "bold";
                        arrow = "tee";
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
                    ss << "    \"" << name << "\" -> \"" << target << "\" [style=" << style
                       << ", arrowhead=" << arrow;
                    if (type == FeatureRelationship::Preempts)
                    {
                        ss << ", color=red";
                    }
                    ss << ", label=\"" << typeStr << "\"];\n";
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
