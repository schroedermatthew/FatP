---
doc_id: OV-VIEWLIFETIMETRACKING-001
doc_type: "Overview"
title: "ViewLifetimeTracking"
fatp_components: ["ViewLifetimeTracking"]
topics: ["lifetime tracking", "dangling reference", "view safety", "debug-only", "LifetimeTracker", "TrackedView", "LifetimeToken", "weak_ptr safety"]
cxx_standard: "C++20"
last_verified: "2026-02-15"
audience: ["C++ developers", "AI assistants"]
status: "draft"
---

# Overview - ViewLifetimeTracking

*February 2026*

---

## What It Does

ViewLifetimeTracking detects dangling references in debug builds. When a non-owning view (a span, a Tensor slice, a string_view-like reference) outlives the object it refers to, accessing the view is undefined behavior. ViewLifetimeTracking catches this at runtime by associating views with a shared `LifetimeToken` that is invalidated when the source object is destroyed. Accessing an invalidated view throws `DanglingReferenceError` with a clear message instead of silently reading freed memory.

## Why It Exists

Non-owning views are essential for performance---they avoid copies. But they introduce a class of bugs that is invisible to the compiler and often undetectable by sanitizers until the freed memory is overwritten. ViewLifetimeTracking provides deterministic detection at the cost of one `shared_ptr` per tracked object, with zero overhead in release builds (the entire mechanism compiles away when `NDEBUG` is defined).

## Key Concepts

`LifetimeTracker<T>` wraps a source object and creates a shared `LifetimeToken`. `TrackedView` holds a weak reference to the token. On access (`operator*`, `operator->`), `TrackedView` checks the token; if invalidated, it throws `DanglingReferenceError`. The `FATP_TRACKED_VIEW(obj)` macro creates a tracker, and `FATP_VIEW_GUARD(view)` asserts validity. In release builds, all macros and classes become no-ops.

## Architecture at a Glance

Single header (`ViewLifetimeTracking.h`) in namespace `fat_p`. Types: `LifetimeToken` (shared validity flag), `LifetimeTracker<T>` (source-side RAII), `TrackedView` (view-side checked access), `ViewGuard` (scope-based validity assertion), `DanglingReferenceError` (exception). Also provides `check_weak_ptr()` for `std::weak_ptr` safety.

---

*ViewLifetimeTracking.h --- Fat-P Library*
