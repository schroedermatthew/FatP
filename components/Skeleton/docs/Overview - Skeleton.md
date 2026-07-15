---
doc_id: OV-SKELETON-001
doc_type: "Overview"
title: "Skeleton"
fatp_components: ["Skeleton", "SkeletonFwd", "SkeletonUtilities"]
topics: ["typed hierarchical registry", "publish-subscribe", "service discovery", "capability mask", "compile-time address", "hierarchical namespacing", "HAL", "plugin architecture"]
constraints: ["pointer stability", "single-threaded", "lifetime contract enforcement", "zero heap indirection for lookup", "schema validation at compile time"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-03-01"
audience: ["C++ developers", "embedded systems engineers", "game engine developers", "AI assistants"]
status: "draft"
---

# Overview - Skeleton

*Fat-P Library — March 2026*

---

## Executive Summary

Skeleton is a typed hierarchical item registry that decouples component discovery from component ownership. Items publish themselves onto a shared Skeleton using compile-time validated addresses called Bones; other parts of the system discover them by address or by capability query. The key mechanism is a compile-time schema that binds each depth level of the hierarchy to a specific enum type, turning address errors into compile failures rather than runtime crashes. Three reactive signals — `onPublished`, `onUnpublishing`, and `onMaskChanged` — let observers track the registry's evolving contents without polling. The lifecycle contract is enforced by terminate-on-violation: destroying a published item or a non-empty Skeleton calls `std::terminate`, eliminating silent dangling-pointer corruption in large component graphs.

---

## Overview Card

| Aspect | Details |
|--------|---------|
| **Component** | Skeleton (Skeleton.h, SkeletonFwd.h, SkeletonUtilities.h) |
| **Problem solved** | Type-safe, address-validated publish/discover for hierarchically organized runtime components |
| **When to use** | HAL driver layers, plugin systems, sensor/actuator registries, game entity component graphs — any system where components need to find each other without tight coupling |
| **When NOT to use** | Thread-safe registries (no ThreadSafeSkeleton in v1), purely static component graphs (direct composition is simpler), systems with > 8 hierarchy levels |
| **Key guarantee** | Compile-time schema validation; terminate-on-lifecycle-violation; O(1) average find; query results in stable BoneId order |
| **std equivalent** | None. No standard equivalent exists or is planned. |
| **Boost equivalent** | None directly. Boost.Signals2 covers the signal-subscription aspect; there is no Boost hierarchical registry. |
| **Other alternatives** | Manual std::unordered_map registries, service-locator patterns, ECS frameworks (EnTT, flecs) |
| **Read next:** | User Manual - Skeleton |

---

## The Problem Domain

### The Tight-Coupling Tangle

Large systems — HAL layers, simulation engines, embedded firmware — accumulate components that need to find each other. A load sensor needs the display controller. The network module needs the status aggregator. The shutdown manager needs every component that holds external resources.

The naive solution is direct pointer injection: pass every required dependency into every constructor. This works at small scale and collapses quickly at large scale. A component that needs six subsystems requires six constructor parameters. Changing a dependency's type ripples through every constructor that touches it. Adding a new consumer requires modifying the producer's interface or adding another pass-through layer.

```cpp
// The pointer-injection tangle -- every dependency is manually wired
class ShutdownManager {
    LoadSensor*      mLoad;
    TempSensor*      mTemp;
    DisplayController* mDisplay;
    NetworkModule*   mNetwork;
    StorageController* mStorage;
    // ... and every new component requires a constructor change here

    ShutdownManager(LoadSensor* load, TempSensor* temp,
                    DisplayController* display, NetworkModule* net,
                    StorageController* storage)
        : mLoad(load), mTemp(temp), mDisplay(display),
          mNetwork(net), mStorage(storage) {}
};
```

| Issue | Impact in Large Systems |
|-------|------------------------|
| Constructor explosion | Every new consumer of a service requires changing its constructor and all call sites |
| No capability queries | "Find all components that provide sensor data" requires a hand-maintained list |
| No lifecycle notifications | Components that arrive or leave at runtime require manual wiring of callbacks |
| Runtime address errors | Typos in string-keyed registries are silent until the lookup returns nullptr |

### The Standard's Limitation

The C++ standard provides no hierarchical registry abstraction. `std::unordered_map<std::string, void*>` gives you a flat registry but no address structure, no capability filtering, no lifecycle signals, and no compile-time validation of the addresses you use. Errors in string-keyed lookups are invisible until runtime. There is no type-safe way to express that address `"sensors/load"` should always hold a `LoadSensor*`, never a `NetworkModule*`.

---

## Architecture

Skeleton's design rests on three mechanisms working together: compile-time address typing via Bones, runtime capability description via SkeletonMasks, and a passive non-owning registry with reactive signals.

![Skeleton hierarchy tree showing a sample HierarchySchema with three depth levels](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iMzE4IiB2aWV3Qm94PSIwIDAgNzAwIDMxOCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3MDAiIGhlaWdodD0iMzE4IiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjY2JkNWUxIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNTAiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCI+SGllcmFyY2h5IFRyZWUg4oCUIEhpZXJhcmNoeVNjaGVtYSZsdDtTeXN0ZW0sIFN1YnN5c3RlbSwgQ2hhbm5lbCZndDs8L3RleHQ+Cjx0ZXh0IHg9IjgiIHk9IjYzIiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMDwvdGV4dD4KPHRleHQgeD0iOCIgeT0iMTQ1IiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMTwvdGV4dD4KPHRleHQgeD0iOCIgeT0iMjM1IiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMjwvdGV4dD4KPGxpbmUgeDE9IjgwIiB5MT0iMzQiIHgyPSI4MCIgeTI9IjI1NCIgc3Ryb2tlPSIjOTRhM2I4IiBzdHJva2Utd2lkdGg9IjEiLz4KPHJlY3QgeD0iMzQwIiB5PSI0NCIgd2lkdGg9IjE0NCIgaGVpZ2h0PSIzMCIgcng9IjE0IiBmaWxsPSIjZGJlYWZlIiBzdHJva2U9IiMyNTYzZWIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI0MTIiIHk9IjY0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMWUzYThhIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+U3lzdGVtOjpSb290PC90ZXh0Pgo8bGluZSB4MT0iNDEyIiB5MT0iNzQiIHgyPSIyMDAiIHkyPSIxMjYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPGxpbmUgeDE9IjQxMiIgeTE9Ijc0IiB4Mj0iNDIwIiB5Mj0iMTI2IiBzdHJva2U9IiNjYmQ1ZTEiIHN0cm9rZS13aWR0aD0iMS41Ii8+CjxsaW5lIHgxPSI0MTIiIHkxPSI3NCIgeDI9IjYyNCIgeTI9IjEyNiIgc3Ryb2tlPSIjY2JkNWUxIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8cmVjdCB4PSIxMjYiIHk9IjEyNiIgd2lkdGg9IjE0OCIgaGVpZ2h0PSIzMCIgcng9IjE0IiBmaWxsPSIjY2ZmYWZlIiBzdHJva2U9IiMwODkxYjIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIyMDAiIHk9IjE0NiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE2NGU2MyIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPlN1Yjo6U2Vuc29yczwvdGV4dD4KPHJlY3QgeD0iMzQ2IiB5PSIxMjYiIHdpZHRoPSIxNDgiIGhlaWdodD0iMzAiIHJ4PSIxNCIgZmlsbD0iI2RjZmNlNyIgc3Ryb2tlPSIjMTZhMzRhIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNDIwIiB5PSIxNDYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5TdWI6OkFjdHVhdG9yczwvdGV4dD4KPHJlY3QgeD0iNTUwIiB5PSIxMjYiIHdpZHRoPSIxNDgiIGhlaWdodD0iMzAiIHJ4PSIxNCIgZmlsbD0iI2VjZmNjYiIgc3Ryb2tlPSIjNjVhMzBkIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNjI0IiB5PSIxNDYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzNjUzMTQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5TdWI6Ok5ldHdvcms8L3RleHQ+CjxyZWN0IHg9Ijg0IiB5PSIyMTYiIHdpZHRoPSI4MCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxMjQiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OkxvYWQ8L3RleHQ+CjxsaW5lIHgxPSIyMDAiIHkxPSIxNTYiIHgyPSIxMjQiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHJlY3QgeD0iMTcyIiB5PSIyMTYiIHdpZHRoPSI4MCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIyMTIiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OlRlbXA8L3RleHQ+CjxsaW5lIHgxPSIyMDAiIHkxPSIxNTYiIHgyPSIyMTIiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHJlY3QgeD0iMjYwIiB5PSIyMTYiIHdpZHRoPSI5OCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIzMDkiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OlByZXNzdXJlPC90ZXh0Pgo8bGluZSB4MT0iMjAwIiB5MT0iMTU2IiB4Mj0iMzA5IiB5Mj0iMjE2IiBzdHJva2U9IiNjYmQ1ZTEiIHN0cm9rZS13aWR0aD0iMS41Ii8+CjxyZWN0IHg9IjQwMCIgeT0iMjE2IiB3aWR0aD0iODQiIGhlaWdodD0iMzAiIHJ4PSIxMiIgZmlsbD0iI2VkZTlmZSIgc3Ryb2tlPSIjN2MzYWVkIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNDQyIiB5PSIyMzYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM0YzFkOTUiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5DaGFuOjpGYW48L3RleHQ+CjxsaW5lIHgxPSI0MjAiIHkxPSIxNTYiIHgyPSI0NDIiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHRleHQgeD0iMjAwIiB5PSIyNjAiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM3MTNmMTIiIGZvbnQtc2l6ZT0iMTAiPlN1Yjo6U2Vuc29ycy5pc0FuY2VzdG9yT2YoTG9hZCwgVGVtcCwgUHJlc3N1cmUpPC90ZXh0Pgo8dGV4dCB4PSI4NCIgeT0iMjc2IiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIiBmb250LWZhbWlseT0iJ0NvdXJpZXIgTmV3Jyxtb25vc3BhY2UiPkJvbmUmbHQ7UywgU3lzOjpSb290LCBTdWI6OlNlbnNvcnMsIENoYW46OkxvYWQmZ3Q7PC90ZXh0PgoKPC9zdmc+)

### Compile-Time Addresses: Bones and Schemas

A `HierarchySchema` binds each level of the hierarchy to a specific enum type. A `Bone` is a sequence of enum values validated at compile time against a schema. Two Bones with the same schema and enum values are the same type and produce equal `BoneId` values. Mismatched enum types are hard compiler errors.

```cpp
// Define the shape of the hierarchy once, for the whole application
enum class System    : uint8_t { Root = 1, Aux = 2 };
enum class Subsystem : uint8_t { Sensors = 1, Actuators = 2, Network = 3 };
enum class Channel   : uint8_t { Load = 0, Temp = 1, Pressure = 2 };

using SysSchema = fat_p::skeleton::HierarchySchema<System, Subsystem, Channel>;

// Addresses are types -- the compiler rejects wrong enum types at the wrong depth
using LoadBone = fat_p::skeleton::Bone<SysSchema, System::Root, Subsystem::Sensors, Channel::Load>;
// fat_p::skeleton::Bone<SysSchema, Subsystem::Sensors, System::Root, Channel::Load>
//   would not compile -- Subsystem at depth 0 violates the schema
```

![BoneId 64-bit memory layout showing eight level bytes and a depth field](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3NDgiIGhlaWdodD0iMTcwIiB2aWV3Qm94PSIwIDAgNzQ4IDE3MCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3NDgiIGhlaWdodD0iMTcwIiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNzQiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+Qm9uZUlkIOKAlCA2NC1iaXQgUGFja2VkIEhpZXJhcmNoaWNhbCBBZGRyZXNzPC90ZXh0Pgo8cmVjdCB4PSIxMCIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNkYmVhZmUiIHN0cm9rZT0iIzI1NjNlYiIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjUxIiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzFlM2E4YSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDA8L3RleHQ+Cjx0ZXh0IHg9IjUxIiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzYzOjU2XTwvdGV4dD4KPHRleHQgeD0iNTEiIHk9Ijg3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMWUzYThhIiBmb250LXNpemU9IjEwIj4weDAxPC90ZXh0Pgo8cmVjdCB4PSIxMDEiIHk9IjQwIiB3aWR0aD0iODIiIGhlaWdodD0iNTIiIHJ4PSI1IiBmaWxsPSIjY2ZmYWZlIiBzdHJva2U9IiMwODkxYjIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxNDIiIHk9IjU4IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMTY0ZTYzIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+TGV2ZWwgMTwvdGV4dD4KPHRleHQgeD0iMTQyIiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzU1OjQ4XTwvdGV4dD4KPHRleHQgeD0iMTQyIiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE2NGU2MyIgZm9udC1zaXplPSIxMCI+MHgwMTwvdGV4dD4KPHJlY3QgeD0iMTkyIiB5PSI0MCIgd2lkdGg9IjgyIiBoZWlnaHQ9IjUyIiByeD0iNSIgZmlsbD0iI2RjZmNlNyIgc3Ryb2tlPSIjMTZhMzRhIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iMjMzIiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE0NTMyZCIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDI8L3RleHQ+Cjx0ZXh0IHg9IjIzMyIgeT0iNzMiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPls0Nzo0MF08L3RleHQ+Cjx0ZXh0IHg9IjIzMyIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTAiPjB4MDE8L3RleHQ+CjxyZWN0IHg9IjI4MyIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNlY2ZjY2IiIHN0cm9rZT0iIzY1YTMwZCIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjMyNCIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzNjUzMTQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCAzPC90ZXh0Pgo8dGV4dCB4PSIzMjQiIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bMzk6MzJdPC90ZXh0Pgo8dGV4dCB4PSIzMjQiIHk9Ijg3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj4weDAwPC90ZXh0Pgo8cmVjdCB4PSIzNzQiIHk9IjQwIiB3aWR0aD0iODIiIGhlaWdodD0iNTIiIHJ4PSI1IiBmaWxsPSIjZmVmOWMzIiBzdHJva2U9IiNjYThhMDQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI0MTUiIHk9IjU4IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNzEzZjEyIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+TGV2ZWwgNDwvdGV4dD4KPHRleHQgeD0iNDE1IiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzMxOjI0XTwvdGV4dD4KPHRleHQgeD0iNDE1IiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+MHgwMDwvdGV4dD4KPHJlY3QgeD0iNDY1IiB5PSI0MCIgd2lkdGg9IjgyIiBoZWlnaHQ9IjUyIiByeD0iNSIgZmlsbD0iI2ZmZWRkNSIgc3Ryb2tlPSIjZWE1ODBjIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNTA2IiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzdjMmQxMiIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDU8L3RleHQ+Cjx0ZXh0IHg9IjUwNiIgeT0iNzMiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPlsyMzoxNl08L3RleHQ+Cjx0ZXh0IHg9IjUwNiIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPjB4MDA8L3RleHQ+CjxyZWN0IHg9IjU1NiIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNmZWUyZTIiIHN0cm9rZT0iI2RjMjYyNiIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjU5NyIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM3ZjFkMWQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCA2PC90ZXh0Pgo8dGV4dCB4PSI1OTciIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bMTU6OF08L3RleHQ+Cjx0ZXh0IHg9IjU5NyIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPjB4MDA8L3RleHQ+CjxyZWN0IHg9IjY0NyIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNlZGU5ZmUiIHN0cm9rZT0iIzdjM2FlZCIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjY4OCIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM0YzFkOTUiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCA3PC90ZXh0Pgo8dGV4dCB4PSI2ODgiIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bNzowXTwvdGV4dD4KPHRleHQgeD0iNjg4IiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+MHgwMDwvdGV4dD4KPHJlY3QgeD0iMTAiIHk9IjEwNiIgd2lkdGg9IjgyIiBoZWlnaHQ9IjI4IiByeD0iNSIgZmlsbD0iI2UwZTdmZiIgc3Ryb2tlPSIjNGY0NmU1IiBzdHJva2Utd2lkdGg9IjEuNSIgc3Ryb2tlLWRhc2hhcnJheT0iNCAyIi8+Cjx0ZXh0IHg9IjUxIiB5PSIxMjQiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzMTJlODEiIGZvbnQtc2l6ZT0iMTEiPmRlcHRoIGJ5dGU8L3RleHQ+CjxsaW5lIHgxPSI5NSIgeTE9IjEyMCIgeDI9IjE1NSIgeTI9IjEyMCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+Cjx0ZXh0IHg9IjE2MCIgeT0iMTI0IiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjExIj5zZXBhcmF0ZSBmaWVsZDsgbm90IHBhcnQgb2YgdGhlIDY0LWJpdCB2YWx1ZTwvdGV4dD4KPHRleHQgeD0iMzc0IiB5PSIxNTUiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiPkluYWN0aXZlIGxldmVscyBhcmUgYWx3YXlzIHplcm8gKGNhbm9uaWNhbCBmb3JtKS4gRXF1YWxpdHkgaXMgYSBzaW5nbGUgNjQtYml0IGludGVnZXIgY29tcGFyaXNvbi48L3RleHQ+CjxkZWZzPgogIDxtYXJrZXIgaWQ9ImRhIiBtYXJrZXJXaWR0aD0iOCIgbWFya2VySGVpZ2h0PSI4IiByZWZYPSI0IiByZWZZPSI0IiBvcmllbnQ9ImF1dG8iPgogICAgPHBhdGggZD0iTTAsMCBMMCw4IEw4LDQgeiIgZmlsbD0iIzY0NzQ4YiIvPgogIDwvbWFya2VyPgo8L2RlZnM+Cjwvc3ZnPg==)

At runtime, each Bone produces a canonical `BoneId`: a 64-bit packed value storing up to 8 enum levels, 8 bits per level. BoneId equality is `O(1)`, and `isAncestorOf` is also `O(1)`, enabling efficient subtree queries and traversal.

### Capability Description: SkeletonMask

Every item carries a `SkeletonMask` — an unbounded capability bitset built from `SkeletonCapability` enum values and/or application capability indices registered by name through `CapabilityRegistry` (framework band 0–31 pre-registered; application indices allocated from 32 upward, collision-free). Capabilities describe what an item *is* and what it *does*, independently of its address. This separation matters: two items at the same address level might have entirely different capabilities. Querying by capability finds all matching items across the whole registry without knowing their addresses — and the vocabulary is open: an application adds capabilities by registering names, never by editing a framework enum.

```cpp
using namespace fat_p::skeleton;

// Mask describes capabilities; address describes location
auto mask = makeMask(SkeletonCapability::Sensor,
                     SkeletonCapability::ProvidesValue,
                     SkeletonCapability::Readable);

// Later: find all readable value-providing sensors across the whole registry
auto sensors = skeleton.query(
    makeMask(SkeletonCapability::Sensor, SkeletonCapability::Readable));
```

### Passive Non-Owning Registry

`Skeleton` stores raw pointers to items. It does not own them. Items own themselves. This design choice has a critical consequence: if an item is destroyed while still published, the registry holds a dangling pointer. Skeleton resolves this by making the consequence explicit and unambiguous: destroy a published item, or destroy a non-empty Skeleton, and the process terminates. Silent corruption is not an option.

The three lifecycle signals — `onPublished`, `onUnpublishing`, `onMaskChanged` — fire at the exact moments when the registry's consistent state changes. `onPublished` fires after the item is inserted (find/query see the new item inside the callback). `onUnpublishing` fires before removal (find/query still see the item). Subscriptions return a `ScopedConnection` that auto-disconnects on destruction.

---

![Registry model showing Skeleton holding non-owning raw pointers to items](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iMjc2IiB2aWV3Qm94PSIwIDAgNzAwIDI3NiIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3MDAiIGhlaWdodD0iMjc2IiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNTAiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+UmVnaXN0cnkgTW9kZWwg4oCUIE5vbi1Pd25pbmcgUG9pbnRlciBSZWdpc3RyeTwvdGV4dD4KPHRleHQgeD0iMTU1IiB5PSI1MCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMSI+QXBwbGljYXRpb24gU3RhY2sgLyBIZWFwPC90ZXh0Pgo8dGV4dCB4PSI1MzUiIHk9IjUwIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjExIj5Ta2VsZXRvbiBSZWdpc3RyeSAoRmFzdEhhc2hNYXApPC90ZXh0Pgo8bGluZSB4MT0iMzUwIiB5MT0iMzgiIHgyPSIzNTAiIHkyPSIyNjgiIHN0cm9rZT0iI2UyZThmMCIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHJlY3QgeD0iMjgiIHk9IjY0IiB3aWR0aD0iMTk0IiBoZWlnaHQ9IjYwIiByeD0iNiIgZmlsbD0iI2RiZWFmZSIgc3Ryb2tlPSIjMjU2M2ViIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iMTI1IiB5PSI4MiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzFlM2E4YSIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9ImJvbGQiPkxvYWRTZW5zb3I8L3RleHQ+Cjx0ZXh0IHg9IjEyNSIgeT0iOTciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPlswMTowMTowMV08L3RleHQ+Cjx0ZXh0IHg9IjEyNSIgeT0iMTExIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5TZW5zb3IgfCBQcm92aWRlc1ZhbHVlPC90ZXh0Pgo8cmVjdCB4PSIyOCIgeT0iMTM4IiB3aWR0aD0iMTk0IiBoZWlnaHQ9IjYwIiByeD0iNiIgZmlsbD0iI2NmZmFmZSIgc3Ryb2tlPSIjMDg5MWIyIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iMTI1IiB5PSIxNTYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNjRlNjMiIGZvbnQtc2l6ZT0iMTIiIGZvbnQtd2VpZ2h0PSJib2xkIj5UZW1wU2Vuc29yPC90ZXh0Pgo8dGV4dCB4PSIxMjUiIHk9IjE3MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzAxOjAxOjAyXTwvdGV4dD4KPHRleHQgeD0iMTI1IiB5PSIxODUiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPlNlbnNvciB8IFByb3ZpZGVzVmFsdWU8L3RleHQ+CjxyZWN0IHg9IjI4IiB5PSIyMDgiIHdpZHRoPSIxOTQiIGhlaWdodD0iNDQiIHJ4PSI2IiBmaWxsPSIjZGNmY2U3IiBzdHJva2U9IiMxNmEzNGEiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxMjUiIHk9IjIyNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE0NTMyZCIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9ImJvbGQiPkRpc3BsYXlDb250cm9sbGVyPC90ZXh0Pgo8dGV4dCB4PSIxMjUiIHk9IjI0MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzAxOjAzOjAxXTwvdGV4dD4KPHJlY3QgeD0iMzY4IiB5PSI1NiIgd2lkdGg9IjMwMiIgaGVpZ2h0PSIxOTAiIHJ4PSI4IiBmaWxsPSIjZjhmYWZjIiBzdHJva2U9IiM5NGEzYjgiIHN0cm9rZS13aWR0aD0iMS41Ii8+Cjx0ZXh0IHg9IjUxOSIgeT0iNzYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtZmFtaWx5PSInQ291cmllciBOZXcnLG1vbm9zcGFjZSI+RmFzdEhhc2hNYXAmbHQ7Qm9uZUlkLCBTa2VsZXRvbkl0ZW0qJmd0OzwvdGV4dD4KPHJlY3QgeD0iMzg0IiB5PSI4OCIgd2lkdGg9IjI3MCIgaGVpZ2h0PSIyNiIgcng9IjQiIGZpbGw9IiNkYmVhZmUiIHN0cm9rZT0iIzI1NjNlYiIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHRleHQgeD0iNTE5IiB5PSIxMDUiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxZTNhOGEiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtZmFtaWx5PSInQ291cmllciBOZXcnLG1vbm9zcGFjZSI+WzAxOjAxOjAxXSAg4oaSICBwdHI8L3RleHQ+CjxyZWN0IHg9IjM4NCIgeT0iMTI0IiB3aWR0aD0iMjcwIiBoZWlnaHQ9IjI2IiByeD0iNCIgZmlsbD0iI2NmZmFmZSIgc3Ryb2tlPSIjMDg5MWIyIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSI1MTkiIHk9IjE0MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE2NGU2MyIgZm9udC1zaXplPSIxMSIgZm9udC1mYW1pbHk9IidDb3VyaWVyIE5ldycsbW9ub3NwYWNlIj5bMDE6MDE6MDJdICDihpIgIHB0cjwvdGV4dD4KPHJlY3QgeD0iMzg0IiB5PSIxNjAiIHdpZHRoPSIyNzAiIGhlaWdodD0iMjYiIHJ4PSI0IiBmaWxsPSIjZGNmY2U3IiBzdHJva2U9IiMxNmEzNGEiIHN0cm9rZS13aWR0aD0iMS41Ii8+Cjx0ZXh0IHg9IjUxOSIgeT0iMTc3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMTQ1MzJkIiBmb250LXNpemU9IjExIiBmb250LWZhbWlseT0iJ0NvdXJpZXIgTmV3Jyxtb25vc3BhY2UiPlswMTowMzowMV0gIOKGkiAgcHRyPC90ZXh0Pgo8cmVjdCB4PSIzODQiIHk9IjE5NiIgd2lkdGg9IjI3MCIgaGVpZ2h0PSIzNCIgcng9IjUiIGZpbGw9IiNmZmZmZmYiIHN0cm9rZT0iI2UyZThmMCIgc3Ryb2tlLXdpZHRoPSIxIi8+Cjx0ZXh0IHg9IjUxOSIgeT0iMjEwIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjExIj5yYXcgcG9pbnRlcnMg4oCUIFNrZWxldG9uIGRvZXMgTk9UIG93bjwvdGV4dD4KPHRleHQgeD0iNTE5IiB5PSIyMjQiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiPml0ZW1zLiBJdGVtcyBvd24gdGhlbXNlbHZlcy48L3RleHQ+CjxsaW5lIHgxPSIzODQiIHkxPSIxMDEiIHgyPSIyMjQiIHkyPSI5NCIgc3Ryb2tlPSIjMjU2M2ViIiBzdHJva2Utd2lkdGg9IjEuNSIgc3Ryb2tlLWRhc2hhcnJheT0iNSAyIi8+Cjxwb2x5Z29uIHBvaW50cz0iMjI0LDk0IDIzMi4yMTA4OTU2NzI4NzIwNiw4OS4zNTQ0NDM4MTcwMjE0MiAyMzEuNzczODEzNzczOTM1OTMsOTkuMzQ0ODg3MjIxMjc2NDQiIGZpbGw9IiMyNTYzZWIiLz4KPGxpbmUgeDE9IjM4NCIgeTE9IjEzNyIgeDI9IjIyNCIgeTI9IjE2OCIgc3Ryb2tlPSIjMDg5MWIyIiBzdHJva2Utd2lkdGg9IjEuNSIgc3Ryb2tlLWRhc2hhcnJheT0iNSAyIi8+Cjxwb2x5Z29uIHBvaW50cz0iMjI0LDE2OCAyMzAuOTAyODc5ODA3MjE5NjIsMTYxLjU2OTU4Mzk2NjI1MjMgMjMyLjgwNTAwNjY4NzQzMTI3LDE3MS4zODcwMTMwMjU0MDkxIiBmaWxsPSIjMDg5MWIyIi8+CjxsaW5lIHgxPSIzODQiIHkxPSIxNzMiIHgyPSIyMjQiIHkyPSIyMzAiIHN0cm9rZT0iIzE2YTM0YSIgc3Ryb2tlLXdpZHRoPSIxLjUiIHN0cm9rZS1kYXNoYXJyYXk9IjUgMiIvPgo8cG9seWdvbiBwb2ludHM9IjIyNCwyMzAgMjI5Ljg1ODExMjM4MTU4MTYzLDIyMi42MDUyMzcwMzM5MDI5NyAyMzMuMjE0MDE1OTU2OTYwMDUsMjMyLjAyNTMxNzI0NTQ5MTU1IiBmaWxsPSIjMTZhMzRhIi8+CjxkZWZzPgogIDxtYXJrZXIgaWQ9ImRhIiBtYXJrZXJXaWR0aD0iOCIgbWFya2VySGVpZ2h0PSI4IiByZWZYPSI0IiByZWZZPSI0IiBvcmllbnQ9ImF1dG8iPgogICAgPHBhdGggZD0iTTAsMCBMMCw4IEw4LDQgeiIgZmlsbD0iIzY0NzQ4YiIvPgogIDwvbWFya2VyPgo8L2RlZnM+Cjwvc3ZnPg==)

## Feature Inventory

### 1. Schema-Validated Addressing

Define a `HierarchySchema` once per application domain. Use `Bone<Schema, Levels...>` to construct typed addresses. The compiler verifies that each level's enum type matches the schema's specification at that depth. Child and parent relationships are derivable at compile time: `LoadBone::Parent` gives the parent Bone type; `LoadBone::template child<NextEnum::Value>` gives a child type.

### 2. BoneItem Base Classes

`BoneItem<Schema, Levels...>` (alias for `BasicBoneItem` with `DefaultMaskPolicy`) binds the compile-time bone identity to a runtime item. Derive from it, initialize all your members, call `this->publish(skeleton)` at the end of your constructor, and call `this->unpublish()` at the beginning of your destructor. The base class restricts `publish`, `unpublish`, and `setMask` to derived-class scope — external callers cannot accidentally bypass the lifecycle contract.

### 3. Reactive Lifecycle Signals

Subscribe once and stay informed as the registry changes. `onPublished` supports connection-chasing: if a new item arrives and you need to wire it up, do it in the callback — the new item is already visible via `find` and `query`. `onUnpublishing` supports teardown notification: react to an item's departure before it disappears from the registry. `onMaskChanged` notifies when an item's capabilities change at runtime, enabling dynamic routing updates.

### 4. Flexible Lookup

`find(BoneId)` returns a raw pointer to the item, or nullptr. `findAs<T>(BoneId)` adds a static cast — use only when the schema guarantees the concrete type at that address. `visitSubtree(BoneId, fn)` visits all items in ascending BoneId order (parent-before-child) within a subtree. `query(required, excluded)` and `querySubtree(root, required, excluded)` collect all items matching a capability mask across the full registry or within a subtree.

### 5. Dynamic Address Generation

When a compile-time schema doesn't fit — plugin systems, data-driven hierarchies, benchmark fixtures — `index2BoneId(prefix, index)` from `SkeletonUtilities.h` maps a flat zero-based index to a unique BoneId descendant of a prefix. Every distinct index produces a distinct BoneId; no result is an ancestor of another result for the same prefix.

---

## Why Not Alternatives?

### Why Not std::unordered_map?

`std::unordered_map<std::string, void*>` is a flat registry without address structure. It provides no hierarchy, no capability filtering, no lifecycle signals, and no compile-time validation of key types. Address strings like `"sensors/load"` are invisible to the compiler; typos silently produce nullptr at runtime. There is no notion of `isAncestorOf`, no subtree traversal, and no automatic notification when entries change.

| If You Need... | std::unordered_map | Skeleton |
|---------------|--------------------|----------|
| Type-safe addresses | No — strings at runtime | Yes — schema-validated Bones at compile time |
| Subtree traversal | No — flat structure | Yes — `visitSubtree`, `querySubtree` |
| Lifecycle signals | No — manual wiring | Yes — `onPublished`, `onUnpublishing`, `onMaskChanged` |
| Capability queries | No — no metadata | Yes — `SkeletonMask`, `query()` |
| Dangling-pointer safety | No — silent corruption | Yes — terminate-on-violation |

### Why Not an ECS Framework?

Entity-Component Systems (EnTT, flecs, etc.) optimize for cache-coherent iteration over large homogeneous component arrays. They are purpose-built for game entity management where thousands of entities share the same component set. Skeleton is purpose-built for the opposite problem: a moderate number of named, heterogeneous components that need to discover each other by address or capability, with lifecycle notifications and hierarchy-aware queries. ECS architectures have no concept of a "Bone address" or a "schema-validated hierarchy." Skeleton has no concept of archetype-based storage or parallel iteration.

### Why Not a Service Locator?

Classic service locators (e.g., a `std::unordered_map<std::type_index, void*>`) provide type-to-singleton lookup. They have no hierarchy, no capability description, no lifetime management, and no signals. They cannot express "find all sensor items under the network subsystem," only "find the one registered sensor type." Skeleton generalizes this pattern: addresses are structured, multiple items can occupy different nodes in the same subtree, and consumers react to changes rather than polling.

---

## The "Forever Stuck" Reality

Scientific computing clusters, automotive ECUs, and embedded systems often run fixed compiler toolchains for years, sometimes by contractual or certification requirement. Even when a future standard provides a closer analogue, the codebase may be locked to C++20. Skeleton requires only C++20 and has no external dependencies. It will not become a legacy liability when your cluster upgrades to C++23 or C++26 — there is nothing to migrate to, because no standard equivalent exists or is planned.

---

## Performance Characteristics

| Operation | Complexity | Mechanism |
|-----------|-----------|-----------|
| `find(BoneId)` | O(1) average | FastHashMap with BoneId key |
| `isAncestorOf(BoneId)` | O(1) | Bitmask comparison on 64-bit value |
| `publish` / `unpublish` | O(1) average | HashMap insert/erase |
| `query` / `visitSubtree` | O(N log N) | Linear scan + sort by BoneId |
| `querySubtree` | O(N log N) | Linear scan with ancestor check + sort |
| `index2BoneId` | O(depth consumed) | At most 8 child() calls |

### Where Skeleton Wins

For moderate-sized registries (up to thousands of items), `find` is O(1) and the BoneId key hash is cache-friendly. Schema-validated addresses eliminate an entire category of runtime errors — wrong-type items at a known address are impossible to publish because the Bone type enforces the schema. Lifecycle signals eliminate polling loops; observers react immediately when items arrive or depart.

### Where Skeleton Loses

`query` and `visitSubtree` are O(N log N): they scan all N published items and sort results. For registries with thousands of items queried in tight loops, this is the first profiling target. The sort dominates at large N; if query performance matters, consider maintaining a separate sorted structure at publish/unpublish time. Skeleton is also single-threaded; concurrent registries require external synchronization.

---

## Integration Points

```
Skeleton
    → uses
FastHashMap.h   (BoneId → SkeletonItem* registry)
Signal.h        (onPublished, onUnpublishing, onMaskChanged)
ScopeGuard.h    (traversal and publish rollback guards)
enforce.h       (precondition assertions in debug builds)
SkeletonFwd.h   (BoneId, HierarchySchema, SkeletonCapability, SkeletonMask)
CapabilityRegistry.h (name → capability-index registry; application band)

SkeletonUtilities.h
    → uses
SkeletonFwd.h   (BoneId::child())
```

---

## Final Assessment

Skeleton delivers on the Fat-P promise:

**Permanence.** No standard equivalent exists. The problem of typed hierarchical component discovery with lifecycle signals is not addressed by any C++ standard feature, and no proposal is in progress. Skeleton fills a permanent gap.

**Specialization.** Compile-time schema validation, BoneId canonical form, terminate-on-lifecycle-violation, and capability-masked queries are HPC-appropriate — they eliminate whole classes of runtime errors without runtime overhead for the properties you don't need.

**Control.** Choose your schema depth. Choose your capability set. Choose between `find` for exact lookup and `query` for capability-based discovery. Subscribe to exactly the lifecycle events you care about, with ScopedConnection auto-disconnection. Extend the mask policy via `BasicBoneItem<Schema, MaskPol, Levels...>` when `DefaultMaskPolicy` is not enough.

For systems where components must discover each other dynamically, be organized hierarchically, and depart safely, Skeleton is the solution.

---

*Skeleton.h / SkeletonFwd.h / SkeletonUtilities.h — Fat-P Library*
