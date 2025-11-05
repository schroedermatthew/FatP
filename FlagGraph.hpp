// FlagGraph.h (Enhanced with User-Provided Group State Enums)
#ifndef CPP_UTILITIES_FLAG_GRAPH_H
#define CPP_UTILITIES_FLAG_GRAPH_H

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Expected.h"  // For validation results
#include "ConcurrencyPolicies.h"  // For thread-safety
#include "JsonLite.h"  // For serialization (assuming it exists)

namespace cpp_utilities {

// Enum for interaction types
enum class FlagInteraction {
    Requires,     // This flag requires another to be enabled
    Conflicts,    // This flag conflicts with another (mutual)
    Implies,      // Enabling this implies enabling another
    MutuallyExclusive  // For groups: all flags in set conflict with each other
};

// Default built-in group state enum
enum class GroupState {
    Inactive,    // No flags enabled
    Partial,     // Some but not all flags enabled/valid
    Active,      // All required flags enabled, no conflicts, all checks pass
    Invalid      // Conflicts or failed checks
};

// Policy for computing group state (user can specialize or provide lambda)
template <typename StateEnum = GroupState>
struct GroupStatePolicy {
    using state_type = StateEnum;

    static state_type compute(const std::set<std::string>& group_flags, size_t enabled_count, bool has_conflict, bool all_checks_pass) {
        if (group_flags.empty()) return static_cast<state_type>(GroupState::Invalid);
        if (has_conflict || !all_checks_pass) return static_cast<state_type>(GroupState::Invalid);
        if (enabled_count == 0) return static_cast<state_type>(GroupState::Inactive);
        if (enabled_count < group_flags.size()) return static_cast<state_type>(GroupState::Partial);
        return static_cast<state_type>(GroupState::Active);
    }
};

// Function type for custom state computation
template <typename StateEnum>
using StateComputer = std::function<StateEnum(const std::set<std::string>& group_flags, size_t enabled_count, bool has_conflict, bool all_checks_pass)>;

// Node in the graph: a flag with its state, check func, and interactions
struct FlagNode {
    bool enabled = false;
    FlagCheck check;
    std::map<FlagInteraction, std::set<std::string>> interactions;  // Keyed by type, set of flag names

    // Serialization helpers
    JsonLite to_json() const {
        JsonLite json = JsonLite::object();
        json["enabled"] = enabled;
        for (const auto& [type, targets] : interactions) {
            std::string type_str;
            switch (type) {
                case FlagInteraction::Requires: type_str = "requires"; break;
                case FlagInteraction::Conflicts: type_str = "conflicts"; break;
                case FlagInteraction::Implies: type_str = "implies"; break;
                case FlagInteraction::MutuallyExclusive: type_str = "mutually_exclusive"; break;
            }
            JsonLite arr = JsonLite::array();
            for (const auto& t : targets) arr.push_back(t);
            json[type_str] = arr;
        }
        return json;
    }

    static Expected<FlagNode, std::string> from_json(const JsonLite& json) {
        FlagNode node;
        node.enabled = json["enabled"].as_bool(false);
        if (json.has("requires")) {
            for (const auto& t : json["requires"].as_array()) {
                node.interactions[FlagInteraction::Requires].insert(t.as_string());
            }
        }
        if (json.has("conflicts")) {
            for (const auto& t : json["conflicts"].as_array()) {
                node.interactions[FlagInteraction::Conflicts].insert(t.as_string());
            }
        }
        if (json.has("implies")) {
            for (const auto& t : json["implies"].as_array()) {
                node.interactions[FlagInteraction::Implies].insert(t.as_string());
            }
        }
        if (json.has("mutually_exclusive")) {
            for (const auto& t : json["mutually_exclusive"].as_array()) {
                node.interactions[FlagInteraction::MutuallyExclusive].insert(t.as_string());
            }
        }
        return node;
    }
};

// Group info struct to hold flags and custom state computer
template <typename StateEnum = GroupState>
struct GroupInfo {
    std::set<std::string> flags;
    StateComputer<StateEnum> state_computer;

    GroupInfo(const std::vector<std::string>& f, StateComputer<StateEnum> comp = GroupStatePolicy<StateEnum>::compute)
        : flags(f.begin(), f.end()), state_computer(comp) {}
};

template <typename SyncPolicy = SingleThreadedPolicy>
class FlagGraph {
private:
    std::map<std::string, FlagNode> flags_;
    std::map<std::string, void*> groups_;  // Erased type; use templated methods to access
    SyncPolicy sync_;

    // Helper to get node or error
    Expected<FlagNode&, std::string> get_node(const std::string& name) {
        auto it = flags_.find(name);
        if (it == flags_.end()) return unexpected("Flag not found: " + name);
        return it->second;
    }

    // Recursive validation with visited set to detect cycles
    Expected<void, std::string> validate_flag(const std::string& name, std::set<std::string>& visited) {
        auto node_res = get_node(name);
        if (!node_res) return unexpected(node_res.error());

        FlagNode& node = *node_res;
        if (!node.enabled) return {};  // Disabled flags don't need validation

        if (visited.count(name)) return unexpected("Cycle detected involving: " + name);
        visited.insert(name);

        // Run custom check
        if (node.check) {
            auto check_res = node.check();
            if (!check_res) return unexpected(check_res.error());
        }

        // Check interactions
        for (const auto& [type, targets] : node.interactions) {
            for (const auto& target : targets) {
                auto target_node_res = get_node(target);
                if (!target_node_res) return unexpected(target_node_res.error());
                bool target_enabled = target_node_res->enabled;

                switch (type) {
                    case FlagInteraction::Requires:
                        if (!target_enabled) return unexpected(name + " requires " + target + " to be enabled");
                        break;
                    case FlagInteraction::Conflicts:
                        if (target_enabled) return unexpected(name + " conflicts with " + target);
                        break;
                    case FlagInteraction::Implies:
                        if (!target_enabled) {
                            auto prop_res = enable_flag(target, visited);
                            if (!prop_res) return unexpected(prop_res.error());
                        }
                        break;
                    case FlagInteraction::MutuallyExclusive:
                        if (target_enabled) return unexpected(name + " is mutually exclusive with " + target);
                        break;
                }

                // Recurse
                auto rec_res = validate_flag(target, visited);
                if (!rec_res) return unexpected(rec_res.error());
            }
        }

        visited.erase(name);
        return {};
    }

    Expected<void, std::string> enable_flag(const std::string& name, std::set<std::string>& visited) {
        auto node_res = get_node(name);
        if (!node_res) return unexpected(node_res.error());
        if (node_res->enabled) return {};  // Already enabled
        node_res->enabled = true;
        return validate_flag(name, visited);
    }

    // Compute state for a group using its custom computer
    template <typename StateEnum>
    StateEnum compute_group_state(const std::string& group_name) const {
        auto group_it = groups_.find(group_name);
        if (group_it == groups_.end()) throw std::runtime_error("Group not found");  // Or return Expected

        auto& info = *static_cast<GroupInfo<StateEnum>*>(group_it->second);
        const auto& group_flags = info.flags;
        if (group_flags.empty()) return info.state_computer(group_flags, 0, true, false);  // Custom handling

        size_t enabled_count = 0;
        bool has_conflict = false;
        bool all_checks_pass = true;

        std::set<std::string> visited;
        for (const auto& flag : group_flags) {
            auto node_res = const_cast<FlagGraph*>(this)->get_node(flag);
            if (!node_res) return info.state_computer(group_flags, 0, true, false);  // Invalid
            if (node_res->enabled) {
                ++enabled_count;
                if (node_res->check) {
                    auto check_res = node_res->check();
                    if (!check_res) all_checks_pass = false;
                }
                visited.clear();
                auto valid_res = const_cast<FlagGraph*>(this)->validate_flag(flag, visited);
                if (!valid_res) has_conflict = true;
            }
        }

        return info.state_computer(group_flags, enabled_count, has_conflict, all_checks_pass);
    }

public:
    // Add a flag with optional check function
    Expected<void, std::string> add_flag(const std::string& name, FlagCheck check = nullptr) {
        std::lock_guard<decltype(sync_)> lock(sync_);
        if (flags_.count(name)) return unexpected("Flag already exists: " + name);
        flags_[name].check = check;
        return {};
    }

    // Add interaction between flags
    Expected<void, std::string> add_interaction(const std::string& from, FlagInteraction type, const std::string& to) {
        std::lock_guard<decltype(sync_)> lock(sync_);
        auto from_res = get_node(from);
        if (!from_res) return unexpected(from_res.error());
        auto to_res = get_node(to);
        if (!to_res) return unexpected(to_res.error());

        from_res->interactions[type].insert(to);
        // Make bidirectional for Conflicts and MutuallyExclusive
        if (type == FlagInteraction::Conflicts || type == FlagInteraction::MutuallyExclusive) {
            to_res->interactions[type].insert(from);
        }
        return {};
    }

    // Add a general group with user-provided state computer (enum inferred from computer)
    template <typename StateEnum = GroupState>
    Expected<void, std::string> add_group(const std::string& group_name, const std::vector<std::string>& group_flags, 
                                          StateComputer<StateEnum> state_computer = GroupStatePolicy<StateEnum>::compute) {
        std::lock_guard<decltype(sync_)> lock(sync_);
        if (groups_.count(group_name)) return unexpected("Group already exists: " + group_name);
        groups_[group_name] = new GroupInfo<StateEnum>(group_flags, state_computer);
        return {};
    }

    // Add mutually exclusive group with custom state computer
    template <typename StateEnum = GroupState>
    Expected<void, std::string> add_mutually_exclusive_group(const std::string& group_name, const std::vector<std::string>& group_flags,
                                                             StateComputer<StateEnum> state_computer = GroupStatePolicy<StateEnum>::compute) {
        auto res = add_group(group_name, group_flags, state_computer);
        if (!res) return unexpected(res.error());
        for (size_t i = 0; i < group_flags.size(); ++i) {
            for (size_t j = i + 1; j < group_flags.size(); ++j) {
                auto int_res = add_interaction(group_flags[i], FlagInteraction::MutuallyExclusive, group_flags[j]);
                if (!int_res) return unexpected(int_res.error());
            }
        }
        return {};
    }

    // Get state of a group (user specifies enum type)
    template <typename StateEnum = GroupState>
    Expected<StateEnum, std::string> get_group_state(const std::string& group_name) const {
        std::lock_guard<decltype(sync_)> lock(sync_);
        auto group_it = groups_.find(group_name);
        if (group_it == groups_.end()) return unexpected("Group not found: " + group_name);
        try {
            return compute_group_state<StateEnum>(group_name);
        } catch (...) {
            return unexpected("Type mismatch for group state enum");
        }
    }

    // Enable a flag and validate/propagate
    Expected<void, std::string> enable(const std::string& name) {
        std::lock_guard<decltype(sync_)> lock(sync_);
        std::set<std::string> visited;
        return enable_flag(name, visited);
    }

    // Disable a flag and check if it invalidates others
    Expected<void, std::string> disable(const std::string& name) {
        std::lock_guard<decltype(sync_)> lock(sync_);
        auto node_res = get_node(name);
        if (!node_res) return unexpected(node_res.error());
        node_res->enabled = false;
        return validate();
    }

    bool is_enabled(const std::string& name) const {
        std::lock_guard<decltype(sync_)> lock(sync_);
        auto it = flags_.find(name);
        return it != flags_.end() && it->second.enabled;
    }

    // Validate entire graph
    Expected<void, std::string> validate() {
        std::lock_guard<decltype(sync_)> lock(sync_);
        std::set<std::string> visited;
        for (const auto& [name, node] : flags_) {
            if (node.enabled) {
                visited.clear();
                auto res = validate_flag(name, visited);
                if (!res) return unexpected(res.error());
            }
        }
        return {};
    }

    // Get all enabled flags
    std::vector<std::string> get_enabled() const {
        std::lock_guard<decltype(sync_)> lock(sync_);
        std::vector<std::string> enabled;
        for (const auto& [name, node] : flags_) {
            if (node.enabled) enabled.push_back(name);
        }
        return enabled;
    }

    // Serialize to JSON (includes groups, but not state computers as they're functions)
    std::string to_json() const {
        std::lock_guard<decltype(sync_)> lock(sync_);
        JsonLite json = JsonLite::object();
        JsonLite flags_json = JsonLite::object();
        for (const auto& [name, node] : flags_) {
            flags_json[name] = node.to_json();
        }
        json["flags"] = flags_json;

        JsonLite groups_json = JsonLite::object();
        for (const auto& [gname, gptr] : groups_) {
            // Can't serialize computer; user can extend if needed
            JsonLite arr = JsonLite::array();
            // Cast to default to serialize flags (lose custom type info)
            auto& info = *static_cast<GroupInfo<GroupState>*>(gptr);
            for (const auto& f : info.flags) arr.push_back(f);
            groups_json[gname] = arr;
        }
        json["groups"] = groups_json;

        return json.to_string();
    }

    // Deserialize from JSON (defaults to built-in state policy)
    static Expected<FlagGraph, std::string> from_json(const std::string& json_str) {
        auto parsed = JsonLite::parse(json_str);
        if (!parsed.is_object()) return unexpected("Invalid JSON for FlagGraph");
        
        FlagGraph graph;
        if (parsed.has("flags")) {
            for (const auto& [name, value] : parsed["flags"].as_object()) {
                auto node_res = FlagNode::from_json(value);
                if (!node_res) return unexpected(node_res.error());
                graph.flags_[name] = *node_res;
            }
        }
        if (parsed.has("groups")) {
            for (const auto& [gname, value] : parsed["groups"].as_object()) {
                std::vector<std::string> gflags;
                for (const auto& f : value.as_array()) {
                    gflags.push_back(f.as_string());
                }
                graph.add_group(gname, gflags);  // Default
            }
        }
        return graph;
    }

    // Generate DOT graph for visualization (includes groups as clusters)
    std::string to_dot() const {
        std::lock_guard<decltype(sync_)> lock(sync_);
        std::string dot = "digraph FlagGraph {\n";
        dot += "  node [shape=box];\n";
        
        // Groups as subgraphs
        for (const auto& [gname, gptr] : groups_) {
            dot += "  subgraph cluster_" + gname + " {\n";
            dot += "    label = \"" + gname + "\";\n";
            // Cast to default for flags (since type erased)
            auto& info = *static_cast<GroupInfo<GroupState>*>(gptr);
            for (const auto& f : info.flags) {
                auto node_it = flags_.find(f);
                if (node_it != flags_.end()) {
                    std::string color = node_it->second.enabled ? "green" : "red";
                    dot += "    \"" + f + "\" [color=" + color + "];\n";
                }
            }
            dot += "  }\n";
        }
        
        // Standalone nodes
        for (const auto& [name, node] : flags_) {
            bool in_group = false;
            for (const auto& [_, gptr] : groups_) {
                auto& info = *static_cast<GroupInfo<GroupState>*>(gptr);
                if (info.flags.count(name)) { in_group = true; break; }
            }
            if (!in_group) {
                std::string color = node.enabled ? "green" : "red";
                dot += "  \"" + name + "\" [color=" + color + "];\n";
            }
        }
        
        // Edges
        for (const auto& [name, node] : flags_) {
            for (const auto& [type, targets] : node.interactions) {
                std::string style, label;
                switch (type) {
                    case FlagInteraction::Requires: style = "solid"; label = "requires"; break;
                    case FlagInteraction::Conflicts: style = "dotted"; label = "conflicts"; break;
                    case FlagInteraction::Implies: style = "dashed"; label = "implies"; break;
                    case FlagInteraction::MutuallyExclusive: style = "bold"; label = "exclusive"; break;
                }
                for (const auto& target : targets) {
                    dot += "  \"" + name + "\" -> \"" + target + "\" [style=" + style + ", label=\"" + label + "\"];\n";
                }
            }
        }
        
        dot += "}\n";
        return dot;
    }

    // Destructor to clean up erased groups
    ~FlagGraph() {
        for (auto& [_, ptr] : groups_) {
            delete ptr;  // Raw ptr for type erasure; consider unique_ptr<void> with deleter in C++20+
        }
    }
};

}  // namespace cpp_utilities

#endif  // CPP_UTILITIES_FLAG_GRAPH_H