---
doc_id: UM-SKELETON-001
doc_type: "User Manual"
title: "Skeleton"
fatp_components: ["Skeleton", "SkeletonFwd", "SkeletonUtilities"]
topics: ["typed hierarchical registry", "publish-subscribe", "BoneId addressing", "HierarchySchema", "capability mask", "lifecycle contract", "signals", "service discovery", "HAL", "plugin architecture"]
constraints: ["pointer stability required", "single-threaded", "lifecycle terminate-on-violation", "8-level depth limit", "enum values 0..255"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: null
build_modes: ["Debug", "Release"]
last_verified: "2026-03-01"
audience: ["C++ developers", "embedded systems engineers", "game engine developers", "AI assistants"]
status: "draft"
---

# User Manual - Skeleton

**Library:** fat_p C++ Utilities
**Standard:** C++20
**Type:** Header-only

---

**Scope:** Complete usage guide for the Skeleton component: HierarchySchema, BoneId, Bone, SkeletonItem, BoneItem, Skeleton, SkeletonMask, CapabilityRegistry, and SkeletonUtilities. Covers schema design, item authoring, publication lifecycle, lookup and traversal, capability queries, registered application capabilities, reactive signals, and dynamic address generation.

**Not covered:**
- Thread-safe registries (Skeleton is single-threaded; no ThreadSafeSkeleton in v1)
- Serialization of Skeleton state across process boundaries (BoneId serialization is covered; registry serialization is not)
- Benchmarking methodology (see `components/Skeleton/benchmarks/`)

**Prerequisites:** C++20; familiarity with CRTP-style base classes and virtual destructors; awareness of the distinction between owning and non-owning pointers

---

## User Manual Card

**Component:** Skeleton
**Primary use case:** Hierarchically organized runtime components that discover each other by typed address or capability without tight constructor coupling
**Integration pattern:** Create one Skeleton per application domain; derive item types from `BoneItem<Schema, Levels...>`; publish in the most-derived constructor, unpublish in the most-derived destructor
**Key API:** `HierarchySchema`, `Bone`, `BoneItem`, `SkeletonItem`, `Skeleton::find`, `Skeleton::query`, `Skeleton::visitSubtree`, `Skeleton::onPublished`, `makeMask`, `CapabilityRegistry`, `index2BoneId`
**std equivalent:** None. No standard equivalent exists or is planned.
**Migration from std:** Not applicable — Skeleton addresses a pattern not covered by any standard facility.
**Common mistakes:** Calling `publish` before all members are initialized; forgetting `unpublish` in the destructor; calling `publish` or `unpublish` from within a `visitSubtree` callback
**Performance notes:** `find` is O(1) average; `query` and `visitSubtree` are O(N log N) due to sort. See `components/Skeleton/results/` for current benchmark data.

---

## Table of Contents

1. [The Directory Service Story](#the-directory-service-story)
2. [Understanding the Data Model](#understanding-the-data-model)
   - [BoneId: The Hierarchical Address](#boneid-the-hierarchical-address)
   - [HierarchySchema: The Type Contract](#hierarchyschema-the-type-contract)
   - [SkeletonMask: The Capability Description](#skeletonmask-the-capability-description)
   - [CapabilityRegistry: Application Capabilities](#capabilityregistry-application-capabilities)
3. [Getting Started](#getting-started)
   - [Prerequisites and Integration](#prerequisites-and-integration)
   - [Your First Schema](#your-first-schema)
   - [Your First Item](#your-first-item)
   - [Your First Skeleton](#your-first-skeleton)
4. [Authoring Items](#authoring-items)
   - [The Lifecycle Contract](#the-lifecycle-contract)
   - [Why Terminate, Not Throw?](#why-terminate-not-throw)
   - [Deriving from BoneItem](#deriving-from-boneitem)
   - [Setting Capabilities](#setting-capabilities)
   - [Updating the Mask at Runtime](#updating-the-mask-at-runtime)
5. [Finding Items](#finding-items)
   - [find and findAs: Direct Lookup](#find-and-findas-direct-lookup)
   - [visitSubtree: Hierarchical Traversal](#visitsubtree-hierarchical-traversal)
   - [query and querySubtree: Capability Filtering](#query-and-querysubtree-capability-filtering)
6. [Reactive Signals](#reactive-signals)
   - [onPublished: Reacting to Arrivals](#onpublished-reacting-to-arrivals)
   - [onUnpublishing: Reacting to Departures](#onunpublishing-reacting-to-departures)
   - [onMaskChanged: Reacting to Capability Changes](#onmaskchanged-reacting-to-capability-changes)
   - [ScopedConnection Lifetimes](#scopedconnection-lifetimes)
   - [Reentrancy Rules](#reentrancy-rules)
7. [Designing a Schema](#designing-a-schema)
   - [Choosing Level Granularity](#choosing-level-granularity)
   - [Enum Value Constraints](#enum-value-constraints)
   - [Multiple Schemas in One Application](#multiple-schemas-in-one-application)
8. [Dynamic Addressing: SkeletonUtilities](#dynamic-addressing-skeletonutilities)
9. [BoneId in Depth](#boneid-in-depth)
   - [Canonical Form](#canonical-form)
   - [Ancestor Queries](#ancestor-queries)
   - [Serialization](#serialization)
10. [SkeletonItem Without BoneItem](#skeletonitem-without-boneitem)
11. [When to Use / When Not To](#when-to-use-when-not-to)
12. [Troubleshooting](#troubleshooting)
    - [Compilation Errors](#compilation-errors)
    - [Runtime Terminations](#runtime-terminations)
    - [Logic Errors](#logic-errors)
13. [API Reference](#api-reference)
14. [FAQ](#faq)

---

## The Directory Service Story

### The Wiring Problem

Every sufficiently complex C++ system has the same crisis at some point. You're building a HAL layer. There are twenty sensor types, twelve actuator types, a network module, a storage controller, and a display subsystem. Every component needs to talk to several others. The startup code — the function that wires everything together — has grown to three hundred lines of `component->setOtherComponent(otherComponent)` calls.

Then a new sensor type arrives. You add the class, add the member variable to the system object, and spend an hour chasing every place you need to thread the new pointer through. A week later, a sensor needs to be optional — sometimes present, sometimes not, depending on runtime configuration. Now you have `if (mOptionalSensor) mOptionalSensor->doThing()` scattered through a dozen files.

The wiring problem is not a sign that the code is badly organized. It is the inevitable consequence of tight coupling between components that have separate lifecycles. The solution is not better organization of the same pattern. It is a different pattern entirely: components publish themselves, and other components discover them.

### The Registry Mental Model

Think of Skeleton as a directory service — the kind a phone book provides, but with a strict address schema enforced at compile time and a notification system that fires whenever the contents change.

Each item in the registry has an *address* (its `BoneId`), a *name* (optional diagnostic string), and a *capability description* (its `SkeletonMask`). The address is hierarchical: a load sensor might live at `Root / Sensors / Load`, and you can query everything under `Root / Sensors` with a single `visitSubtree` call. The capability description is orthogonal to the address: you can ask "give me all items anywhere in the registry that provide readable sensor values," regardless of where they sit in the hierarchy.

The items are *self-registering*. Each item knows its own address — it is encoded in its type — and publishes itself onto a Skeleton when it is fully constructed. When it is destroyed, it removes itself. The rest of the system subscribes to arrival and departure events and wires itself up reactively.

---

## Understanding the Data Model

### BoneId: The Hierarchical Address

![BoneId 64-bit memory layout — eight level bytes packed into a single uint64_t](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3NDgiIGhlaWdodD0iMTcwIiB2aWV3Qm94PSIwIDAgNzQ4IDE3MCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3NDgiIGhlaWdodD0iMTcwIiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNzQiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+Qm9uZUlkIOKAlCA2NC1iaXQgUGFja2VkIEhpZXJhcmNoaWNhbCBBZGRyZXNzPC90ZXh0Pgo8cmVjdCB4PSIxMCIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNkYmVhZmUiIHN0cm9rZT0iIzI1NjNlYiIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjUxIiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzFlM2E4YSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDA8L3RleHQ+Cjx0ZXh0IHg9IjUxIiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzYzOjU2XTwvdGV4dD4KPHRleHQgeD0iNTEiIHk9Ijg3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMWUzYThhIiBmb250LXNpemU9IjEwIj4weDAxPC90ZXh0Pgo8cmVjdCB4PSIxMDEiIHk9IjQwIiB3aWR0aD0iODIiIGhlaWdodD0iNTIiIHJ4PSI1IiBmaWxsPSIjY2ZmYWZlIiBzdHJva2U9IiMwODkxYjIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxNDIiIHk9IjU4IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMTY0ZTYzIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+TGV2ZWwgMTwvdGV4dD4KPHRleHQgeD0iMTQyIiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzU1OjQ4XTwvdGV4dD4KPHRleHQgeD0iMTQyIiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE2NGU2MyIgZm9udC1zaXplPSIxMCI+MHgwMTwvdGV4dD4KPHJlY3QgeD0iMTkyIiB5PSI0MCIgd2lkdGg9IjgyIiBoZWlnaHQ9IjUyIiByeD0iNSIgZmlsbD0iI2RjZmNlNyIgc3Ryb2tlPSIjMTZhMzRhIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iMjMzIiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE0NTMyZCIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDI8L3RleHQ+Cjx0ZXh0IHg9IjIzMyIgeT0iNzMiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPls0Nzo0MF08L3RleHQ+Cjx0ZXh0IHg9IjIzMyIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTAiPjB4MDE8L3RleHQ+CjxyZWN0IHg9IjI4MyIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNlY2ZjY2IiIHN0cm9rZT0iIzY1YTMwZCIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjMyNCIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzNjUzMTQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCAzPC90ZXh0Pgo8dGV4dCB4PSIzMjQiIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bMzk6MzJdPC90ZXh0Pgo8dGV4dCB4PSIzMjQiIHk9Ijg3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj4weDAwPC90ZXh0Pgo8cmVjdCB4PSIzNzQiIHk9IjQwIiB3aWR0aD0iODIiIGhlaWdodD0iNTIiIHJ4PSI1IiBmaWxsPSIjZmVmOWMzIiBzdHJva2U9IiNjYThhMDQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI0MTUiIHk9IjU4IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNzEzZjEyIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+TGV2ZWwgNDwvdGV4dD4KPHRleHQgeD0iNDE1IiB5PSI3MyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+WzMxOjI0XTwvdGV4dD4KPHRleHQgeD0iNDE1IiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+MHgwMDwvdGV4dD4KPHJlY3QgeD0iNDY1IiB5PSI0MCIgd2lkdGg9IjgyIiBoZWlnaHQ9IjUyIiByeD0iNSIgZmlsbD0iI2ZmZWRkNSIgc3Ryb2tlPSIjZWE1ODBjIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNTA2IiB5PSI1OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzdjMmQxMiIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkxldmVsIDU8L3RleHQ+Cjx0ZXh0IHg9IjUwNiIgeT0iNzMiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPlsyMzoxNl08L3RleHQ+Cjx0ZXh0IHg9IjUwNiIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPjB4MDA8L3RleHQ+CjxyZWN0IHg9IjU1NiIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNmZWUyZTIiIHN0cm9rZT0iI2RjMjYyNiIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjU5NyIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM3ZjFkMWQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCA2PC90ZXh0Pgo8dGV4dCB4PSI1OTciIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bMTU6OF08L3RleHQ+Cjx0ZXh0IHg9IjU5NyIgeT0iODciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPjB4MDA8L3RleHQ+CjxyZWN0IHg9IjY0NyIgeT0iNDAiIHdpZHRoPSI4MiIgaGVpZ2h0PSI1MiIgcng9IjUiIGZpbGw9IiNlZGU5ZmUiIHN0cm9rZT0iIzdjM2FlZCIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjY4OCIgeT0iNTgiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM0YzFkOTUiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5MZXZlbCA3PC90ZXh0Pgo8dGV4dCB4PSI2ODgiIHk9IjczIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5bNzowXTwvdGV4dD4KPHRleHQgeD0iNjg4IiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+MHgwMDwvdGV4dD4KPHJlY3QgeD0iMTAiIHk9IjEwNiIgd2lkdGg9IjgyIiBoZWlnaHQ9IjI4IiByeD0iNSIgZmlsbD0iI2UwZTdmZiIgc3Ryb2tlPSIjNGY0NmU1IiBzdHJva2Utd2lkdGg9IjEuNSIgc3Ryb2tlLWRhc2hhcnJheT0iNCAyIi8+Cjx0ZXh0IHg9IjUxIiB5PSIxMjQiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzMTJlODEiIGZvbnQtc2l6ZT0iMTEiPmRlcHRoIGJ5dGU8L3RleHQ+CjxsaW5lIHgxPSI5NSIgeTE9IjEyMCIgeDI9IjE1NSIgeTI9IjEyMCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+Cjx0ZXh0IHg9IjE2MCIgeT0iMTI0IiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjExIj5zZXBhcmF0ZSBmaWVsZDsgbm90IHBhcnQgb2YgdGhlIDY0LWJpdCB2YWx1ZTwvdGV4dD4KPHRleHQgeD0iMzc0IiB5PSIxNTUiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiPkluYWN0aXZlIGxldmVscyBhcmUgYWx3YXlzIHplcm8gKGNhbm9uaWNhbCBmb3JtKS4gRXF1YWxpdHkgaXMgYSBzaW5nbGUgNjQtYml0IGludGVnZXIgY29tcGFyaXNvbi48L3RleHQ+CjxkZWZzPgogIDxtYXJrZXIgaWQ9ImRhIiBtYXJrZXJXaWR0aD0iOCIgbWFya2VySGVpZ2h0PSI4IiByZWZYPSI0IiByZWZZPSI0IiBvcmllbnQ9ImF1dG8iPgogICAgPHBhdGggZD0iTTAsMCBMMCw4IEw4LDQgeiIgZmlsbD0iIzY0NzQ4YiIvPgogIDwvbWFya2VyPgo8L2RlZnM+Cjwvc3ZnPg==)

`BoneId` is a 64-bit packed value representing a path through the hierarchy. It stores up to 8 levels, 8 bits per level. Level 0 occupies bits 63–56 (the most significant byte), level 1 occupies bits 55–48, and so on. Unused levels are always zero — this is the canonical form, and it is what makes equality checks and `std::hash` reliable.

You rarely construct `BoneId` values by hand. They are produced by `Bone<Schema, Levels...>::id()` at compile time, or by `BoneId::child(index)` and `BoneId::parent()` for runtime navigation, or by `index2BoneId(prefix, index)` for bulk dynamic generation.

Two `BoneId` values are equal if and only if they represent the same path. `isAncestorOf(other)` returns true if this BoneId is a strict prefix of `other`'s path — a node is not its own ancestor. Both operations are O(1): equality is a 64-bit integer comparison; `isAncestorOf` is a bitmask comparison of the active prefix bytes.

### HierarchySchema: The Type Contract

`HierarchySchema<Level0Enum, Level1Enum, ...Level7Enum>` binds each depth position to an enum type. The schema is the single point of truth for the address structure. It is a compile-time structure — there is no runtime schema object, only template instantiations.

```cpp
#include "SkeletonFwd.h"

using namespace fat_p::skeleton;

// Each enum type represents one level of the hierarchy.
// All level enums must be enum types; underlying values must fit in one byte (0..255).
enum class System    : uint8_t { Root = 1, Aux = 2 };
enum class Subsystem : uint8_t { Sensors = 1, Actuators = 2, Network = 3 };
enum class Channel   : uint8_t { Load = 0, Temp = 1, Pressure = 2 };

// The schema declares: depth 0 must be System, depth 1 must be Subsystem, depth 2 must be Channel.
using SysSchema = HierarchySchema<System, Subsystem, Channel>;
```

![Sample hierarchy tree built from HierarchySchema with System, Subsystem, and Channel levels](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iMzE4IiB2aWV3Qm94PSIwIDAgNzAwIDMxOCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3MDAiIGhlaWdodD0iMzE4IiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjY2JkNWUxIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNTAiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCI+SGllcmFyY2h5IFRyZWUg4oCUIEhpZXJhcmNoeVNjaGVtYSZsdDtTeXN0ZW0sIFN1YnN5c3RlbSwgQ2hhbm5lbCZndDs8L3RleHQ+Cjx0ZXh0IHg9IjgiIHk9IjYzIiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMDwvdGV4dD4KPHRleHQgeD0iOCIgeT0iMTQ1IiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMTwvdGV4dD4KPHRleHQgeD0iOCIgeT0iMjM1IiBmaWxsPSIjMWUyOTNiIiBmb250LXNpemU9IjEwIiBmb250LXdlaWdodD0iYm9sZCI+RGVwdGggMjwvdGV4dD4KPGxpbmUgeDE9IjgwIiB5MT0iMzQiIHgyPSI4MCIgeTI9IjI1NCIgc3Ryb2tlPSIjOTRhM2I4IiBzdHJva2Utd2lkdGg9IjEiLz4KPHJlY3QgeD0iMzQwIiB5PSI0NCIgd2lkdGg9IjE0NCIgaGVpZ2h0PSIzMCIgcng9IjE0IiBmaWxsPSIjZGJlYWZlIiBzdHJva2U9IiMyNTYzZWIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI0MTIiIHk9IjY0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMWUzYThhIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+U3lzdGVtOjpSb290PC90ZXh0Pgo8bGluZSB4MT0iNDEyIiB5MT0iNzQiIHgyPSIyMDAiIHkyPSIxMjYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPGxpbmUgeDE9IjQxMiIgeTE9Ijc0IiB4Mj0iNDIwIiB5Mj0iMTI2IiBzdHJva2U9IiNjYmQ1ZTEiIHN0cm9rZS13aWR0aD0iMS41Ii8+CjxsaW5lIHgxPSI0MTIiIHkxPSI3NCIgeDI9IjYyNCIgeTI9IjEyNiIgc3Ryb2tlPSIjY2JkNWUxIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8cmVjdCB4PSIxMjYiIHk9IjEyNiIgd2lkdGg9IjE0OCIgaGVpZ2h0PSIzMCIgcng9IjE0IiBmaWxsPSIjY2ZmYWZlIiBzdHJva2U9IiMwODkxYjIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIyMDAiIHk9IjE0NiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzE2NGU2MyIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPlN1Yjo6U2Vuc29yczwvdGV4dD4KPHJlY3QgeD0iMzQ2IiB5PSIxMjYiIHdpZHRoPSIxNDgiIGhlaWdodD0iMzAiIHJ4PSIxNCIgZmlsbD0iI2RjZmNlNyIgc3Ryb2tlPSIjMTZhMzRhIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNDIwIiB5PSIxNDYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5TdWI6OkFjdHVhdG9yczwvdGV4dD4KPHJlY3QgeD0iNTUwIiB5PSIxMjYiIHdpZHRoPSIxNDgiIGhlaWdodD0iMzAiIHJ4PSIxNCIgZmlsbD0iI2VjZmNjYiIgc3Ryb2tlPSIjNjVhMzBkIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNjI0IiB5PSIxNDYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzNjUzMTQiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5TdWI6Ok5ldHdvcms8L3RleHQ+CjxyZWN0IHg9Ijg0IiB5PSIyMTYiIHdpZHRoPSI4MCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxMjQiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OkxvYWQ8L3RleHQ+CjxsaW5lIHgxPSIyMDAiIHkxPSIxNTYiIHgyPSIxMjQiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHJlY3QgeD0iMTcyIiB5PSIyMTYiIHdpZHRoPSI4MCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIyMTIiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OlRlbXA8L3RleHQ+CjxsaW5lIHgxPSIyMDAiIHkxPSIxNTYiIHgyPSIyMTIiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHJlY3QgeD0iMjYwIiB5PSIyMTYiIHdpZHRoPSI5OCIgaGVpZ2h0PSIzMCIgcng9IjEyIiBmaWxsPSIjZWRlOWZlIiBzdHJva2U9IiM3YzNhZWQiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIzMDkiIHk9IjIzNiIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzRjMWQ5NSIgZm9udC1zaXplPSIxMSIgZm9udC13ZWlnaHQ9ImJvbGQiPkNoYW46OlByZXNzdXJlPC90ZXh0Pgo8bGluZSB4MT0iMjAwIiB5MT0iMTU2IiB4Mj0iMzA5IiB5Mj0iMjE2IiBzdHJva2U9IiNjYmQ1ZTEiIHN0cm9rZS13aWR0aD0iMS41Ii8+CjxyZWN0IHg9IjQwMCIgeT0iMjE2IiB3aWR0aD0iODQiIGhlaWdodD0iMzAiIHJ4PSIxMiIgZmlsbD0iI2VkZTlmZSIgc3Ryb2tlPSIjN2MzYWVkIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNDQyIiB5PSIyMzYiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM0YzFkOTUiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5DaGFuOjpGYW48L3RleHQ+CjxsaW5lIHgxPSI0MjAiIHkxPSIxNTYiIHgyPSI0NDIiIHkyPSIyMTYiIHN0cm9rZT0iI2NiZDVlMSIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHRleHQgeD0iMjAwIiB5PSIyNjAiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM3MTNmMTIiIGZvbnQtc2l6ZT0iMTAiPlN1Yjo6U2Vuc29ycy5pc0FuY2VzdG9yT2YoTG9hZCwgVGVtcCwgUHJlc3N1cmUpPC90ZXh0Pgo8dGV4dCB4PSI4NCIgeT0iMjc2IiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIiBmb250LWZhbWlseT0iJ0NvdXJpZXIgTmV3Jyxtb25vc3BhY2UiPkJvbmUmbHQ7UywgU3lzOjpSb290LCBTdWI6OlNlbnNvcnMsIENoYW46OkxvYWQmZ3Q7PC90ZXh0PgoKPC9zdmc+)

`SysSchema::expected_type<0>` is `System`. `SysSchema::expected_type<2>` is `Channel`. `SysSchema::kMaxDepth` is 3. A `Bone` instantiated against this schema checks at compile time that each level value has the enum type the schema expects at that depth. Passing a `Subsystem` value where a `System` is expected is a hard error.

### SkeletonMask: The Capability Description

A `SkeletonMask` is an **unbounded** capability bitset (a normalized word vector — it grows to the highest set index; masks of different storage widths compare and combine correctly). It is not a `std::bitset`: bitset idioms that presume a fixed width — no-argument `set()`, `all()`, `size()`, `to_ulong()` — are deliberately not provided. Use `set(i)`, `reset(i)`, `test(i)`, `none()`, `any()`, `count()`, the set operators (`&`, `|`, `&=`, `|=`, `==`), and `toString()` for diagnostics.

The capability *vocabulary* is open and registered, not hardcoded. Indices `0–31` are the reserved **framework band**, pre-registered from the `SkeletonCapability` enum:

```
Bits 0–7:   Category   — Sensor, Controller, Display, Network, Storage
Bits 8–15:  Providers  — ProvidesValue, ProvidesCommand, ProvidesStatus,
                         ValueBinary, ValueContinuous, ValueDiscrete
Bits 16–23: Consumers  — ConsumesValue, ConsumesCommand, ConsumesStatus
Bits 24–31: Properties — Readable, Writable, Serializable, NetworkVisible
```

The value-kind bits qualify a provided value orthogonally to provider-ness: a relay is `ProvidesValue + ValueBinary`, an analog channel is `ProvidesValue + ValueContinuous` — a new value kind never needs a new provider bit.

Build masks with `makeMask()`:

```cpp
using namespace fat_p::skeleton;

// A temperature sensor that provides readable continuous values
auto tempMask = makeMask(SkeletonCapability::Sensor,
                         SkeletonCapability::ProvidesValue,
                         SkeletonCapability::ValueContinuous,
                         SkeletonCapability::Readable);

// A display controller that consumes values and is writable
auto dispMask = makeMask(SkeletonCapability::Display,
                         SkeletonCapability::ConsumesValue,
                         SkeletonCapability::Writable);
```

`makeMask` accepts any number of arguments — framework capabilities and registered application capability indices (`std::size_t`), freely mixed. Passing `SkeletonCapability::Count` terminates the process: `Count` (32) is the framework band size and the first application index, not a capability.

### CapabilityRegistry: Application Capabilities

Applications extend the vocabulary at init through `CapabilityRegistry` (`CapabilityRegistry.h`): register a capability by name and receive an allocated index from `kFrameworkCapabilityBand` (32) upward. Allocation is sequential and collision-free by construction; registration is idempotent by name, so independent modules may register a shared name and receive the same index. Register during startup, before items are constructed, and never afterwards — the same immutable-after-init contract as every other registry.

```cpp
using namespace fat_p::skeleton;

const std::size_t hydraulic =
    CapabilityRegistry::instance().registerCapability("App.Hydraulic");

auto pumpMask = makeMask(SkeletonCapability::Controller,
                         SkeletonCapability::ProvidesValue,
                         hydraulic);

// Later, anywhere: query by the registered capability like any other bit.
auto pumps = skeleton.query(makeMask(hydraulic));
```

`find(name)` returns the index for a registered name; `name(index)` returns the name for an index (empty for reserved/unregistered slots); `highWater()` is one past the highest allocated index. There is no ceiling on how many capabilities can exist — a sanity enforce far above any real vocabulary (index < 2^20) guards against corrupted indices only.

---

## Getting Started

### Prerequisites and Integration

Include `Skeleton.h` for the full implementation. Include only `SkeletonFwd.h` in headers that need schema types, BoneId, or SkeletonMask but should not pull in the full Skeleton machinery — forward-declaration files, shared type headers, plugin interfaces. Include `SkeletonUtilities.h` only when you need `index2BoneId`.

```cpp
// In headers that define the schema and item types:
#include "SkeletonFwd.h"

// In translation units that implement item bodies or use Skeleton directly:
#include "Skeleton.h"

// Only when dynamic address generation is needed:
#include "SkeletonUtilities.h"
```

### Your First Schema

Design a schema that reflects your application's natural hierarchy. Start with the broadest categorization at depth 0 and narrow toward the most specific address at the deepest level.

```cpp
#include "SkeletonFwd.h"
using namespace fat_p::skeleton;

// A three-level hierarchy for a system with sensors and actuators
enum class Domain   : uint8_t { System = 1 };
enum class Category : uint8_t { Sensors = 1, Actuators = 2 };
enum class Node     : uint8_t { Load = 1, Temp = 2, Fan = 3, Valve = 4 };

using AppSchema = HierarchySchema<Domain, Category, Node>;
```

The enum underlying type must be an integer type and each value must fit in one byte (0..255). Using `uint8_t` everywhere makes this explicit. Do not assign the value `0` to a meaningful enum member if you intend to use null-checking — `BoneId::isNull()` checks depth, not value, but values of zero can cause confusion. Prefer starting meaningful enum values at 1.

### Your First Item

Derive from `BoneItem<Schema, Levels...>`. Initialize all your members, then call `this->publish(skeleton)` at the end of the constructor body. Call `this->unpublish()` at the beginning of the destructor body, before any member teardown. This ordering is not optional — it is a hard contract.

```cpp
#include "Skeleton.h"
using namespace fat_p::skeleton;

class LoadSensor final
    : public BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>
{
public:
    using Base = BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>;

    explicit LoadSensor(Skeleton& sk, double initialValue = 0.0)
        : Base(makeMask(SkeletonCapability::Sensor,
                        SkeletonCapability::ProvidesValue,
                        SkeletonCapability::Readable),
               "load_sensor")   // optional diagnostic name
        , mValue(initialValue)  // all members initialized before publish
    {
        this->publish(sk);      // last line of constructor body
    }

    ~LoadSensor() override
    {
        this->unpublish();      // first line of destructor body
        // mValue destructs normally afterward
    }

    [[nodiscard]] double value() const noexcept { return mValue; }

private:
    double mValue;
};
```

Notice the key points: the `using Base` alias names the full `BoneItem` instantiation; the `Base` constructor receives the initial mask and optional name; `publish` is the last statement in the constructor; `unpublish` is the first statement in the destructor; the class is `final` (not strictly required, but the most common pattern).

### Your First Skeleton

Construct a `Skeleton` with an optional diagnostic name. The name appears in fatal error messages and `dump()` output.

```cpp
#include "Skeleton.h"
using namespace fat_p::skeleton;

int main()
{
    Skeleton system("app_system");

    // Items are constructed with a reference to the Skeleton;
    // they publish themselves during construction.
    LoadSensor load(system, 42.0);
    // load is now visible in system

    // find by exact address
    SkeletonItem* item = system.find(
        Bone<AppSchema, Domain::System, Category::Sensors, Node::Load>::id());
    if (item)
    {
        // findAs performs a static_cast -- safe when the schema guarantees the type
        auto* sensor = system.findAs<LoadSensor>(
            Bone<AppSchema, Domain::System, Category::Sensors, Node::Load>::id());
        if (sensor)
        {
            std::cout << "Load: " << sensor->value() << "\n";
        }
    }

    // LoadSensor destructor calls unpublish() before system is destroyed.
    // Both load and system destruct cleanly.
    return 0;
}
```

The `Skeleton` destructor terminates the process if any items remain published when it runs. This means the Skeleton must outlive all items that publish onto it — typically by being declared first (and thus destroyed last) in the enclosing scope.

---

## Authoring Items

### The Lifecycle Contract

The lifecycle contract is strict and symmetric:

- **Publish:** Call `this->publish(skeleton)` in the **most-derived constructor body**, as the last statement, after all members are initialized.
- **Unpublish:** Call `this->unpublish()` in the **most-derived destructor body**, as the first statement, before any member teardown begins.

"Most-derived" is the critical phrase. If you have an inheritance chain `A : B : C` (where C is the base), C's constructor runs first — but C should not publish, because A's members are not yet initialized. A's constructor runs last and A's destructor runs first, so A is the correct place to call `publish` and `unpublish`.

```cpp
// Multi-level hierarchy -- only the most-derived class calls publish/unpublish
class SensorBase : public BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>
{
protected:
    using Base = BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>;

    explicit SensorBase(SkeletonMask mask, std::string name)
        : Base(mask, std::move(name))
        , mCalibration(1.0)
    {
        // DO NOT call publish here -- derived class members not yet initialized
    }

    ~SensorBase() override
    {
        // DO NOT call unpublish here -- already called by most-derived destructor
    }

    double mCalibration;
};

class CalibratedLoadSensor final : public SensorBase
{
public:
    explicit CalibratedLoadSensor(Skeleton& sk, double rawValue)
        : SensorBase(makeMask(SkeletonCapability::Sensor,
                               SkeletonCapability::ProvidesValue,
                               SkeletonCapability::Readable),
                     "calibrated_load")
        , mRawValue(rawValue)         // all members initialized
    {
        this->publish(sk);            // now safe to publish
    }

    ~CalibratedLoadSensor() override
    {
        this->unpublish();            // remove from registry first
        // mRawValue destructs after unpublish
    }

private:
    double mRawValue;
};
```

![Lifecycle contract showing construction order with publish last and destruction order with unpublish first](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iMzU1IiB2aWV3Qm94PSIwIDAgNzAwIDM1NSIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3MDAiIGhlaWdodD0iMzU1IiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNTAiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+TGlmZWN5Y2xlIENvbnRyYWN0IOKAlCBDb25zdHJ1Y3Rpb24gYW5kIERlc3RydWN0aW9uIE9yZGVyPC90ZXh0Pgo8dGV4dCB4PSIxOTUiIHk9IjUwIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMTZhMzRhIiBmb250LXNpemU9IjEyIiBmb250LXdlaWdodD0iYm9sZCI+Q09OU1RSVUNUSU9OPC90ZXh0Pgo8dGV4dCB4PSI1MDUiIHk9IjUwIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjZGMyNjI2IiBmb250LXNpemU9IjEyIiBmb250LXdlaWdodD0iYm9sZCI+REVTVFJVQ1RJT048L3RleHQ+CjxsaW5lIHgxPSIzNTAiIHkxPSIzNiIgeDI9IjM1MCIgeTI9IjMyNSIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8cmVjdCB4PSI1OCIgeT0iNjQiIHdpZHRoPSIyNjQiIGhlaWdodD0iMzYiIHJ4PSI2IiBmaWxsPSIjZjhmYWZjIiBzdHJva2U9IiM5NGEzYjgiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxOTAiIHk9Ijg3IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMzM0MTU1IiBmb250LXNpemU9IjEyIiBmb250LXdlaWdodD0ibm9ybWFsIj4xLiBCYXNlIGNsYXNzIGNvbnN0cnVjdG9ycyBydW48L3RleHQ+CjxsaW5lIHgxPSIxOTAiIHkxPSIxMDAiIHgyPSIxOTAiIHkyPSIxMTIiIHN0cm9rZT0iIzY0NzQ4YiIgc3Ryb2tlLXdpZHRoPSIxLjUiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+CjxyZWN0IHg9IjU4IiB5PSIxMTIiIHdpZHRoPSIyNjQiIGhlaWdodD0iMzYiIHJ4PSI2IiBmaWxsPSIjZjhmYWZjIiBzdHJva2U9IiM5NGEzYjgiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxOTAiIHk9IjEzNSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzMzNDE1NSIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9Im5vcm1hbCI+Mi4gTWVtYmVyIHZhcmlhYmxlcyBpbml0aWFsaXplZDwvdGV4dD4KPGxpbmUgeDE9IjE5MCIgeTE9IjE0OCIgeDI9IjE5MCIgeTI9IjE2MCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIgbWFya2VyLWVuZD0idXJsKCNkYSkiLz4KPHJlY3QgeD0iNTgiIHk9IjE2MCIgd2lkdGg9IjI2NCIgaGVpZ2h0PSI0NCIgcng9IjYiIGZpbGw9IiNkY2ZjZTciIHN0cm9rZT0iIzE2YTM0YSIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjE5MCIgeT0iMTgyIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMTQ1MzJkIiBmb250LXNpemU9IjEyIiBmb250LXdlaWdodD0iYm9sZCI+My4gdGhpcy0mZ3Q7cHVibGlzaChza2VsZXRvbik8L3RleHQ+Cjx0ZXh0IHg9IjE5MCIgeT0iMTk2IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjEwIj5sYXN0IGxpbmUgb2YgbW9zdC1kZXJpdmVkIGN0b3I8L3RleHQ+CjxsaW5lIHgxPSIxOTAiIHkxPSIyMDQiIHgyPSIxOTAiIHkyPSIyMTYiIHN0cm9rZT0iIzY0NzQ4YiIgc3Ryb2tlLXdpZHRoPSIxLjUiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+CjxyZWN0IHg9IjU4IiB5PSIyMTYiIHdpZHRoPSIyNjQiIGhlaWdodD0iMzYiIHJ4PSI2IiBmaWxsPSIjZTBlN2ZmIiBzdHJva2U9IiM0ZjQ2ZTUiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxOTAiIHk9IjIzOSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzMxMmU4MSIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9Im5vcm1hbCI+SXRlbSB2aXNpYmxlIHZpYSBmaW5kKCkgLyBxdWVyeSgpPC90ZXh0Pgo8cmVjdCB4PSIzNzgiIHk9IjY0IiB3aWR0aD0iMjY0IiBoZWlnaHQ9IjM2IiByeD0iNiIgZmlsbD0iI2UwZTdmZiIgc3Ryb2tlPSIjNGY0NmU1IiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNTEwIiB5PSI4NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzMxMmU4MSIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9Im5vcm1hbCI+SXRlbSBzdGlsbCBpbiByZWdpc3RyeTwvdGV4dD4KPGxpbmUgeDE9IjUxMCIgeTE9IjEwMCIgeDI9IjUxMCIgeTI9IjExMiIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIgbWFya2VyLWVuZD0idXJsKCNkYSkiLz4KPHJlY3QgeD0iMzc4IiB5PSIxMTIiIHdpZHRoPSIyNjQiIGhlaWdodD0iNDQiIHJ4PSI2IiBmaWxsPSIjZmVlMmUyIiBzdHJva2U9IiNkYzI2MjYiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI1MTAiIHk9IjEzNCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzdmMWQxZCIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9ImJvbGQiPjEuIHRoaXMtJmd0O3VucHVibGlzaCgpPC90ZXh0Pgo8dGV4dCB4PSI1MTAiIHk9IjE0OCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMCI+Zmlyc3QgbGluZSBvZiBtb3N0LWRlcml2ZWQgZHRvcjwvdGV4dD4KPGxpbmUgeDE9IjUxMCIgeTE9IjE1NiIgeDI9IjUxMCIgeTI9IjE2OCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIgbWFya2VyLWVuZD0idXJsKCNkYSkiLz4KPHJlY3QgeD0iMzc4IiB5PSIxNjgiIHdpZHRoPSIyNjQiIGhlaWdodD0iMzYiIHJ4PSI2IiBmaWxsPSIjZjhmYWZjIiBzdHJva2U9IiM5NGEzYjgiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI1MTAiIHk9IjE5MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzMzNDE1NSIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9Im5vcm1hbCI+Mi4gTWVtYmVyIHZhcmlhYmxlcyBkZXN0cm95ZWQ8L3RleHQ+CjxsaW5lIHgxPSI1MTAiIHkxPSIyMDQiIHgyPSI1MTAiIHkyPSIyMTYiIHN0cm9rZT0iIzY0NzQ4YiIgc3Ryb2tlLXdpZHRoPSIxLjUiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+CjxyZWN0IHg9IjM3OCIgeT0iMjE2IiB3aWR0aD0iMjY0IiBoZWlnaHQ9IjM2IiByeD0iNiIgZmlsbD0iI2Y4ZmFmYyIgc3Ryb2tlPSIjOTRhM2I4IiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNTEwIiB5PSIyMzkiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzMzQxNTUiIGZvbnQtc2l6ZT0iMTIiIGZvbnQtd2VpZ2h0PSJub3JtYWwiPjMuIEJhc2UgY2xhc3MgZGVzdHJ1Y3RvcnMgcnVuPC90ZXh0Pgo8cmVjdCB4PSIzNzgiIHk9IjI2NiIgd2lkdGg9IjI2NCIgaGVpZ2h0PSI0NCIgcng9IjYiIGZpbGw9IiNmZWUyZTIiIHN0cm9rZT0iI2RjMjYyNiIgc3Ryb2tlLXdpZHRoPSIyIi8+Cjx0ZXh0IHg9IjUxMCIgeT0iMjgzIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjN2YxZDFkIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+TWlzcyB1bnB1Ymxpc2goKSDihpI8L3RleHQ+Cjx0ZXh0IHg9IjUxMCIgeT0iMjk4IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjN2YxZDFkIiBmb250LXNpemU9IjExIiBmb250LXdlaWdodD0iYm9sZCI+c3RkOjp0ZXJtaW5hdGUoKSBpbiB+U2tlbGV0b25JdGVtPC90ZXh0Pgo8bGluZSB4MT0iNTEwIiB5MT0iMjUyIiB4Mj0iNTEwIiB5Mj0iMjY2IiBzdHJva2U9IiNkYzI2MjYiIHN0cm9rZS13aWR0aD0iMS41IiBzdHJva2UtZGFzaGFycmF5PSI0IDIiIG1hcmtlci1lbmQ9InVybCgjZGEpIi8+CjxyZWN0IHg9IjEwMCIgeT0iMjk1IiB3aWR0aD0iMTkwIiBoZWlnaHQ9IjI4IiByeD0iNSIgZmlsbD0iI2UwZTdmZiIgc3Ryb2tlPSIjNGY0NmU1IiBzdHJva2Utd2lkdGg9IjEiLz4KPHRleHQgeD0iMTk1IiB5PSIzMTMiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMzMTJlODEiIGZvbnQtc2l6ZT0iMTEiPiJtb3N0LWRlcml2ZWQiID0gZmluYWwgY2xhc3M8L3RleHQ+CjxkZWZzPgogIDxtYXJrZXIgaWQ9ImRhIiBtYXJrZXJXaWR0aD0iOCIgbWFya2VySGVpZ2h0PSI4IiByZWZYPSI0IiByZWZZPSI0IiBvcmllbnQ9ImF1dG8iPgogICAgPHBhdGggZD0iTTAsMCBMMCw4IEw4LDQgeiIgZmlsbD0iIzY0NzQ4YiIvPgogIDwvbWFya2VyPgo8L2RlZnM+Cjwvc3ZnPg==)

### Why Terminate, Not Throw?

Destroying a published item calls `std::terminate`. This may seem severe. It is a deliberate choice rooted in the alternative's failure mode.

If the destructor threw instead, it would propagate through the stack unwinding of whatever exception is already in flight — itself illegal in C++, leading to `std::terminate` anyway via the double-exception rule. Returning silently would leave a dangling pointer in the registry, causing memory corruption or a crash at the next lookup — far from the root cause, with no diagnostic.

Terminating in the destructor fires immediately at the site of the mistake: a published item being destroyed without calling `unpublish` first. The fatal message printed to stderr names the item and its BoneId. Debug builds will produce a stack trace. The root cause is unambiguous.

In release builds, the check uses `std::fprintf` to avoid heap allocation in a noexcept context. The raw BoneId value and depth are sufficient to identify the item without calling `toString()`.

### Deriving from BoneItem

`BoneItem<Schema, Levels...>` is an alias for `BasicBoneItem<Schema, DefaultMaskPolicy, Levels...>`. For almost all use cases, `BoneItem` is the right choice. The `publish`, `unpublish`, and `setMask` methods are `protected` in `BasicBoneItem` — they are deliberately inaccessible to external callers. Only the most-derived class should manage publication.

```cpp
// The BoneType is available inside BasicBoneItem for derived classes that need it
class MySensor final
    : public BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>
{
    using Base = BoneItem<AppSchema, Domain::System, Category::Sensors, Node::Load>;

    // Base::BoneType is Bone<AppSchema, Domain::System, Category::Sensors, Node::Load>
    // Base::BoneType::id() gives the compile-time BoneId
    // Base::BoneType::Parent is Bone<AppSchema, Domain::System, Category::Sensors>
};
```

### Setting Capabilities

Pass the initial mask to the `BoneItem` base constructor. The mask is set before publication and does not change unless you call `setMask` explicitly.

```cpp
explicit MySensor(Skeleton& sk)
    : Base(makeMask(SkeletonCapability::Sensor,
                    SkeletonCapability::ProvidesValue,
                    SkeletonCapability::Readable),
           "my_sensor")
{
    this->publish(sk);
}
```

An empty mask (`SkeletonMask{}`) is valid. Items with empty masks are discoverable by address but invisible to capability queries that require any bit set.

### Updating the Mask at Runtime

Call `setMask(newMask)` from within the item class to change capabilities. If the item is currently published, this fires `onMaskChanged` on the Skeleton, passing the item and the old mask. If the item is not published, the mask is updated locally without any signal.

```cpp
void MySensor::setOffline()
{
    // Remove Readable; add no new capability
    this->setMask(makeMask(SkeletonCapability::Sensor,
                           SkeletonCapability::ProvidesValue));
    // Skeleton::onMaskChanged fires immediately if published
}
```

The full mask is replaced; `setMask` does not perform bitwise OR with the previous value. If you want to add or remove individual bits, compute the new mask before calling `setMask`.

---

## Finding Items

### find and findAs: Direct Lookup

`find(BoneId id)` returns a raw pointer to the item, or nullptr if nothing is published at that address. The lookup is O(1) average via the internal `FastHashMap`.

```cpp
Skeleton system("app");
LoadSensor load(system);

// Obtain the BoneId from the Bone type -- compile-time, zero runtime cost
constexpr BoneId loadId =
    Bone<AppSchema, Domain::System, Category::Sensors, Node::Load>::id();

SkeletonItem* item = system.find(loadId);
if (item)
{
    std::cout << item->name() << "\n";  // "load_sensor"
}
```

`findAs<T>(BoneId id)` adds a `static_cast<T*>`. Use it only when the schema guarantees the concrete type at that address. There is no dynamic type check — this is a deliberate performance choice. If the item at the given address has a different dynamic type, behavior is undefined.

```cpp
// Safe: the schema guarantees Node::Load is always a LoadSensor
LoadSensor* sensor = system.findAs<LoadSensor>(loadId);
if (sensor)
{
    std::cout << sensor->value() << "\n";
}
```

Both `find` and `findAs` have const overloads that return `const SkeletonItem*` / `const T*` respectively.

### visitSubtree: Hierarchical Traversal

`visitSubtree(BoneId root, fn)` calls `fn(SkeletonItem&)` for each item whose BoneId is exactly `root` or a descendant of `root`, in ascending BoneId order (parent-before-child). The traversal is O(N log N): all N published items are scanned, and results are sorted.

```cpp
// Visit all sensor items under Category::Sensors
constexpr BoneId sensorsRoot =
    Bone<AppSchema, Domain::System, Category::Sensors>::id();

system.visitSubtree(sensorsRoot, [](SkeletonItem& item)
{
    std::cout << item.boneId().toString() << " " << item.name() << "\n";
});
// Output (in ascending BoneId order, parent first):
//   [01:01] my_base_sensor   (the Category::Sensors item, if published)
//   [01:01:01] load_sensor   (depth 3 child)
//   [01:01:02] temp_sensor   (depth 3 child)
```

Publishing or unpublishing items during a `visitSubtree` traversal terminates the process. The mutation-guard check fires immediately with a diagnostic message naming the Skeleton.

The const overload of `visitSubtree` yields `const SkeletonItem&` and does not allow mutation of items via the callback.

### query and querySubtree: Capability Filtering

`query(required, excluded)` returns a `std::vector<SkeletonItem*>` containing every published item whose mask has all bits in `required` set and no bits in `excluded` set. Order is ascending BoneId. Both parameters have default value `{}` (empty mask): an empty `required` mask matches every item; an empty `excluded` mask excludes nothing.

```cpp
using SC = SkeletonCapability;

// All readable sensor items, anywhere in the hierarchy
auto readableSensors = system.query(
    makeMask(SC::Sensor, SC::Readable));

// All items that provide values but are NOT network-visible
auto localProviders = system.query(
    makeMask(SC::ProvidesValue),
    makeMask(SC::NetworkVisible));  // excluded
```

`querySubtree(BoneId root, required, excluded)` applies the same mask predicate but restricts results to the subtree rooted at `root` (inclusive). Use this when you want capability-filtered items within a specific branch of the hierarchy.

```cpp
// All readable sensors under Category::Sensors
auto subsensorReadable = system.querySubtree(
    sensorsRoot,
    makeMask(SC::Sensor, SC::Readable));
```

Both `query` and `querySubtree` have const overloads returning `std::vector<const SkeletonItem*>`.

---

## Reactive Signals

Skeleton exposes three signals. Each returns a `ScopedConnection` that auto-disconnects when it goes out of scope. All callbacks run on the same thread as the operation that triggered them.

![Signal emission timeline for onPublished, onUnpublishing, and onMaskChanged relative to registry state](data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI3MDAiIGhlaWdodD0iMzA4IiB2aWV3Qm94PSIwIDAgNzAwIDMwOCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+CjxyZWN0IHdpZHRoPSI3MDAiIGhlaWdodD0iMzA4IiByeD0iOCIgZmlsbD0iI2ZmZmZmZiIgc3Ryb2tlPSIjZTJlOGYwIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIzNTAiIHk9IjI0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjMGYxNzJhIiBmb250LXNpemU9IjEzIiBmb250LXdlaWdodD0iYm9sZCIgZm9udC1mYW1pbHk9IidTZWdvZSBVSScsc2Fucy1zZXJpZiI+U2lnbmFsIEVtaXNzaW9uIE9yZGVyaW5nPC90ZXh0Pgo8bGluZSB4MT0iNTgiIHkxPSI0NiIgeDI9IjU4IiB5Mj0iMjg4IiBzdHJva2U9IiM2NDc0OGIiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSI0OCIgeT0iNDQiIHRleHQtYW5jaG9yPSJlbmQiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTAiPnRpbWU8L3RleHQ+CjxsaW5lIHgxPSI1MiIgeTE9IjgwIiB4Mj0iNjQiIHkyPSI4MCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8bGluZSB4MT0iNTIiIHkxPSIxMjgiIHgyPSI2NCIgeTI9IjEyOCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8bGluZSB4MT0iNTIiIHkxPSIxODAiIHgyPSI2NCIgeTI9IjE4MCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8bGluZSB4MT0iNTIiIHkxPSIyMzIiIHgyPSI2NCIgeTI9IjIzMiIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8bGluZSB4MT0iNTIiIHkxPSIyODAiIHgyPSI2NCIgeTI9IjI4MCIgc3Ryb2tlPSIjNjQ3NDhiIiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSI3MiIgeT0iNDQiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5wdWJsaXNoKCk8L3RleHQ+CjxsaW5lIHgxPSI1OCIgeTE9IjQ2IiB4Mj0iNTgiIHkyPSIxNzgiIHN0cm9rZT0iIzI1NjNlYiIgc3Ryb2tlLXdpZHRoPSIyIi8+CjxyZWN0IHg9IjcyIiB5PSI2MiIgd2lkdGg9IjI0OCIgaGVpZ2h0PSIzMCIgcng9IjUiIGZpbGw9IiNkYmVhZmUiIHN0cm9rZT0iIzI1NjNlYiIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHRleHQgeD0iMTk2IiB5PSI4MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzFlM2E4YSIgZm9udC1zaXplPSIxMSI+SXRlbSBpbnNlcnRlZCBpbnRvIHJlZ2lzdHJ5PC90ZXh0Pgo8cmVjdCB4PSI3MiIgeT0iMTEyIiB3aWR0aD0iMjQ4IiBoZWlnaHQ9IjQ0IiByeD0iNSIgZmlsbD0iI2RjZmNlNyIgc3Ryb2tlPSIjMTZhMzRhIiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iMTk2IiB5PSIxMzAiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTIiIGZvbnQtd2VpZ2h0PSJib2xkIj5vblB1Ymxpc2hlZCBmaXJlczwvdGV4dD4KPHRleHQgeD0iMTk2IiB5PSIxNDciIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiMxNDUzMmQiIGZvbnQtc2l6ZT0iMTAiPmZpbmQoKSAvIHF1ZXJ5KCkgc2VlIG5ldyBpdGVtIGhlcmU8L3RleHQ+Cjx0ZXh0IHg9IjcyIiB5PSIxNzQiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj51bnB1Ymxpc2goKTwvdGV4dD4KPGxpbmUgeDE9IjU4IiB5MT0iMTc4IiB4Mj0iNTgiIHkyPSIyODgiIHN0cm9rZT0iI2RjMjYyNiIgc3Ryb2tlLXdpZHRoPSIyIi8+CjxyZWN0IHg9IjcyIiB5PSIxODYiIHdpZHRoPSIyNDgiIGhlaWdodD0iNDQiIHJ4PSI1IiBmaWxsPSIjZmVlMmUyIiBzdHJva2U9IiNkYzI2MjYiIHN0cm9rZS13aWR0aD0iMiIvPgo8dGV4dCB4PSIxOTYiIHk9IjIwNCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzdmMWQxZCIgZm9udC1zaXplPSIxMiIgZm9udC13ZWlnaHQ9ImJvbGQiPm9uVW5wdWJsaXNoaW5nIGZpcmVzPC90ZXh0Pgo8dGV4dCB4PSIxOTYiIHk9IjIyMSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzdmMWQxZCIgZm9udC1zaXplPSIxMCI+ZmluZCgpIC8gcXVlcnkoKSBzdGlsbCBzZWUgaXRlbSBoZXJlPC90ZXh0Pgo8cmVjdCB4PSI3MiIgeT0iMjQyIiB3aWR0aD0iMjQ4IiBoZWlnaHQ9IjMwIiByeD0iNSIgZmlsbD0iI2Y4ZmFmYyIgc3Ryb2tlPSIjOTRhM2I4IiBzdHJva2Utd2lkdGg9IjEuNSIvPgo8dGV4dCB4PSIxOTYiIHk9IjI2MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzMzNDE1NSIgZm9udC1zaXplPSIxMSI+SXRlbSByZW1vdmVkIGZyb20gcmVnaXN0cnk8L3RleHQ+Cjx0ZXh0IHg9IjM4NiIgeT0iNDQiIGZpbGw9IiM2NDc0OGIiIGZvbnQtc2l6ZT0iMTEiIGZvbnQtd2VpZ2h0PSJib2xkIj5zZXRNYXNrKCk8L3RleHQ+CjxsaW5lIHgxPSIzNzQiIHkxPSI0NiIgeDI9IjM3NCIgeTI9IjE3OCIgc3Ryb2tlPSIjY2E4YTA0IiBzdHJva2Utd2lkdGg9IjIiLz4KPHJlY3QgeD0iMzg2IiB5PSI2MiIgd2lkdGg9IjI4NCIgaGVpZ2h0PSIzMCIgcng9IjUiIGZpbGw9IiNmZWY5YzMiIHN0cm9rZT0iI2NhOGEwNCIgc3Ryb2tlLXdpZHRoPSIxLjUiLz4KPHRleHQgeD0iNTI4IiB5PSI4MSIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzcxM2YxMiIgZm9udC1zaXplPSIxMSI+aXRlbS5tYXNrKCkgdXBkYXRlZCB0byBuZXcgdmFsdWU8L3RleHQ+CjxyZWN0IHg9IjM4NiIgeT0iMTEyIiB3aWR0aD0iMjg0IiBoZWlnaHQ9IjU2IiByeD0iNSIgZmlsbD0iI2ZlZjljMyIgc3Ryb2tlPSIjY2E4YTA0IiBzdHJva2Utd2lkdGg9IjIiLz4KPHRleHQgeD0iNTI4IiB5PSIxMzAiIHRleHQtYW5jaG9yPSJtaWRkbGUiIGZpbGw9IiM3MTNmMTIiIGZvbnQtc2l6ZT0iMTIiIGZvbnQtd2VpZ2h0PSJib2xkIj5vbk1hc2tDaGFuZ2VkIGZpcmVzPC90ZXh0Pgo8dGV4dCB4PSI1MjgiIHk9IjE0NyIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzcxM2YxMiIgZm9udC1zaXplPSIxMCI+aXRlbS5tYXNrKCkgPSBuZXc7IG9sZE1hc2sgYXJnID0gcHJldmlvdXM8L3RleHQ+Cjx0ZXh0IHg9IjUyOCIgeT0iMTYxIiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNzEzZjEyIiBmb250LXNpemU9IjEwIj5vbmx5IGZpcmVzIGlmIGl0ZW0gaXMgcHVibGlzaGVkPC90ZXh0Pgo8cmVjdCB4PSIzODYiIHk9IjE4NiIgd2lkdGg9IjI4NCIgaGVpZ2h0PSI0NCIgcng9IjUiIGZpbGw9IiNmOGZhZmMiIHN0cm9rZT0iIzk0YTNiOCIgc3Ryb2tlLXdpZHRoPSIxIi8+Cjx0ZXh0IHg9IjUyOCIgeT0iMjA0IiB0ZXh0LWFuY2hvcj0ibWlkZGxlIiBmaWxsPSIjNjQ3NDhiIiBmb250LXNpemU9IjExIj5zZXRNYXNrKCkgd2hpbGUgdW5wdWJsaXNoZWQ6PC90ZXh0Pgo8dGV4dCB4PSI1MjgiIHk9IjIyMCIgdGV4dC1hbmNob3I9Im1pZGRsZSIgZmlsbD0iIzY0NzQ4YiIgZm9udC1zaXplPSIxMSI+bWFzayB1cGRhdGVzIGxvY2FsbHksIG5vIHNpZ25hbCBmaXJlczwvdGV4dD4KPGRlZnM+CiAgPG1hcmtlciBpZD0iZGEiIG1hcmtlcldpZHRoPSI4IiBtYXJrZXJIZWlnaHQ9IjgiIHJlZlg9IjQiIHJlZlk9IjQiIG9yaWVudD0iYXV0byI+CiAgICA8cGF0aCBkPSJNMCwwIEwwLDggTDgsNCB6IiBmaWxsPSIjNjQ3NDhiIi8+CiAgPC9tYXJrZXI+CjwvZGVmcz4KPC9zdmc+)

### onPublished: Reacting to Arrivals

`onPublished` fires after the item is inserted into the registry. Inside the callback, `find` and `query` see the new item. This makes it safe to "chase" new arrivals: use the published item immediately from the callback.

```cpp
auto conn = system.onPublished([](SkeletonItem& item)
{
    using SC = SkeletonCapability;
    if ((item.mask() & makeMask(SC::Sensor, SC::Readable)) ==
         makeMask(SC::Sensor, SC::Readable))
    {
        std::cout << "New readable sensor arrived: " << item.name() << "\n";
    }
});
// conn is a ScopedConnection; the subscription is active until conn is destroyed
```

Reentrant `publish` calls from within an `onPublished` callback are permitted: Signal handles recursive emission, and `Skeleton::publish` only modifies the registry, not the signal slot list.

### onUnpublishing: Reacting to Departures

`onUnpublishing` fires while the item is still in the registry, before its removal. Inside the callback, `find` and `query` still see the item. Use this to perform cleanup — release references, cancel pending operations — before the item disappears.

```cpp
auto conn = system.onUnpublishing([](SkeletonItem& item)
{
    std::cout << "Item departing: " << item.name() << "\n";
    // item is still in the registry here -- find() returns it
});
```

Calling `unpublish` from within an `onUnpublishing` callback is not prohibited but is typically unnecessary; the item is already being unpublished.

### onMaskChanged: Reacting to Capability Changes

`onMaskChanged` fires after an item's mask is updated. The callback receives the item and the *previous* mask value. `item.mask()` already reflects the new value at callback time.

```cpp
auto conn = system.onMaskChanged([](SkeletonItem& item, SkeletonMask oldMask)
{
    using SC = SkeletonCapability;
    bool wasReadable = oldMask.test(static_cast<size_t>(SC::Readable));
    bool isReadable  = item.mask().test(static_cast<size_t>(SC::Readable));

    if (wasReadable && !isReadable)
    {
        std::cout << item.name() << " went offline\n";
    }
    else if (!wasReadable && isReadable)
    {
        std::cout << item.name() << " came online\n";
    }
});
```

### ScopedConnection Lifetimes

`onPublished`, `onUnpublishing`, and `onMaskChanged` return `ScopedConnection` values. A `ScopedConnection` owns the subscription; when it is destroyed, the subscription is automatically removed. Always store `ScopedConnection` objects in a member variable or container that matches the lifetime of the subscription you want.

```cpp
class MyWatcher
{
public:
    explicit MyWatcher(Skeleton& sk)
    {
        // Store the connection -- it stays alive as long as MyWatcher does
        mConn = sk.onPublished([this](SkeletonItem& item)
        {
            handleArrival(item);
        });
    }
    // When MyWatcher is destroyed, mConn is destroyed, auto-disconnecting the subscription

private:
    void handleArrival(SkeletonItem& item) { /* ... */ }
    ScopedConnection mConn;
};
```

Discarding the returned `ScopedConnection` immediately disconnects the subscription:

```cpp
// BUG: subscription disconnects immediately
system.onPublished([](SkeletonItem&) { /* never called */ });

// Correct: store the connection
auto conn = system.onPublished([](SkeletonItem&) { /* called */ });
```

### Reentrancy Rules

The reentrancy rules are enforced at runtime and terminate the process on violation:

- `publish` and `unpublish` are **forbidden during `visitSubtree`** traversal. The traversal holds an iterator over the registry; mutation would invalidate it.
- `unpublish` is **forbidden during `onPublished` emission**. An item that tries to remove itself before `publish` has returned would leave the registry in an inconsistent state.
- Reentrant `publish` from an `onPublished` callback is **permitted**. Signal handles recursive emission; Skeleton::publish modifies only the registry.

---

## Designing a Schema

### Choosing Level Granularity

A good schema reflects the natural partitioning of your domain. Each level should divide the previous level's space along a meaningful axis.

**Too flat:** One-level schemas lose the hierarchy advantage. If everything lives at depth 1, `isAncestorOf` never matches, and `visitSubtree` degenerates to a full scan.

**Too deep:** Deep schemas (5+ levels) are rare and should arise from genuine domain structure, not speculative future flexibility. Deep hierarchies mean deeper `Bone` template instantiations and longer `BoneId::toString()` output.

**Good pattern:** 2–4 levels covering natural boundaries: domain category → functional area → specific address.

### Enum Value Constraints

Each enum value packed into a BoneId occupies exactly 8 bits. Two hard constraints follow:

1. Values must be representable in one byte: unsigned range `0..255`, signed range `-128..127` (but values outside `0..255` will fail the `static_assert` in `Bone<>`).
2. The value `0` in any level position is valid but deserves care: a `BoneId` where *all* levels are zero is `BoneId::isNull()` — this is the null BoneId. A single level with value `0` in a non-null BoneId is fine.

Using `uint8_t` as the underlying type makes all constraints self-documenting:

```cpp
enum class Category : uint8_t { Sensors = 1, Actuators = 2, Network = 3 };
//                    ^^^^^^^^ -- explicitly one byte; compiler enforces the range
```

### Multiple Schemas in One Application

Different subsystems can have their own schemas. Schemas are fully independent: a `Bone<SchemaA, ...>` and a `Bone<SchemaB, ...>` are different types even if they have the same enum values. One `Skeleton` can hold items from different schemas as long as their `BoneId` values do not collide — which they won't in practice unless the raw packed values happen to match by coincidence. Use separate top-level enum values to partition the space.

```cpp
using SensorSchema    = HierarchySchema<SensorDomain, SensorNode>;
using ActuatorSchema  = HierarchySchema<ActuatorDomain, ActuatorNode>;
// Items from both schemas can safely coexist on one Skeleton,
// as long as their BoneId values do not coincide numerically.
```

---

## Dynamic Addressing: SkeletonUtilities

When a compile-time schema does not fit — plugin systems that enumerate addresses at runtime, data-driven hierarchies loaded from configuration, test fixtures that need thousands of unique items — use `index2BoneId` from `SkeletonUtilities.h`.

```cpp
#include "SkeletonUtilities.h"
using namespace fat_p::skeleton;

// Assign unique BoneIds to 10,000 plugin components under a fixed root
BoneId pluginRoot = BoneId{}.child(7);  // arbitrary root value

for (std::size_t i = 0; i < 10'000; ++i)
{
    BoneId id = index2BoneId(pluginRoot, i);
    // id is unique for each i; no id is an ancestor of another
}
```

The encoding is little-endian in the BoneId path: the least-significant byte of the index occupies the first appended level. Indices 0..255 consume one additional level; 256..65535 consume two. The maximum index before depth overflow depends on the prefix's remaining capacity (up to `8 - prefix.depth()` additional levels).

Partitioning two independent populations is straightforward: use different roots.

```cpp
BoneId sensorRoot   = BoneId{}.child(1);
BoneId actuatorRoot = BoneId{}.child(2);

BoneId sensor42   = index2BoneId(sensorRoot, 42);
BoneId actuator42 = index2BoneId(actuatorRoot, 42);
// sensor42 != actuator42 -- different roots
```

---

## BoneId in Depth

### Canonical Form

A BoneId is in canonical form when all inactive level bytes (indices ≥ depth) are zero. All factory sites — `Bone<>::id()`, `BoneId::child()`, `BoneId::parent()`, `BoneId::deserialize()`, `detail::buildBoneId()` — produce canonical form. The private-tag constructor prevents external code from constructing non-canonical BoneIds, which would break equality, hashing, and duplicate detection in `Skeleton::publish`.

### Ancestor Queries

`a.isAncestorOf(b)` returns true when `a`'s path is a strict prefix of `b`'s path. A BoneId is not its own ancestor. This mirrors the file-system path intuition: `/sensors` is an ancestor of `/sensors/load`, but not of `/sensors` itself.

The implementation is a bitmask comparison on the 64-bit value. No loop, no recursion: O(1).

```cpp
constexpr BoneId sensors =
    Bone<AppSchema, Domain::System, Category::Sensors>::id();   // depth 2
constexpr BoneId loadId  =
    Bone<AppSchema, Domain::System, Category::Sensors, Node::Load>::id();  // depth 3

static_assert(sensors.isAncestorOf(loadId));        // true
static_assert(!loadId.isAncestorOf(sensors));       // false -- child not ancestor of parent
static_assert(!sensors.isAncestorOf(sensors));      // false -- not its own ancestor
```

### Serialization

`BoneId::serialize(span<std::byte, 9>)` writes the canonical 9-byte big-endian form: 8 bytes of packed path value, then 1 byte of depth. `BoneId::deserialize(span<const std::byte, 9>)` reconstructs from that form, masking inactive bytes to zero to guarantee canonical form on input. A depth byte greater than 8 is silently returned as the null BoneId.

```cpp
std::array<std::byte, 9> buf{};
loadId.serialize(buf);  // write 9 bytes

BoneId recovered = BoneId::deserialize(buf);
assert(recovered == loadId);
```

---

## SkeletonItem Without BoneItem

`SkeletonItem` is the non-owning base for all items. `BoneItem` is a convenience wrapper that pre-computes the BoneId from the Bone type. In unusual cases — items that do not have a fixed schema, or items whose BoneId is computed at runtime — you can derive directly from `SkeletonItem`.

```cpp
class DynamicItem : public SkeletonItem
{
public:
    explicit DynamicItem(Skeleton& sk, BoneId id, SkeletonMask mask)
        : SkeletonItem(id, mask, "dynamic_item")
    {
        publish(sk);  // publish is public on SkeletonItem
    }

    ~DynamicItem() override
    {
        unpublish();
    }
};
```

Note that `publish` and `unpublish` are `public` on `SkeletonItem` but `protected` on `BoneItem`. When deriving directly from `SkeletonItem`, external callers can also call `publish` and `unpublish`. This is intentional for the manual-BoneId use case but means you must rely on convention rather than access control to enforce the lifecycle contract.

---

## When to Use / When Not To

**Use Skeleton when:**
- Components have separate construction and destruction lifetimes
- Consumers need to discover producers without knowing them at compile time
- A hierarchy of addresses reflects a natural domain structure
- Reactive notification of arrival, departure, or capability change is needed
- Multiple independent consumers want to find the same set of items

**Do not use Skeleton when:**
- The component graph is entirely static (use direct composition — it is simpler and zero-overhead)
- Thread safety is required (Skeleton is single-threaded; all operations must run on the same thread or under external synchronization)
- The hierarchy has more than 8 levels (BoneId physical limit)
- You need fast capability queries on thousands of items in a tight loop (query is O(N log N); consider a separate sorted index)
- You need polymorphic storage or archetype-based iteration (use an ECS framework)

---

## Troubleshooting

### Compilation Errors

**"Each Bone level enum value must fit in one byte (0..255)"**

A Bone instantiation references an enum value whose underlying representation exceeds 255. Check the enum's underlying type. Use `uint8_t` or constrain enum values to `0..255`.

**"All HierarchySchema level types must be enums"**

A type passed to `HierarchySchema` is not an enum type. Check that all level type parameters are `enum` or `enum class`.

**"HierarchySchema kMaxDepth cannot exceed 8"**

The schema has more than 8 level types. BoneId stores at most 8 levels. Redesign the schema to stay within this limit.

**Implicit conversion error on Bone<> instantiation**

You passed an enum value of the wrong type at some depth position. The schema's `expected_type<N>` at depth N must match the type of the N-th enum value. Check the order and types of your level arguments against the schema.

### Runtime Terminations

**"FATAL: SkeletonItem ... destroyed while still published"**

The most-derived destructor did not call `unpublish()` before the object was destroyed. Add `this->unpublish()` as the first statement in the most-derived destructor.

**"FATAL: Skeleton ... destroyed with N item(s) still published"**

The Skeleton was destroyed before some items unpublished. Either the Skeleton outlived the items (correct order) but those items forgot to call `unpublish()`, or the items outlived the Skeleton (incorrect order — the Skeleton must be destroyed last). Check object lifetime ordering in the enclosing scope.

**"FATAL: publish() called during visitSubtree traversal"**

A `visitSubtree` callback published a new item. Publishing is forbidden during traversal. Defer the publish or restructure to avoid it.

**"FATAL: unpublish() called during mOnPublished emission"**

An `onPublished` callback tried to unpublish an item before `publish()` returned. Unpublish is forbidden during `onPublished` emission. Defer the unpublish.

### Logic Errors

**`find` returns nullptr for a known-published item**

Check that the BoneId passed to `find` was produced by `Bone<Schema, Levels...>::id()` with the same schema and same level values used at publication. Schemas are separate types: a `Bone<SchemaA, ...>` and `Bone<SchemaB, ...>` produce different BoneIds even if the enum values are numerically equal. Also check that the item's constructor has returned without throwing before you call `find`.

**`query` returns empty for a published item**

Check that the item's mask has all the bits in `required` set and no bits in `excluded` set. Use `item.mask()` to inspect the actual mask. An item with an empty mask matches only `query({}, {})`.

**ScopedConnection auto-disconnects unexpectedly**

The `ScopedConnection` was stored as a local variable that went out of scope, or it was stored in a temporary and not moved into a member. Assign `ScopedConnection` to a member variable with the intended lifetime.

---

## API Reference

### BoneId (SkeletonFwd.h)

| Member | Description |
|--------|-------------|
| `BoneId()` | Null BoneId (depth 0, value 0) |
| `value() → uint64_t` | Raw packed path value |
| `depth() → uint8_t` | Number of active levels (0..8) |
| `isNull() → bool` | True if depth == 0 |
| `isAncestorOf(BoneId) → bool` | True if this is a strict prefix of other's path |
| `isDescendantOf(BoneId) → bool` | True if other is a strict prefix of this path |
| `parent() → BoneId` | One fewer level. UB if depth == 0; asserts in debug |
| `child(uint8_t) → BoneId` | One more level. UB if depth >= 8; asserts in debug |
| `serialize(span<byte,9>)` | Write 9-byte big-endian form |
| `deserialize(span<const byte,9>) → BoneId` | Read 9-byte big-endian form; null on depth > 8 |
| `toString() → std::string` | Human-readable `[l0:l1:...:lN]` form |
| `operator==`, `operator<=>` | Equality and ordering (constexpr) |
| `std::hash<BoneId>` | `<functional>` specialization |

### HierarchySchema (SkeletonFwd.h)

| Member | Description |
|--------|-------------|
| `expected_type<N>` | Enum type at depth N |
| `kMaxDepth` | Number of depth levels |

### Bone\<Schema, Levels...\> (Skeleton.h)

| Member | Description |
|--------|-------------|
| `id() → BoneId` | Compile-time BoneId for this path |
| `kDepth` | Number of levels |
| `Parent` | Parent Bone type (void if depth == 0) |
| `child<ChildLevel>` | Child Bone type alias |
| `schema_type` | The HierarchySchema instantiation |

### SkeletonCapability / SkeletonMask / makeMask (SkeletonFwd.h), CapabilityRegistry (CapabilityRegistry.h)

| Item | Description |
|------|-------------|
| `SkeletonCapability` | The pre-registered framework band (indices 0–31): Sensor, Controller, Display, Network, Storage, ProvidesValue, ProvidesCommand, ProvidesStatus, ValueBinary, ValueContinuous, ValueDiscrete, ConsumesValue, ConsumesCommand, ConsumesStatus, Readable, Writable, Serializable, NetworkVisible |
| `kFrameworkCapabilityBand` | 32 — the framework band size / first application capability index |
| `SkeletonMask` | Unbounded capability bitset (normalized word vector). `set(i)/reset(i)/test(i)`, `none/any/count`, `&`, `\|`, `&=`, `\|=`, `==`, `toString()`. No width-presuming bitset APIs (no-arg `set`, `all`, `size`, `to_ulong`) |
| `makeMask(caps...) → SkeletonMask` | Build a mask from framework capabilities and/or registered indices, mixed. Terminates if `SkeletonCapability::Count` is passed |
| `CapabilityRegistry::instance()` | Singleton name → capability-index registry |
| `registerCapability(name) → size_t` | Register (idempotent by name); allocates application indices from 32 upward. Call at init only |
| `find(name)` / `name(index)` / `highWater()` | Lookup by name; reverse lookup; one past the highest allocated index |

### SkeletonItem (Skeleton.h)

| Member | Description |
|--------|-------------|
| `boneId() → BoneId` | Item's hierarchical address |
| `mask() → const SkeletonMask&` | Current capability mask |
| `name() → std::string_view` | Diagnostic name |
| `isPublished() → bool` | True if currently on a Skeleton |
| `publish(Skeleton&)` | Register. Throws `PublicationError` / `DuplicateBoneError` |
| `unpublish() noexcept` | Remove from registry. No-op if not published |
| `setMask(SkeletonMask)` (protected) | Replace mask; fires `onMaskChanged` if published |
| `~SkeletonItem()` | Terminates if still published |

### BoneItem\<Schema, Levels...\> (Skeleton.h)

Alias for `BasicBoneItem<Schema, DefaultMaskPolicy, Levels...>`. Inherits from `SkeletonItem`. Constructor takes `(SkeletonMask mask, std::string name = {})`. `publish`, `unpublish`, `setMask` are protected.

### Skeleton (Skeleton.h)

| Member | Description |
|--------|-------------|
| `Skeleton(string name = "default")` | Construct empty registry |
| `~Skeleton()` | Terminates if any items still published |
| `name() → string_view` | Diagnostic name |
| `find(BoneId) → SkeletonItem*` | O(1). nullptr if not found |
| `findAs<T>(BoneId) → T*` | O(1). static_cast. nullptr if not found |
| `visitSubtree(BoneId, fn)` | O(N log N). fn receives items in ascending BoneId order |
| `query(required, excluded) → vector<SkeletonItem*>` | O(N log N). Both masks default to `{}` |
| `querySubtree(BoneId, required, excluded) → vector<SkeletonItem*>` | O(N log N). Restricted to subtree |
| `onPublished(fn, priority=0) → ScopedConnection` | Subscribe to item-published events |
| `onUnpublishing(fn, priority=0) → ScopedConnection` | Subscribe to item-departing events |
| `onMaskChanged(fn, priority=0) → ScopedConnection` | Subscribe to mask-change events |
| `size() → size_t` | Number of published items |
| `dump(ostream&)` | Print registry contents in BoneId order |

### index2BoneId (SkeletonUtilities.h)

```cpp
[[nodiscard]] BoneId index2BoneId(BoneId prefix, std::size_t index) noexcept;
```

Maps a flat zero-based index to a unique BoneId descended from `prefix`. O(depth consumed), at most 8 iterations. Stateless; safe to call concurrently on distinct threads. Terminates if the encoded depth would exceed 8 (via `BoneId::child`).

---

## FAQ

**Can one item be published on multiple Skeletons simultaneously?**

No. `SkeletonItem` stores a single back-pointer to its Skeleton. `publish` throws `PublicationError` if called when already published. To publish on a second Skeleton, call `unpublish()` first.

**Can I change the BoneId of an item after construction?**

No. `BoneItem` and `SkeletonItem` bind the BoneId at construction time. The BoneId is fixed for the lifetime of the item. If you need a different address, unpublish, destroy, and create a new item.

**Can I publish from a base class constructor before the most-derived class is fully constructed?**

You must not. The lifecycle contract requires that all members be initialized before `publish` is called. If a base constructor publishes prematurely and the derived-class constructor throws, the item will be destroyed (triggering the "destroyed while still published" terminate) before the derived destructor can call `unpublish`. Always publish from the most-derived constructor.

**Is BoneId serialization suitable for cross-process or network communication?**

`BoneId::serialize` produces a 9-byte, endian-stable, canonical representation. Two processes with the same `HierarchySchema` and the same enum values will produce identical BoneIds for the same addresses. The format is stable as long as the schema does not change. There is no versioning in the serialized form.

**Can `query` return different results between two calls with no intervening publish/unpublish?**

No — with one exception: if an `onMaskChanged` callback fires between the two calls (triggered by some item calling `setMask`), the mask predicate can match or unmatch items. Query results are a deterministic snapshot of the registry state at the moment of the call.

**What is the maximum number of items a Skeleton can hold?**

There is no enforced limit beyond available memory. `FastHashMap` will resize as items are added. Query and traversal performance degrades at large N (O(N log N) due to sort).

---

*Skeleton.h / SkeletonFwd.h / SkeletonUtilities.h — Fat-P Library*
