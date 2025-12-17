// FeatureManager.h - Type-safe feature flag management with dependency resolution
//
// A modern C++17 header-only library for managing feature flags with complex dependencies,
// relationships, and validation. Designed for scenarios where features have interdependencies
// (Requires, Implies, Conflicts, MutuallyExclusive) and need automatic resolution.
//
// Key features:
// - Cycle detection with detailed error messages showing full dependency path
// - Pluggable thread-safety policies (single-threaded, mutex, spinlock, shared_mutex)
// - Type-safe group states with custom enums
// - Observer pattern with priority ordering and RAII lifetime management
// - JSON and GraphViz DOT serialization
// - RAII helpers for scoped state changes
// - Optimized with SortedContainer for relationship storage (cache-friendly)
//
// Performance characteristics:
// - Add feature: O(log n)
// - Enable/disable: O(d x log n) where d = dependency depth (limited to MAX_VALIDATION_DEPTH)
// - Validate: O(n x d x log n)
// - Memory: ~550 bytes per feature with 5 relationships (using SortedContainer)
//

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <stdexcept>
#include <sstream>
#include <utility>
#include <regex>
#include <algorithm>
#include <cstdint>

#include "Expected.h"
#include "ConcurrencyPolicies.h"
#include "JsonLite.h"
#include "ValueGuard.h"
#include "Stringify.h"
#include "EnumPlus.h"
#include "SortedContainer.h"
#include "Factory.h"

namespace fat_p {

// ============================================================================
// Enum Definitions
// ============================================================================

// Relationship types between features
enum class FeatureRelationship {
    Requires,             // This feature requires another to be enabled
    Conflicts,            // This feature conflicts with another (mutual)
    Implies,              // Enabling this implies enabling another
    MutuallyExclusive     // For groups: all features in set conflict with each other
};

// Default built-in group state enum
enum class FeatureGroupState {
    Inactive,    // No features enabled
    Partial,     // Some but not all features enabled/valid
    Active,      // All required features enabled, no conflicts, all checks pass
    Invalid      // Conflicts or failed checks
};

// ============================================================================
// EnumPlus Specializations
// ============================================================================

template<>
struct EnableOverloadedOperators<FeatureRelationship> {
    static constexpr bool value = true;
};

template<>
struct EnumStringPolicy<FeatureRelationship> {
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {
        "Requires", "Conflicts", "Implies", "MutuallyExclusive"
    };
    
    static std::string_view to_string(FeatureRelationship e) {
        return names[static_cast<size_t>(e)];
    }
    
    static FeatureRelationship from_string(std::string_view str) {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end()) {
            throw std::invalid_argument("Invalid FeatureRelationship string");
        }
        return static_cast<FeatureRelationship>(std::distance(names.begin(), it));
    }
};

template<>
struct EnumStringPolicy<FeatureGroupState> {
    static constexpr bool has_names = true;
    static constexpr std::array<std::string_view, 4> names = {
        "Inactive", "Partial", "Active", "Invalid"
    };
    
    static std::string_view to_string(FeatureGroupState e) {
        return names[static_cast<size_t>(e)];
    }
    
    static FeatureGroupState from_string(std::string_view str) {
        auto it = std::find(names.begin(), names.end(), str);
        if (it == names.end()) {
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
using FeatureCheck = std::function<Expected<void, std::string>()>;

// Observer callback type: Called on feature state changes
using FeatureObserver = std::function<void(const std::string& feature_name,
                                           bool new_state,
                                           bool success)>;

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
struct FeatureGroupStatePolicy {
    using state_type = StateEnum;
    static state_type compute(const std::set<std::string>& group_features,
                              size_t enabled_count,
                              bool has_conflict,
                              bool all_checks_pass) {
        if (group_features.empty()) {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (has_conflict || !all_checks_pass) {
            return static_cast<state_type>(FeatureGroupState::Invalid);
        }
        if (enabled_count == 0) {
            return static_cast<state_type>(FeatureGroupState::Inactive);
        }
        if (enabled_count < group_features.size()) {
            return static_cast<state_type>(FeatureGroupState::Partial);
        }
        return static_cast<state_type>(FeatureGroupState::Active);
    }
};

// Function type for custom state computation
template <typename StateEnum>
using StateComputer = std::function<StateEnum(const std::set<std::string>&,
                                              size_t,
                                              bool,
                                              bool)>;

// ============================================================================
// FeatureCheck Callback Factory
// ============================================================================

// Global factory for FeatureCheck callbacks
// Key: string identifier, Product: FeatureCheck
using FeatureCheckFactory = SimpleFactory<std::string, FeatureCheck>;

// Singleton accessor for the global FeatureCheck factory
inline FeatureCheckFactory& get_feature_check_factory() {
    static FeatureCheckFactory factory;
    return factory;
}

// RAII helper class for automatic registration/unregistration of FeatureCheck callbacks
// This allows modules to register their checks independently and have them automatically
// cleaned up when the registration object goes out of scope
class FeatureCheckRegistration {
public:
    // Register a FeatureCheck creator function with the given key
    // The creator is a factory function that returns a FeatureCheck
    FeatureCheckRegistration(const std::string& key, 
                            std::function<FeatureCheck()> creator) 
        : key_(key) {
        [[maybe_unused]] bool registered = 
            get_feature_check_factory().registerType(key_, std::move(creator));
    }
    
    // Automatically unregister on destruction
    ~FeatureCheckRegistration() {
        if (!key_.empty()) {
            [[maybe_unused]] bool unregistered = 
                get_feature_check_factory().unregisterType(key_);
        }
    }
    
    // Non-copyable
    FeatureCheckRegistration(const FeatureCheckRegistration&) = delete;
    FeatureCheckRegistration& operator=(const FeatureCheckRegistration&) = delete;
    
    // Moveable
    FeatureCheckRegistration(FeatureCheckRegistration&& other) noexcept
        : key_(std::move(other.key_)) {
        other.key_.clear();
    }
    
    FeatureCheckRegistration& operator=(FeatureCheckRegistration&& other) noexcept {
        if (this != &other) {
            if (!key_.empty()) {
                (void)get_feature_check_factory().unregisterType(key_);
            }
            key_ = std::move(other.key_);
            other.key_.clear();
        }
        return *this;
    }

private:
    std::string key_;
};

// ============================================================================
// FeatureNode Structure
// ============================================================================

// Internal representation of a feature with its state and relationships
struct FeatureNode {
    bool enabled = false;
    FeatureCheck check;
    std::string check_key;  // For serialization: the factory key to restore check on load
    
    // Relationship storage using SortedContainer for cache efficiency
    // Use std::map for relationship types (only 4 possible keys)
    // Use SortedContainer for target sets (frequently iterated, rarely modified)
    std::map<FeatureRelationship, SortedContainer<std::string>> relationships;

    JsonValue to_json() const {
        JsonObject obj;
        obj["enabled"] = JsonValue{enabled};
        if (!check_key.empty()) {
            obj["check_key"] = JsonValue{check_key};
        }
        for (const auto& [type, targets] : relationships) {
            JsonArray arr;
            for (const auto& target : targets) {
                arr.push_back(JsonValue{target});
            }
            std::string type_name(EnumStringPolicy<FeatureRelationship>::to_string(type));
            obj[type_name] = JsonValue{std::move(arr)};
        }
        return JsonValue{std::move(obj)};
    }

    static Expected<FeatureNode, std::string> from_json(const JsonValue& value) {
        if (!value.is_object()) {
            return unexpected("FeatureNode JSON must be an object");
        }
        const auto& obj = std::get<JsonObject>(value);
        FeatureNode node;

        auto it = obj.find("enabled");
        if (it != obj.end()) {
            if (!it->second.is_bool()) {
                return unexpected("enabled must be boolean");
            }
            node.enabled = std::get<bool>(it->second);
        }

        it = obj.find("check_key");
        if (it != obj.end()) {
            if (!it->second.is_string()) {
                return unexpected("check_key must be string");
            }
            node.check_key = std::get<std::string>(it->second);
            
            // Look up callback from factory
            auto check_result = get_feature_check_factory().make(node.check_key);
            if (check_result) {
                node.check = *check_result;
            }
            // Note: If lookup fails, check remains empty (feature will have no validation)
            // This allows deserialization to succeed even if callbacks aren't registered yet
        }

        std::array<std::string_view, 4> types = {
            "Requires", "Conflicts", "Implies", "MutuallyExclusive"
        };
        for (const auto& type_str : types) {
            std::string ts(type_str);
            it = obj.find(ts);
            if (it != obj.end()) {
                if (!it->second.is_array()) {
                    return unexpected(ts + " must be array");
                }
                const auto& arr = std::get<JsonArray>(it->second);
                FeatureRelationship type = 
                    EnumStringPolicy<FeatureRelationship>::from_string(type_str);
                for (const auto& elem : arr) {
                    if (!elem.is_string()) {
                        return unexpected("Element in " + ts + " must be string");
                    }
                    auto insert_res = node.relationships[type].insert(
                        std::get<std::string>(elem));
                    if (!insert_res) {
                        return unexpected("Failed to insert relationship: " + 
                                         insert_res.error());
                    }
                }
            }
        }
        return node;
    }
};

// Base class for type-erased group storage
struct FeatureGroupInfoBase {
    virtual ~FeatureGroupInfoBase() = default;
    virtual std::set<std::string> get_features() const = 0;
    virtual JsonValue to_json() const = 0;
    virtual std::string state_to_string() const = 0;
};

// Concrete group info with type-safe state computer
template <typename StateEnum = FeatureGroupState>
struct FeatureGroupInfo : public FeatureGroupInfoBase {
    std::set<std::string> features;
    StateComputer<StateEnum> state_computer;
    mutable StateEnum cached_state;

    FeatureGroupInfo(const std::vector<std::string>& f,
                     StateComputer<StateEnum> comp = FeatureGroupStatePolicy<StateEnum>::compute)
        : features(f.begin(), f.end())
        , state_computer(comp)
        , cached_state(static_cast<StateEnum>(FeatureGroupState::Inactive)) {}

    std::set<std::string> get_features() const override { return features; }

    JsonValue to_json() const override {
        JsonArray arr;
        for (const auto& f : features) {
            arr.push_back(JsonValue{f});
        }
        return JsonValue{std::move(arr)};
    }

    std::string state_to_string() const override {
        if constexpr (EnumStringPolicy<StateEnum>::has_names) {
            return std::string(EnumStringPolicy<StateEnum>::to_string(cached_state));
        } else {
            return toString(cached_state);
        }
    }

    void update_cached_state(StateEnum state) const { cached_state = state; }
};

// ============================================================================
// FeatureManager Class
// ============================================================================

template <typename SyncPolicy = SingleThreadedPolicy>
class FeatureManager {
private:
    // Internal observer storage with unique ID for removal support
    struct ObserverEntry {
        ObserverId id;
        int priority;
        FeatureObserver callback;
    };
    
    struct BatchObserverEntry {
        ObserverId id;
        int priority;
        BatchObserver callback;
    };

    std::map<std::string, FeatureNode> features_;
    std::map<std::string, std::unique_ptr<FeatureGroupInfoBase>> groups_;
    std::vector<ObserverEntry> observers_;
    std::vector<BatchObserverEntry> batch_observers_;
    ObserverId next_observer_id_ = 1;
    mutable SyncPolicy sync_;
    
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

    Expected<FeatureNode*, std::string> get_node(const std::string& name) {
        auto it = features_.find(name);
        if (it == features_.end()) {
            return unexpected("Feature not found: " + name);
        }
        return &(it->second);
    }

    Expected<const FeatureNode*, std::string> get_node(const std::string& name) const {
        auto it = features_.find(name);
        if (it == features_.end()) {
            return unexpected("Feature not found: " + name);
        }
        return &(it->second);
    }
    
    // Build a human-readable cycle path from the enabling chain
    // The enabling_chain preserves insertion order (the actual traversal path)
    // Example: enabling_chain = {"A", "B", "C"}, target = "A"
    // Returns: "A -> B -> C -> A" 
    std::string build_cycle_path(const std::vector<std::string>& enabling_chain, 
                                  const std::string& target) const {
        if (enabling_chain.empty()) {
            return target + " (self-referencing)";
        }
        
        // Build ordered path: the vector preserves actual traversal order
        std::string path;
        bool started = false;
        
        for (const auto& feature : enabling_chain) {
            if (!started) {
                path = feature;
                started = true;
            } else {
                path += " -> " + feature;
            }
        }
        path += " -> " + target;
        return path;
    }

    Expected<void, std::string> validate_feature(const std::string& name,
                                                  std::vector<std::string>& visited,
                                                  int depth = 0) const {
        if (static_cast<size_t>(depth) > MAX_VALIDATION_DEPTH) {
            return unexpected("Maximum validation depth exceeded at feature: " + name);
        }
        
        // Helper to check if name is in visited path
        auto in_visited = [&visited](const std::string& n) {
            return std::find(visited.begin(), visited.end(), n) != visited.end();
        };
        
        if (in_visited(name)) {
            return unexpected("Circular dependency detected: " + 
                             build_cycle_path(visited, name));
        }

        auto node_res = get_node(name);
        if (!node_res) {
            return unexpected(node_res.error());
        }
        const FeatureNode* node = *node_res;

        if (!node->enabled) {
            return {};
        }

        visited.push_back(name);

        // Check all enabled features that this depends on
        if (node->relationships.count(FeatureRelationship::Requires)) {
            const auto& targets = node->relationships.at(FeatureRelationship::Requires);
            for (const auto& required : targets) {
                auto req_node_res = get_node(required);
                if (!req_node_res) {
                    return unexpected("Required feature not found: " + required);
                }
                const FeatureNode* req_node = *req_node_res;
                if (!req_node->enabled) {
                    return unexpected(name + " requires " + required + " to be enabled");
                }
                auto res = validate_feature(required, visited, depth + 1);
                if (!res) {
                    return res;
                }
            }
        }

        // Check for conflicts
        for (auto type : {FeatureRelationship::Conflicts, 
                          FeatureRelationship::MutuallyExclusive}) {
            if (node->relationships.count(type)) {
                const auto& targets = node->relationships.at(type);
                for (const auto& conflicting : targets) {
                    auto conf_node_res = get_node(conflicting);
                    if (conf_node_res) {
                        const FeatureNode* conf_node = *conf_node_res;
                        if (conf_node->enabled) {
                            return unexpected(name + " conflicts with " + conflicting);
                        }
                    }
                }
            }
        }

        // Run validation check
        if (node->check) {
            auto check_result = node->check();
            if (!check_result) {
                return unexpected("Check failed for " + name + ": " + check_result.error());
            }
        }

        visited.pop_back();
        return {};
    }

    Expected<void, std::string> enable_feature(const std::string& name,
                                                std::vector<std::string>& enabling_chain,
                                                std::vector<std::string>* changed_features,
                                                int depth = 0) {
        if (static_cast<size_t>(depth) > MAX_VALIDATION_DEPTH) {
            return unexpected("Maximum dependency depth exceeded at feature: " + name);
        }
        
        // Helper to check if name is in the chain (preserves order for cycle path reporting)
        auto in_chain = [&enabling_chain](const std::string& n) {
            return std::find(enabling_chain.begin(), enabling_chain.end(), n) != enabling_chain.end();
        };
        
        // Check for circular dependencies first
        if (in_chain(name)) {
            return unexpected("Circular dependency detected: " + 
                             build_cycle_path(enabling_chain, name));
        }

        auto node_res = get_node(name);
        if (!node_res) {
            return unexpected(node_res.error());
        }
        FeatureNode* node = *node_res;
        
        if (node->enabled) {
            return {};  // Already enabled - nothing to do
        }

        // Track that we're in the process of enabling this feature
        enabling_chain.push_back(name);

        // Enable this feature first (may be rolled back on error)
        bool was_enabled = node->enabled;
        node->enabled = true;

        // Process Required relationships (recursively enable dependencies)
        if (node->relationships.count(FeatureRelationship::Requires)) {
            const auto& targets = node->relationships[FeatureRelationship::Requires];
            for (const auto& required : targets) {
                // Check for circular dependency before recursing
                if (in_chain(required)) {
                    node->enabled = was_enabled;
                    return unexpected("Circular dependency detected: " + 
                                    build_cycle_path(enabling_chain, required));
                }
                
                auto req_node_res = get_node(required);
                if (!req_node_res) { 
                    node->enabled = false; 
                    return unexpected("Required feature not found: " + required); 
                }
                FeatureNode* req_node = *req_node_res;
                if (!req_node->enabled) {
                    auto enable_res = enable_feature(required, enabling_chain, 
                                                      changed_features, depth + 1);
                    if (!enable_res) { 
                        node->enabled = false; 
                        return enable_res; 
                    }
                }
            }
        }

        // Process Implies relationships
        if (node->relationships.count(FeatureRelationship::Implies)) {
            const auto& targets = node->relationships[FeatureRelationship::Implies];
            for (const auto& implied : targets) {
                // Check for circular dependency before checking if already enabled
                if (in_chain(implied)) {
                    node->enabled = false;
                    return unexpected("Circular dependency detected: " + 
                                    build_cycle_path(enabling_chain, implied));
                }
                
                auto impl_node_res = get_node(implied);
                if (impl_node_res) {
                    FeatureNode* impl_node = *impl_node_res;
                    if (!impl_node->enabled) {
                        auto enable_res = enable_feature(implied, enabling_chain, 
                                                          changed_features, depth + 1);
                        if (!enable_res) { 
                            node->enabled = false; 
                            return enable_res; 
                        }
                    }
                }
            }
        }

        // Check for conflicts
        for (auto type : {FeatureRelationship::Conflicts, 
                          FeatureRelationship::MutuallyExclusive}) {
            if (node->relationships.count(type)) {
                const auto& targets = node->relationships[type];
                for (const auto& conflicting : targets) {
                    auto conf_node_res = get_node(conflicting);
                    if (conf_node_res) {
                        FeatureNode* conf_node = *conf_node_res;
                        if (conf_node->enabled) { 
                            node->enabled = false; 
                            return unexpected(name + " conflicts with " + conflicting); 
                        }
                    }
                }
            }
        }

        // Run validation check
        if (node->check) {
            auto check_result = node->check();
            if (!check_result) { 
                node->enabled = false; 
                return unexpected("Check failed for " + name + ": " + check_result.error()); 
            }
        }

        // Remove this feature from the chain (stack-like: last in, first out)
        enabling_chain.pop_back();
        
        // Track this feature as changed (for observer notification)
        if (changed_features && !was_enabled) {
            changed_features->push_back(name);
        }
        
        return {};
    }

    void notify_observers(const std::string& feature_name, bool new_state, bool success) {
        // Sort by priority (higher first) - stable sort preserves insertion order
        std::vector<ObserverEntry*> sorted;
        sorted.reserve(observers_.size());
        for (auto& entry : observers_) {
            sorted.push_back(&entry);
        }
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const ObserverEntry* a, const ObserverEntry* b) {
                return a->priority > b->priority;
            });
        
        for (auto* entry : sorted) {
            entry->callback(feature_name, new_state, success);
        }
    }
    
    void notify_batch_observers(const std::string& requested_feature,
                                 const std::vector<std::string>& all_changed,
                                 bool enabled,
                                 bool success) {
        std::vector<BatchObserverEntry*> sorted;
        sorted.reserve(batch_observers_.size());
        for (auto& entry : batch_observers_) {
            sorted.push_back(&entry);
        }
        std::stable_sort(sorted.begin(), sorted.end(),
            [](const BatchObserverEntry* a, const BatchObserverEntry* b) {
                return a->priority > b->priority;
            });
        
        for (auto* entry : sorted) {
            entry->callback(requested_feature, all_changed, enabled, success);
        }
    }

    template <typename StateEnum>
    Expected<StateEnum, std::string> compute_group_state_impl(
        const std::string& group_name) const {
        auto git = groups_.find(group_name);
        if (git == groups_.end()) {
            return unexpected("Group not found: " + group_name);
        }
        auto* group_ptr = dynamic_cast<FeatureGroupInfo<StateEnum>*>(git->second.get());
        if (!group_ptr) {
            return unexpected("Type mismatch: group '" + group_name + 
                            "' is not of the requested state type");
        }
        const auto& group_features = group_ptr->features;
        size_t enabled_count = 0;
        bool has_conflict = false;
        bool all_checks_pass = true;
        for (const auto& feature_name : group_features) {
            auto node_res = get_node(feature_name);
            if (!node_res) {
                continue;
            }
            const FeatureNode* node = *node_res;
            if (node->enabled) {
                ++enabled_count;
                // Check for conflicts within group
                for (const auto& other : group_features) {
                    if (other == feature_name) {
                        continue;
                    }
                    if (node->relationships.count(FeatureRelationship::Conflicts)) {
                        const auto& targets = 
                            node->relationships.at(FeatureRelationship::Conflicts);
                        if (targets.count(other) > 0) {
                            auto other_node_res = get_node(other);
                            if (other_node_res && (*other_node_res)->enabled) {
                                has_conflict = true;
                            }
                        }
                    }
                    if (node->relationships.count(FeatureRelationship::MutuallyExclusive)) {
                        const auto& targets = 
                            node->relationships.at(FeatureRelationship::MutuallyExclusive);
                        if (targets.count(other) > 0) {
                            auto other_node_res = get_node(other);
                            if (other_node_res && (*other_node_res)->enabled) {
                                has_conflict = true;
                            }
                        }
                    }
                }
                if (node->check) {
                    auto check_result = node->check();
                    if (!check_result) {
                        all_checks_pass = false;
                    }
                }
            }
        }
        StateEnum state = group_ptr->state_computer(group_features, enabled_count, 
                                                     has_conflict, all_checks_pass);
        group_ptr->update_cached_state(state);
        return state;
    }

public:
    FeatureManager() = default;
    
    FeatureManager(FeatureManager&& other) noexcept 
        : features_(std::move(other.features_))
        , groups_(std::move(other.groups_))
        , observers_(std::move(other.observers_))
        , batch_observers_(std::move(other.batch_observers_))
        , next_observer_id_(other.next_observer_id_)
        , sync_() {}
    
    FeatureManager& operator=(FeatureManager&& other) noexcept {
        if (this != &other) {
            features_ = std::move(other.features_);
            groups_ = std::move(other.groups_);
            observers_ = std::move(other.observers_);
            batch_observers_ = std::move(other.batch_observers_);
            next_observer_id_ = other.next_observer_id_;
            sync_ = SyncPolicy();
        }
        return *this;
    }

    // RAII helper for temporary feature changes
    class ScopedFeatureChange {
    private:
        FeatureManager* manager_;
        std::string feature_name_;
        bool original_state_;
        bool valid_;
    public:
        ScopedFeatureChange(FeatureManager& manager,
                           const std::string& feature_name,
                           bool new_state)
            : manager_(&manager)
            , feature_name_(feature_name)
            , valid_(false) {
            typename SyncPolicy::LockGuard guard(manager_->sync_.getLock());
            auto node_res = manager_->get_node(feature_name);
            if (node_res) {
                FeatureNode* node = *node_res;
                original_state_ = node->enabled;
                valid_ = true;
                if (new_state && !node->enabled) {
                    node->enabled = true;
                } else if (!new_state && node->enabled) {
                    node->enabled = false;
                }
            }
        }
        
        ~ScopedFeatureChange() {
            if (valid_) {
                typename SyncPolicy::LockGuard guard(manager_->sync_.getLock());
                auto node = manager_->get_node(feature_name_);
                if (node) {
                    (*node)->enabled = original_state_;
                    // Best-effort validation during cleanup
                    // Cannot throw from destructor
                    auto validate_res = manager_->validate();
                    (void)validate_res;
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
    class ScopedObserver {
    private:
        FeatureManager* manager_;
        ObserverId id_;
        
    public:
        ScopedObserver(FeatureManager& manager, FeatureObserver callback, int priority = 0)
            : manager_(&manager)
            , id_(manager.add_observer(std::move(callback), priority)) {}
        
        ~ScopedObserver() {
            if (manager_ && id_ != 0) {
                manager_->remove_observer(id_);
            }
        }
        
        // Non-copyable
        ScopedObserver(const ScopedObserver&) = delete;
        ScopedObserver& operator=(const ScopedObserver&) = delete;
        
        // Moveable
        ScopedObserver(ScopedObserver&& other) noexcept
            : manager_(std::exchange(other.manager_, nullptr))
            , id_(std::exchange(other.id_, 0)) {}
        
        ScopedObserver& operator=(ScopedObserver&& other) noexcept {
            if (this != &other) {
                if (manager_ && id_ != 0) {
                    manager_->remove_observer(id_);
                }
                manager_ = std::exchange(other.manager_, nullptr);
                id_ = std::exchange(other.id_, 0);
            }
            return *this;
        }
        
        // Get the observer ID (for manual removal if needed)
        ObserverId id() const { return id_; }
        
        // Release ownership without unregistering
        ObserverId release() {
            manager_ = nullptr;
            return std::exchange(id_, 0);
        }
    };
    
    // RAII helper for batch observers
    class ScopedBatchObserver {
    private:
        FeatureManager* manager_;
        ObserverId id_;
        
    public:
        ScopedBatchObserver(FeatureManager& manager, BatchObserver callback, int priority = 0)
            : manager_(&manager)
            , id_(manager.add_batch_observer(std::move(callback), priority)) {}
        
        ~ScopedBatchObserver() {
            if (manager_ && id_ != 0) {
                manager_->remove_observer(id_);
            }
        }
        
        ScopedBatchObserver(const ScopedBatchObserver&) = delete;
        ScopedBatchObserver& operator=(const ScopedBatchObserver&) = delete;
        
        ScopedBatchObserver(ScopedBatchObserver&& other) noexcept
            : manager_(std::exchange(other.manager_, nullptr))
            , id_(std::exchange(other.id_, 0)) {}
        
        ScopedBatchObserver& operator=(ScopedBatchObserver&& other) noexcept {
            if (this != &other) {
                if (manager_ && id_ != 0) {
                    manager_->remove_observer(id_);
                }
                manager_ = std::exchange(other.manager_, nullptr);
                id_ = std::exchange(other.id_, 0);
            }
            return *this;
        }
        
        ObserverId id() const { return id_; }
        ObserverId release() {
            manager_ = nullptr;
            return std::exchange(id_, 0);
        }
    };

    // Add a feature with an optional validation check
    [[nodiscard]] Expected<void, std::string> add_feature(const std::string& name,
                                            FeatureCheck check = nullptr) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        if (features_.count(name)) {
            return unexpected("Feature already exists: " + name);
        }
        FeatureNode node;
        node.enabled = false;
        node.check = check;
        node.check_key = "";  // No key when added directly with callback
        features_[name] = std::move(node);
        return {};
    }

    // Add a feature using a registered callback key from the factory
    // This allows the feature to be fully serialized and deserialized
    [[nodiscard]] Expected<void, std::string> add_feature(const std::string& name,
                                            const std::string& check_key) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        if (features_.count(name)) {
            return unexpected("Feature already exists: " + name);
        }
        
        // Look up the check from factory
        auto check_result = get_feature_check_factory().make(check_key);
        if (!check_result) {
            return unexpected("Check key '" + check_key + "' not found in factory");
        }
        
        FeatureNode node;
        node.enabled = false;
        node.check = *check_result;
        node.check_key = check_key;
        features_[name] = std::move(node);
        return {};
    }

    // Add a relationship between two features
    [[nodiscard]] Expected<void, std::string> add_relationship(const std::string& from,
                                                  FeatureRelationship type,
                                                  const std::string& to) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        auto from_res = get_node(from);
        if (!from_res) {
            return unexpected("Feature not found: " + from + " in relationship setup");
        }
        auto to_res = get_node(to);
        if (!to_res) {
            return unexpected("Feature not found: " + to + " in relationship setup");
        }
        
        // Prevent self-referencing relationships
        if (from == to) {
            return unexpected("Cannot add self-referencing relationship: " + from);
        }
        
        FeatureNode* from_node = *from_res;
        auto insert_res = from_node->relationships[type].insert(to);
        if (!insert_res) {
            return unexpected("Failed to insert relationship: " + insert_res.error());
        }
        
        // Bidirectional for conflicts and mutually exclusive
        if (type == FeatureRelationship::Conflicts || 
            type == FeatureRelationship::MutuallyExclusive) {
            FeatureNode* to_node = *to_res;
            auto rev_insert_res = to_node->relationships[type].insert(from);
            if (!rev_insert_res) {
                return unexpected("Failed to insert reverse relationship: " + 
                                 rev_insert_res.error());
            }
        }
        return {};
    }

    // Add a feature group with optional custom state computer
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string> add_group(
        const std::string& group_name,
        const std::vector<std::string>& feature_names,
        StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        if (groups_.count(group_name)) {
            return unexpected("Group already exists: " + group_name);
        }
        for (const auto& feature_name : feature_names) {
            if (!features_.count(feature_name)) {
                return unexpected("Feature not found: " + feature_name);
            }
        }
        groups_[group_name] = std::make_unique<FeatureGroupInfo<StateEnum>>(
            feature_names, computer);
        return {};
    }

    // Add a mutually exclusive group
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<void, std::string> add_mutually_exclusive_group(
        const std::string& group_name,
        const std::vector<std::string>& feature_names,
        StateComputer<StateEnum> computer = FeatureGroupStatePolicy<StateEnum>::compute) {
        auto add_result = add_group(group_name, feature_names, computer);
        if (!add_result) {
            return add_result;
        }
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        for (size_t i = 0; i < feature_names.size(); ++i) {
            for (size_t j = i + 1; j < feature_names.size(); ++j) {
                auto res = add_relationship(feature_names[i],
                                           FeatureRelationship::MutuallyExclusive,
                                           feature_names[j]);
                if (!res) {
                    return res;
                }
            }
        }
        return {};
    }

    // Get group state with type safety
    template <typename StateEnum = FeatureGroupState>
    [[nodiscard]] Expected<StateEnum, std::string> get_group_state(const std::string& group_name) const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        return compute_group_state_impl<StateEnum>(group_name);
    }

    // Get features in a group
    [[nodiscard]] Expected<std::set<std::string>, std::string> get_group_features(
        const std::string& group_name) const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        auto git = groups_.find(group_name);
        if (git == groups_.end()) {
            return unexpected("Group not found: " + group_name);
        }
        return git->second->get_features();
    }

    // Enable a feature with full transactional semantics
    // If enabling fails (e.g., due to conflicts), no dependencies are left enabled
    [[nodiscard]] Expected<void, std::string> enable(const std::string& name) {
        return batch_enable({name});
    }

    // Disable a feature (Transactional)
    //
    // Delegates to batch_disable to ensure safety constraints (Requires/Implies)
    // are checked and all side effects are handled atomically.
    [[nodiscard]] Expected<void, std::string> disable(const std::string& name) {
        return batch_disable({name});
    }

    // Batch enable multiple features with transactional semantics
    // All features (including dependencies) succeed or all changes are rolled back.
    // NOTIFICATIONS ARE DEFERRED until after lock release to prevent deadlocks.
    [[nodiscard]] Expected<void, std::string> batch_enable(const std::vector<std::string>& names) {
        std::vector<std::string> all_changed;
        
        { // Scope for LockGuard - Lock held only during state modification
            typename SyncPolicy::LockGuard guard(sync_.getLock());
            
            // Validate all features exist first
            for (const auto& name : names) {
                auto node_res = get_node(name);
                if (!node_res) {
                    return unexpected(node_res.error());
                }
            }
            
            // Snapshot ALL feature states before any modifications
            std::map<std::string, bool> original_states;
            for (const auto& [name, node] : features_) {
                original_states[name] = node.enabled;
            }
            
            // Attempt to enable each feature
            for (const auto& name : names) {
                std::vector<std::string> chain;
                auto res = enable_feature(name, chain, &all_changed);
                if (!res) {
                    // Rollback ALL features to original states
                    for (auto& [feature_name, node] : features_) {
                        node.enabled = original_states[feature_name];
                    }
                    return res;
                }
            }
        } // Lock released here
        
        // Notify observers safely outside the lock (prevents deadlock if observer
        // tries to modify features)
        for (const auto& feature : all_changed) {
            notify_observers(feature, true, true);
        }
        
        // Notify batch observers
        if (!batch_observers_.empty() && !all_changed.empty()) {
            notify_batch_observers(names.empty() ? "" : names[0], all_changed, true, true);
        }
        
        return {};
    }

    // Batch disable multiple features with transactional semantics
    // All disables succeed or all changes are rolled back.
    // Validates that no enabled feature requires any disabled feature.
    // Also validates Implies relationships: cannot disable a feature that is implied by
    // an enabled feature.
    // NOTIFICATIONS ARE DEFERRED until after lock release to prevent deadlocks.
    [[nodiscard]] Expected<void, std::string> batch_disable(const std::vector<std::string>& names) {
        std::vector<std::string> actually_changed;
        
        { // Scope for LockGuard - Lock held only during state modification
            typename SyncPolicy::LockGuard guard(sync_.getLock());
            
            // Validate all features exist first
            for (const auto& name : names) {
                auto node_res = get_node(name);
                if (!node_res) {
                    return unexpected(node_res.error());
                }
            }
            
            // Build set for O(1) lookups
            std::set<std::string> disabled_set(names.begin(), names.end());
            
            // Record original states for rollback and track which actually changed
            std::vector<bool> original_states;
            original_states.reserve(names.size());
            
            for (const auto& name : names) {
                auto node_res = get_node(name);
                original_states.push_back((*node_res)->enabled);
                if ((*node_res)->enabled) {
                    actually_changed.push_back(name);
                }
                (*node_res)->enabled = false;
            }
            
            // Validate the resulting state
            for (const auto& [feature_name, node] : features_) {
                if (!node.enabled) {
                    continue;
                }
                
                // Check if this enabled feature requires any of the disabled features
                if (node.relationships.count(FeatureRelationship::Requires)) {
                    for (const auto& required : node.relationships.at(
                             FeatureRelationship::Requires)) {
                        auto req_node = get_node(required);
                        if (req_node && !(*req_node)->enabled) {
                            // Rollback
                            for (size_t i = 0; i < names.size(); ++i) {
                                auto n = get_node(names[i]);
                                if (n) {
                                    (*n)->enabled = original_states[i];
                                }
                            }
                            return unexpected("Cannot disable '" + required + 
                                             "': required by enabled feature '" + 
                                             feature_name + "'");
                        }
                    }
                }
                
                // Check if this enabled feature implies any of the disabled features
                // If A implies B and A is enabled, then B cannot be disabled
                if (node.relationships.count(FeatureRelationship::Implies)) {
                    for (const auto& implied : node.relationships.at(
                             FeatureRelationship::Implies)) {
                        if (disabled_set.count(implied)) {
                            // Rollback
                            for (size_t i = 0; i < names.size(); ++i) {
                                auto n = get_node(names[i]);
                                if (n) {
                                    (*n)->enabled = original_states[i];
                                }
                            }
                            return unexpected("Cannot disable '" + implied + 
                                             "': implied by enabled feature '" + 
                                             feature_name + "'. Disable '" + 
                                             feature_name + "' first.");
                        }
                    }
                }
            }
        } // Lock released here
        
        // Notify observers safely outside the lock (prevents deadlock if observer
        // tries to modify features)
        for (const auto& feature : actually_changed) {
            notify_observers(feature, false, true);
        }
        
        // Notify batch observers
        if (!batch_observers_.empty() && !actually_changed.empty()) {
            notify_batch_observers(names.empty() ? "" : names[0], 
                                    actually_changed, false, true);
        }
        
        return {};
    }

    // Check if feature is enabled
    bool is_enabled(const std::string& name) const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        auto node_res = get_node(name);
        if (!node_res) {
            return false;
        }
        return (*node_res)->enabled;
    }

    // Validate entire feature set
    [[nodiscard]] Expected<void, std::string> validate() {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        for (const auto& [name, _] : features_) {
            std::vector<std::string> visited;
            auto res = validate_feature(name, visited);
            if (!res) {
                return res;
            }
        }
        return {};
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
    // CRITICAL REENTRANCY WARNING:
    // Observers are called while holding the FeatureManager's internal lock.
    // DO NOT call any FeatureManager methods from within an observer callback,
    // as this will cause a deadlock:
    //
    //   WRONG - Will deadlock:
    //     manager.add_observer([&](auto name, auto enabled, auto success) {
    //         if (enabled) manager.enable("AnotherFeature");  // DEADLOCK!
    //     });
    //
    //   CORRECT - Defer action or use flag:
    //     bool needs_update = false;
    //     manager.add_observer([&](auto name, auto enabled, auto success) {
    //         if (enabled) needs_update = true;  // Set flag
    //     });
    //     // Later, outside the observer:
    //     if (needs_update) manager.enable("AnotherFeature");
    //
    // If you need to trigger other feature changes from an observer, consider:
    //   1. Set flags in the observer, check them after the operation completes
    //   2. Queue operations to process after the current lock is released
    //   3. Use a separate thread/event queue for async processing
    //   4. Redesign dependencies so cascading changes are handled by Implies/Requires
    ObserverId add_observer(FeatureObserver callback, int priority = 0) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        ObserverId id = next_observer_id_++;
        observers_.push_back({id, priority, std::move(callback)});
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
    ObserverId add_batch_observer(BatchObserver callback, int priority = 0) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
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
    bool remove_observer(ObserverId id) {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        
        // Check regular observers
        auto it = std::find_if(observers_.begin(), observers_.end(),
            [id](const ObserverEntry& entry) { return entry.id == id; });
        if (it != observers_.end()) {
            observers_.erase(it);
            return true;
        }
        
        // Check batch observers
        auto bit = std::find_if(batch_observers_.begin(), batch_observers_.end(),
            [id](const BatchObserverEntry& entry) { return entry.id == id; });
        if (bit != batch_observers_.end()) {
            batch_observers_.erase(bit);
            return true;
        }
        
        return false;
    }
    
    // Remove all observers
    void clear_observers() {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        observers_.clear();
        batch_observers_.clear();
    }

    // Get all enabled features
    std::vector<std::string> get_enabled() const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        std::vector<std::string> enabled;
        for (const auto& [name, node] : features_) {
            if (node.enabled) {
                enabled.push_back(name);
            }
        }
        return enabled;
    }

    // Get all feature names
    std::vector<std::string> get_all_features() const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        std::vector<std::string> all_features;
        for (const auto& [name, _] : features_) {
            all_features.push_back(name);
        }
        return all_features;
    }

    // Get all group names
    std::vector<std::string> get_all_groups() const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        std::vector<std::string> all_groups;
        for (const auto& [name, _] : groups_) {
            all_groups.push_back(name);
        }
        return all_groups;
    }

    // Serialize to JSON
    std::string to_json() const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        JsonObject root;
        JsonObject features_json;
        for (const auto& [name, node] : features_) {
            features_json[name] = node.to_json();
        }
        root["features"] = JsonValue{std::move(features_json)};
        JsonObject groups_json;
        for (const auto& [name, group] : groups_) {
            groups_json[name] = group->to_json();
        }
        root["groups"] = JsonValue{std::move(groups_json)};
        return to_json_string(root);
    }

    // Deserialize from JSON
    [[nodiscard]] static Expected<FeatureManager, std::string> from_json(const std::string& json_str) {
        JsonValue root;
        try {
            root = parse_json(json_str);
        } catch (const std::exception& e) {
            return unexpected(std::string("JSON parse error: ") + e.what());
        }
        if (!root.is_object()) {
            return unexpected("Root JSON must be an object");
        }
        const auto& obj = std::get<JsonObject>(root);
        FeatureManager manager;
        auto features_it = obj.find("features");
        if (features_it != obj.end()) {
            if (!features_it->second.is_object()) {
                return unexpected("'features' must be an object");
            }
            const auto& features_obj = std::get<JsonObject>(features_it->second);
            for (const auto& [name, value] : features_obj) {
                auto node_res = FeatureNode::from_json(value);
                if (!node_res) {
                    return unexpected("Error parsing feature '" + name + "': " + 
                                     node_res.error());
                }
                manager.features_[name] = std::move(*node_res);
            }
        }
        auto groups_it = obj.find("groups");
        if (groups_it != obj.end()) {
            if (!groups_it->second.is_object()) {
                return unexpected("'groups' must be an object");
            }
            const auto& groups_obj = std::get<JsonObject>(groups_it->second);
            for (const auto& [name, value] : groups_obj) {
                if (!value.is_array()) {
                    return unexpected("Group '" + name + "' must be an array");
                }
                const auto& arr = std::get<JsonArray>(value);
                std::vector<std::string> feature_names;
                for (const auto& elem : arr) {
                    if (!elem.is_string()) {
                        return unexpected("Group feature must be string");
                    }
                    feature_names.push_back(std::get<std::string>(elem));
                }
                manager.groups_[name] = 
                    std::make_unique<FeatureGroupInfo<FeatureGroupState>>(feature_names);
            }
        }
        return manager;
    }

    // Export to GraphViz DOT format
    std::string to_dot() const {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        std::ostringstream ss;
        ss << "digraph FeatureGraph {\n";
        ss << "    rankdir=LR;\n";
        ss << "    node [shape=box];\n";
        for (const auto& [name, node] : features_) {
            std::string color = node.enabled ? "green" : "gray";
            ss << "    \"" << name << "\" [style=filled, fillcolor=" << color << "];\n";
        }
        for (const auto& [name, node] : features_) {
            for (const auto& [type, targets] : node.relationships) {
                std::string style;
                std::string arrow;
                switch (type) {
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
                for (const auto& target : targets) {
                    std::string_view type_str = 
                        EnumStringPolicy<FeatureRelationship>::to_string(type);
                    ss << "    \"" << name << "\" -> \"" << target 
                       << "\" [style=" << style << ", arrowhead=" << arrow
                       << ", label=\"" << type_str << "\"];\n";
                }
            }
        }
        ss << "}\n";
        return ss.str();
    }

    // Parse from DOT format (basic support)
    [[nodiscard]] static Expected<FeatureManager, std::string> from_dot(const std::string& dot_str) {
        FeatureManager manager;
        std::regex node_regex(R"(\"([^\"]+)\"\s*\[|([a-zA-Z_][a-zA-Z0-9_]*)\s*\[)");
        std::sregex_iterator nodes_begin(dot_str.begin(), dot_str.end(), node_regex);
        std::sregex_iterator nodes_end;
        for (auto it = nodes_begin; it != nodes_end; ++it) {
            std::string node_name = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
            auto res = manager.add_feature(node_name);
            if (!res && !manager.features_.count(node_name)) {
                return unexpected(res.error());
            }
        }
        std::regex edge_regex(
            R"(\"([^\"]+)\"\s*(?:->|--)\s*\"([^\"]+)\"\s*\[[^\]]*label\s*=\s*\"([^\"]+)\")");
        std::sregex_iterator edges_begin(dot_str.begin(), dot_str.end(), edge_regex);
        std::sregex_iterator edges_end;
        for (auto it = edges_begin; it != edges_end; ++it) {
            std::string from = (*it)[1].str();
            std::string to = (*it)[2].str();
            std::string label = (*it)[3].str();
            FeatureRelationship type;
            try {
                type = EnumStringPolicy<FeatureRelationship>::from_string(label);
            } catch (const std::invalid_argument&) {
                continue;
            }
            auto res = manager.add_relationship(from, type, to);
            if (!res) {
                return unexpected(res.error());
            }
        }
        return manager;
    }

    // Clear all features, groups, and observers
    void clear() {
        typename SyncPolicy::LockGuard guard(sync_.getLock());
        features_.clear();
        groups_.clear();
        observers_.clear();
        batch_observers_.clear();
    }
};

}  // namespace fat_p
