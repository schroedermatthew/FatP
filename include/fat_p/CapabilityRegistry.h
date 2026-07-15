#pragma once

/*
FATP_META:
  meta_version: 1
  component: Skeleton
  file_role: public_header
  path: include/fat_p/CapabilityRegistry.h
  namespace: fat_p::skeleton
  layer: Domain
  summary: >
    Name-to-index registry for capability bits. The framework band (0-31) is
    pre-registered from SkeletonCapability; applications register their own
    named capabilities at init and receive indices from 32 upward.
  api_stability: in_work
*/

/**
 * @file CapabilityRegistry.h
 * @brief Open capability vocabulary — registered, not hardcoded.
 *
 * @details
 * SkeletonMask is unbounded (SkeletonFwd.h); this registry is what makes the
 * capability *vocabulary* open. The framework's capabilities (the
 * SkeletonCapability enum) are pre-registered at their enum indices inside the
 * reserved framework band [0, kFrameworkCapabilityBand). An application
 * registers additional capabilities by name — schema-level bits such as
 * hardware categories, manufacturers, or value semantics the framework cannot
 * know — and receives allocated indices from kFrameworkCapabilityBand upward.
 * Allocation is sequential and collision-free by construction.
 *
 * Discipline (registration-at-init): register every capability during
 * application startup, before items are constructed, and never afterwards —
 * the same immutable-after-init contract as every other registry in the
 * architecture. Registration is idempotent by name, so independent modules
 * may safely register a shared capability name; both receive the same index.
 *
 * Threading: not synchronized. Registration must happen on the startup thread;
 * lookups afterwards are read-only and safe.
 *
 * @code
 * using fat_p::skeleton::CapabilityRegistry;
 * const std::size_t hydraulic =
 *     CapabilityRegistry::instance().registerCapability("App.Hydraulic");
 * auto mask = fat_p::skeleton::makeMask(
 *     fat_p::skeleton::SkeletonCapability::ProvidesValue, hydraulic);
 * @endcode
 */

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "SkeletonFwd.h"

namespace fat_p::skeleton
{

class CapabilityRegistry
{
public:
    [[nodiscard]] static CapabilityRegistry& instance()
    {
        static CapabilityRegistry sInstance;
        return sInstance;
    }

    CapabilityRegistry(const CapabilityRegistry&)            = delete;
    CapabilityRegistry& operator=(const CapabilityRegistry&) = delete;

    /**
     * @brief Registers @p name and returns its capability index.
     *
     * Idempotent: a name already registered (including the pre-registered
     * framework names) returns its existing index. New names are allocated
     * sequentially starting at kFrameworkCapabilityBand.
     */
    std::size_t registerCapability(std::string name)
    {
        if (const auto it = mByName.find(name); it != mByName.end())
        {
            return it->second;
        }
        const std::size_t index = mNames.size();
        mByName.emplace(name, index);
        mNames.push_back(std::move(name));
        return index;
    }

    /// @brief Index for @p name, or nullopt if never registered.
    [[nodiscard]] std::optional<std::size_t> find(std::string_view name) const
    {
        const auto it = mByName.find(std::string(name));
        return it != mByName.end() ? std::optional<std::size_t>(it->second)
                                   : std::nullopt;
    }

    /// @brief Name at @p index; empty view for unregistered/reserved slots.
    [[nodiscard]] std::string_view name(std::size_t index) const noexcept
    {
        return index < mNames.size() ? std::string_view(mNames[index])
                                     : std::string_view{};
    }

    /// @brief One past the highest allocated index (== next index to allocate).
    [[nodiscard]] std::size_t highWater() const noexcept { return mNames.size(); }

private:
    CapabilityRegistry()
    {
        // Reserve the whole framework band, then pre-register the framework
        // capability names at their enum indices. Unused band slots remain
        // reserved (unnamed, never allocated to applications).
        mNames.resize(kFrameworkCapabilityBand);

        const auto preRegister = [this](SkeletonCapability cap, std::string name)
        {
            const auto index = static_cast<std::size_t>(cap);
            mByName.emplace(name, index);
            mNames[index] = std::move(name);
        };

        preRegister(SkeletonCapability::Sensor,          "Sensor");
        preRegister(SkeletonCapability::Controller,      "Controller");
        preRegister(SkeletonCapability::Display,         "Display");
        preRegister(SkeletonCapability::Network,         "Network");
        preRegister(SkeletonCapability::Storage,         "Storage");
        preRegister(SkeletonCapability::ProvidesValue,   "ProvidesValue");
        preRegister(SkeletonCapability::ProvidesCommand, "ProvidesCommand");
        preRegister(SkeletonCapability::ProvidesStatus,  "ProvidesStatus");
        preRegister(SkeletonCapability::ValueBinary,     "ValueBinary");
        preRegister(SkeletonCapability::ValueContinuous, "ValueContinuous");
        preRegister(SkeletonCapability::ValueDiscrete,   "ValueDiscrete");
        preRegister(SkeletonCapability::ConsumesValue,   "ConsumesValue");
        preRegister(SkeletonCapability::ConsumesCommand, "ConsumesCommand");
        preRegister(SkeletonCapability::ConsumesStatus,  "ConsumesStatus");
        preRegister(SkeletonCapability::Readable,        "Readable");
        preRegister(SkeletonCapability::Writable,        "Writable");
        preRegister(SkeletonCapability::Serializable,    "Serializable");
        preRegister(SkeletonCapability::NetworkVisible,  "NetworkVisible");
    }

    std::unordered_map<std::string, std::size_t> mByName;
    std::vector<std::string>                     mNames; // index → name ("" = reserved)
};

} // namespace fat_p::skeleton
