# TMP Project - DeviceData: Host↔Device Lifecycle for Kokkos

## The Problem This Solves

The most common Kokkos workflow is a six-step ceremony:

```cpp
// Step 1: Create host data
double matrix[64][64];
fill_matrix(matrix);

// Step 2: Wrap in an unmanaged host view (ArrayView does this)
auto hostView = ArrayView(matrix).to_kokkos();

// Step 3: Create a MATCHING device view (get the DataType right)
Kokkos::View<double[64][64], Kokkos::CudaSpace> deviceView("matrix");

// Step 4: Copy host → device
Kokkos::deep_copy(deviceView, hostView);

// Step 5: Run kernel
Kokkos::parallel_for(64, KOKKOS_LAMBDA(int i) {
    for (int j = 0; j < 64; ++j)
        deviceView(i, j) *= 2.0;
});

// Step 6: Copy device → host
Kokkos::deep_copy(hostView, deviceView);
```

Steps 2–4 and 6 are pure boilerplate. The DataType must match between host and device views. The deep_copy calls must be in the right order. The device view must be constructed with the right memory space. Every project repeats this pattern hundreds of times, and getting any piece wrong is a compile error at best, silent data corruption at worst.

This project builds a `DeviceData` wrapper that reduces the ceremony to:

```cpp
double matrix[64][64];
fill_matrix(matrix);

auto device = make_device_data(matrix, "matrix"); // Steps 2-4 in one line
run_kernel(device.view());                    // Step 5
device.copy_back();                           // Step 6 — matrix is updated
```

## What You Will Build

A `DeviceData<ArrayType>` that:

1. Deduces the array dimensions from a C array (reuses Peeler)
2. Computes the Kokkos DataType at compile time (reuses KokkosDataType)
3. Constructs a device-resident owning `Kokkos::View` with the matching type
4. Deep-copies host data to device on construction
5. Provides `copy_back()` to transfer results back to the host array
6. Cleans up device memory on destruction (RAII)

All type computation happens at compile time. No runtime type introspection, no string-based lookups, no manual DataType specification.

## Prerequisites

- Completed the ArrayView TMP exercises (understand Peeler and KokkosDataType)
- Basic understanding of Kokkos views and deep_copy
- Familiar with RAII (constructors/destructors managing resources)

---

## Phase 1: DeviceViewType — Computing the Device View Type from a C Array

### The Problem

Given `double matrix[64][64]`, compute the type `Kokkos::View<double[64][64], Layout, DeviceSpace>` at compile time.

### What Is the Compiler Figuring Out?

A **type** — the fully-specified `Kokkos::View` type for device memory. This requires chaining two existing traits: Peeler (to extract dimensions from the array type) and KokkosDataType (to encode those dimensions as a Kokkos DataType).

### Pattern

Type composition — no new recursion, just chaining existing traits. The output of Peeler (an `ArrayView<T, Dims...>`) feeds into KokkosDataType (which produces the Kokkos `DataType` encoding). We add the Layout and Space template parameters to produce the final `View` type.

### Why Not Just Write the View Type Directly?

For `double[64][64]`, writing `Kokkos::View<double[64][64]>` is manageable. For `ArrayView<double, dynamic_extent, dynamic_extent, 5>`, the correct DataType is `double**[5]` — and getting this wrong is a compile error with a wall of template instantiation backtraces. The trait computes it mechanically and correctly.

### Solution

```cpp
#pragma once

#include "ArrayView.h"  // For Peeler, KokkosDataType, ArrayView
#include <Kokkos_Core.hpp>

namespace bridge {

// Given an ArrayView type, compute the Kokkos::View type for a given Space.
template<typename AView,
         typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space>
struct DeviceViewType;

template<typename T, std::size_t... Dims, typename Layout, typename Space>
struct DeviceViewType<ulib::ArrayView<T, Dims...>, Layout, Space>
{
    // Remove const from element type — device views own their data
    using value_type = std::remove_cv_t<T>;

    // Reuse ArrayView's KokkosDataType to get the DataType encoding
    using data_type = ulib::detail::KokkosDataType_t<value_type, Dims...>;

    // The owning device view type
    using type = Kokkos::View<data_type, Layout, Space>;

    // The unmanaged host view type (for deep_copy source/destination)
    using host_mirror = Kokkos::View<
        ulib::detail::KokkosDataType_t<T, Dims...>,
        Layout,
        Kokkos::HostSpace,
        Kokkos::MemoryTraits<Kokkos::Unmanaged>>;
};

} // namespace bridge

// Tests (compile-time only — verify types match)
using AV = ulib::ArrayView<double, 64, 64>;
using DVT = bridge::DeviceViewType<AV>;

static_assert(std::is_same_v<
    DVT::data_type,
    double[64][64]
>);

// For a mixed-extent view:
using AV2 = ulib::ArrayView<double, ulib::dynamic_extent, 3>;
using DVT2 = bridge::DeviceViewType<AV2>;

static_assert(std::is_same_v<
    DVT2::data_type,
    double*[3]
>);
```

### What to Notice

No new TMP recursion is introduced. The entire type computation reuses `KokkosDataType_t` from ArrayView. `DeviceViewType` is a *composition* trait — it plugs the output of one trait (KokkosDataType) into the template parameters of another type (Kokkos::View). This is the TMP equivalent of function composition: `f(g(x))` where `g` = KokkosDataType and `f` = Kokkos::View construction.

The `host_mirror` type keeps `const` on the element type (if the source array was `const`) and adds `Unmanaged` (because it wraps existing host memory, not a Kokkos allocation). The `type` (device view) strips `const` because the device copy is an independent allocation that the kernel will write to.

---

## Phase 2: DeviceData — The RAII Wrapper

### The Problem

Build the class that manages the lifecycle: allocate device memory, copy host→device on construction, provide access to the device view, copy device→host on request.

### What Is the Compiler Figuring Out?

The **class template parameters and member types**. From a C array type like `double[64][64]`, the class must deduce the ArrayView type (via Peeler), compute the device and host view types (via Phase 1), and store the right members.

### Pattern

CTAD + type composition. The constructor accepts a C array reference, Peeler deduces the ArrayView type, and DeviceViewType computes the Kokkos types. The class stores a device view and a pointer back to the host data (for copy_back).

### Solution

The class must be specialized on `ArrayView<T, Dims...>` so that it can access the dimension pack and forward dynamic extents to the Kokkos::View constructor. The label-only constructor `deviceView_(label)` works for all-static shapes — Kokkos reads the dimensions from the DataType template parameter. But for dynamic shapes, the Kokkos::View constructor needs the runtime extents as arguments. The host ArrayView already stores these extents in its `ExtentStorage`, and its `to_kokkos()` method already forwards them via `apply`. The device view needs the same forwarding.

The fix uses a helper method that constructs the device view with the label and the dynamic extents extracted from the host view:

```cpp
namespace bridge {

template<typename ArrayViewT,
         typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space>
class DeviceData;

// Partial specialization: extract T and Dims... from the ArrayView type
template<typename T, std::size_t... Dims, typename Layout, typename Space>
class DeviceData<ulib::ArrayView<T, Dims...>, Layout, Space>
{
    static_assert(!std::is_const_v<T>,
        "DeviceData requires mutable host data (for copy_back). "
        "Use DeviceInput for const/read-only host data.");

    using ArrayViewT = ulib::ArrayView<T, Dims...>;
    using Types = DeviceViewType<ArrayViewT, Layout, Space>;

public:
    using device_view_type = typename Types::type;
    using host_view_type   = typename Types::host_mirror;
    using element_type     = T;

    // Construct from an ArrayView — allocate device memory + copy
    explicit DeviceData(ArrayViewT hostView, const std::string& label = "DeviceData")
        : hostView_(hostView)
        , deviceView_(createDeviceView(hostView, label))
    {
        auto kokkosHost = hostView_.to_kokkos();
        Kokkos::deep_copy(deviceView_, kokkosHost);
    }

    // Access the device view (for use in kernels)
    device_view_type& view() noexcept { return deviceView_; }
    const device_view_type& view() const noexcept { return deviceView_; }

    // Copy device data back to the original host array
    void copy_back()
    {
        auto kokkosHost = hostView_.to_kokkos();
        Kokkos::deep_copy(kokkosHost, deviceView_);
    }

    // Copy fresh host data to the device (re-upload)
    void copy_to_device()
    {
        auto kokkosHost = hostView_.to_kokkos();
        Kokkos::deep_copy(deviceView_, kokkosHost);
    }

private:
    ArrayViewT hostView_;             // Non-owning view of original host data
    device_view_type deviceView_;     // Owning device allocation

    // Construct the device view with the label and dynamic extents.
    // For all-static shapes, no dynamic extents are forwarded and the
    // Kokkos::View constructor receives only the label.
    // For mixed/dynamic shapes, the runtime extents from the host view
    // are forwarded via the same index_sequence expansion that
    // ArrayView::to_kokkos() uses internally.
    //
    // IMPORTANT: This relies on ArrayView's dynamic-before-static ordering
    // invariant. extent(0)...extent(rank_dynamic-1) returns the dynamic
    // extents because ArrayView guarantees that all dynamic_extent
    // dimensions precede all static dimensions in the pack.
    static device_view_type createDeviceView(
        ArrayViewT hostView, const std::string& label)
    {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return device_view_type(label, hostView.extent(Is)...);
        }(std::make_index_sequence<ulib::detail::rank_dynamic_v<Dims...>>{});
    }
};

} // namespace bridge
```

### What to Notice

`DeviceData` stores the `ArrayView` (not a copy of the data — the view is non-owning). This means the original host array must outlive the `DeviceData` object. `copy_back()` writes directly into the original host array through the stored view.

> **⚠️ Lifetime hazard.** `DeviceData` stores a non-owning view of the host buffer. If the host buffer is destroyed before `copy_back()` is called — because it was a temporary, a short-lived stack object, or a borrowed buffer whose owner deallocated it — the `copy_back()` writes through a dangling pointer. This is silent data corruption, not a crash. The same hazard applies to `DeviceInOut`. `DeviceInput` is safer in this regard: it copies host data into device memory on construction and does not retain a host reference, so the host buffer can go out of scope after construction without consequence.

The `static_assert(!std::is_const_v<T>)` enforces a contract: `DeviceData` is for mutable host data only, because `copy_back()` must be able to write through the stored view. Const host data should go through `DeviceInput` (Phase 6), which has no `copy_back()` and presents a const device view. This mutability split aligns the type system with the data-flow direction: writable host buffer → bidirectional staging (`DeviceData`), read-only host buffer → one-way staging (`DeviceInput`).

The `createDeviceView` helper uses the same `index_sequence` expansion pattern as `ExtentStorage::apply`: it generates an index sequence of length `rank_dynamic_v<Dims...>` and expands `hostView.extent(Is)...` to forward only the dynamic extents. For all-static views, the index sequence is empty and the Kokkos::View constructor receives only the label. For `ArrayView<double, dynamic_extent, 3>` with 5 rows, the expansion produces `device_view_type(label, 5)` — forwarding the one runtime extent.

This is why the class must be specialized on `ArrayView<T, Dims...>`: the `createDeviceView` helper needs the `Dims...` pack to compute `rank_dynamic_v` and to know which extents are dynamic. Without the partial specialization, the pack is trapped inside the `ArrayViewT` type and cannot be accessed.

---

## Phase 3: Factory — One-Line Construction from C Arrays

### The Problem

The user should be able to write `make_device_data(matrix, "grid")` where `matrix` is a C array. The factory must deduce the ArrayView type from the array type, then construct the `DeviceData` with the correct template parameters.

### Pattern

A factory function that calls `makeArrayView` (which uses Peeler internally) to produce the ArrayView, then forwards it to `DeviceData`. A CTAD guide on `DeviceData` itself would also work, but chaining Peeler into a deduction guide is verbose — the factory is clearer and gives us a natural place for the label parameter and the mutability constraint.

### Solution

```cpp
namespace bridge {

// Factory: C array → DeviceData (mutable arrays only — use make_device_input for const)
template<typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space,
         typename Array>
    requires (std::is_array_v<Array>
              && !std::is_const_v<std::remove_all_extents_t<Array>>)
auto make_device_data(Array& arr, const std::string& label = "DeviceData")
{
    auto view = ulib::makeArrayView(arr);
    using ViewType = decltype(view);
    return DeviceData<ViewType, Layout, Space>(view, label);
}

// Factory: ArrayView → DeviceData (mutable element type only)
template<typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space,
         typename T, std::size_t... Dims>
    requires (!std::is_const_v<T>)
auto make_device_data(ulib::ArrayView<T, Dims...> view,
                      const std::string& label = "DeviceData")
{
    return DeviceData<ulib::ArrayView<T, Dims...>, Layout, Space>(view, label);
}

} // namespace bridge
```

### Usage — The Full Workflow in Three Lines

```cpp
void compute()
{
    // Host data — a plain C array
    double grid[128][128];
    initialize_grid(grid);

    // One line: wrap, allocate device, copy host→device
    auto device = bridge::make_device_data(grid, "grid");

    // Run kernel on device data
    auto dv = device.view();
    Kokkos::parallel_for("scale", 128, KOKKOS_LAMBDA(int i) {
        for (int j = 0; j < 128; ++j)
            dv(i, j) *= 2.0;
    });
    Kokkos::fence();

    // Copy results back to the original C array
    device.copy_back();
    // grid[0][0] is now doubled
}
```

Compare with the six-step ceremony from the introduction. Steps 2, 3, 4, and 6 are gone. The type computation (DataType, Layout, Space) is invisible. The user names the C array and gets a device-resident copy with full type safety.

### What to Notice

`make_device_data` calls `makeArrayView` (which uses Peeler internally) to produce the ArrayView, then constructs `DeviceData` with the deduced ArrayView type. The type chain is: C array type → Peeler → ArrayView type → KokkosDataType → Kokkos DataType → Kokkos::View type. Four TMP traits, chained at compile time, producing the correct device view type with zero runtime overhead.

The factory has two overloads: one for C arrays (calls `makeArrayView`), one for pre-existing ArrayViews. This lets users who already have an ArrayView skip the Peeler step.

---

## Phase 4: Dynamic Extents — Runtime-Sized Data

### The Problem

Not all data is statically sized. A simulation might have a grid whose dimensions are read from a config file:

```cpp
size_t nx = config.get("nx");
size_t ny = config.get("ny");
double* buffer = allocate_grid(nx, ny);

// How do we make DeviceData from this?
```

### Solution

The user constructs an ArrayView with dynamic extents, then passes it to `make_device_data`:

```cpp
ulib::ArrayView<double, ulib::dynamic_extent, ulib::dynamic_extent> grid(buffer, nx, ny);
auto device = bridge::make_device_data(grid, "grid");
```

This works because the `createDeviceView` helper in Phase 2 already handles the dynamic case. It uses `index_sequence` expansion over `rank_dynamic_v<Dims...>` to forward the runtime extents from the host ArrayView into the Kokkos::View constructor. For `ArrayView<double, dynamic_extent, dynamic_extent>` with extents `(nx, ny)`, the expansion produces `device_view_type(label, nx, ny)`. For all-static views, the expansion is empty and the constructor receives only the label.

No new TMP pattern was needed — the `createDeviceView` helper uses the same `index_sequence` expansion that ArrayView's `to_kokkos()` already uses internally. The machinery existed; Phase 2 wired it into the device view constructor.

### What to Notice

The all-static and all-dynamic cases use the same `DeviceData` class, the same factory, and the same copy methods. The type system handles both: static extents are encoded in the DataType template parameter, dynamic extents are forwarded at runtime by `createDeviceView`. The user does not choose a "static mode" or "dynamic mode" — the template machinery selects the right behavior based on the ArrayView type.

This is the payoff of the mixed-extent design in ArrayView. If ArrayView only supported all-static extents (like CArrayView), `DeviceData` would need a separate code path for runtime-sized data. The mixed-extent machinery eliminates that bifurcation.

---

## Phase 5: The Shape Mismatch Problem

### What Is Wrong With DeviceData

`DeviceData` assumes in-place computation: the output has the same shape as the input, and results are copied back to the same buffer. This covers element-wise operations (scaling, normalization, thresholding) but fails for anything where the output shape differs from the input:

- Matrix multiply: `[M×K]` × `[K×N]` → `[M×N]`
- Reductions: `[N]` → scalar, or `[M×N]` → `[M]`
- Histograms: `[N]` samples → `[num_bins]` counts
- Convolutions: `[H×W]` input → `[(H-K+1)×(W-K+1)]` output
- Reshaping: `[M×N]` → `[N×M]` (same data, different view)

The problem is that `DeviceData` conflates three roles: staging input data, providing device workspace, and staging output data. When input and output have different shapes, these roles must be separated.

### The Fix: Three Separate Wrappers

| Wrapper | Role | Owns device memory? | Copies host→device? | Copies device→host? | Host buffer must outlive? |
|---------|------|--------------------|--------------------|--------------------|-----------------------------|
| `DeviceInput` | Read-only input staging | Yes | On construction | No | No |
| `DeviceOutput` | Write-only output staging | Yes | No | On request | Destination provided at copy time |
| `DeviceInOut` | In-place read-write | Yes | On construction | On request | Yes (same buffer for both) |

`DeviceInOut` is the renamed `DeviceData` from Phases 1–4. `DeviceInput` and `DeviceOutput` are the new pieces.

---

## Phase 6: DeviceInput — Read-Only Input Staging

### The Problem

Kernel input data lives on the host as a C array. It needs to be on the device for the kernel to read. After the kernel finishes, the input is no longer needed — there is nothing to copy back.

### What Is the Compiler Figuring Out?

The same types as Phase 1 (DeviceViewType), but the device view is const-qualified: the kernel should not be able to modify input data. The type computation must propagate `const` through the DataType encoding.

### Why Const Matters

Without const, a kernel could accidentally write to the input view and the compiler would not catch it. By making the device view's element type `const`, any write attempt is a compile error inside the `KOKKOS_LAMBDA`. This is the same principle as `ArrayView<const T, ...>` — constness of the data is encoded in the type, not in the wrapper.

### Solution

The implementation needs access to the `Dims...` pack from the ArrayView type — both for `KokkosDataType_t` (which builds the DataType encoding) and for `createDeviceView` (which needs `rank_dynamic_v<Dims...>` to forward dynamic extents). The pack is trapped inside the `ArrayViewT` type parameter and cannot be accessed from the primary template. The fix is the same as `DeviceData` in Phase 2: declare a primary template as a forward declaration, then provide a partial specialization that pattern-matches `ArrayView<T, Dims...>` to extract the pack.

```cpp
namespace bridge {

// Forward declaration — the real implementation is the partial specialization below
template<typename ArrayViewT,
         typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space>
class DeviceInput;

// Partial specialization that extracts Dims... from the ArrayView type
template<typename T, std::size_t... Dims, typename Layout, typename Space>
class DeviceInput<ulib::ArrayView<T, Dims...>, Layout, Space>
{
    using value_type = std::remove_cv_t<T>;
    using const_data_type = ulib::detail::KokkosDataType_t<const value_type, Dims...>;
    using mutable_data_type = ulib::detail::KokkosDataType_t<value_type, Dims...>;

    // Device view has const elements — kernel cannot write
    using device_view_type = Kokkos::View<const_data_type, Layout, Space>;

    // Internal mutable view for the initial deep_copy
    // (Kokkos requires mutable destination for deep_copy)
    using mutable_device_type = Kokkos::View<mutable_data_type, Layout, Space>;

public:
    explicit DeviceInput(ulib::ArrayView<T, Dims...> hostView,
                         const std::string& label = "DeviceInput")
        : deviceView_(createDeviceView(hostView, label))
    {
        auto kokkosHost = hostView.to_kokkos();
        Kokkos::deep_copy(deviceView_, kokkosHost);
    }

    // Read-only access for kernels
    device_view_type view() const noexcept
    {
        // Implicit conversion from mutable to const view
        return deviceView_;
    }

    // No copy_back — input data flows one direction only

private:
    mutable_device_type deviceView_;

    // Same pattern as DeviceData::createDeviceView — forward dynamic extents
    static mutable_device_type createDeviceView(
        ulib::ArrayView<T, Dims...> hostView, const std::string& label)
    {
        return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return mutable_device_type(label, hostView.extent(Is)...);
        }(std::make_index_sequence<ulib::detail::rank_dynamic_v<Dims...>>{});
    }
};

// Factory
template<typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space,
         typename Array>
    requires std::is_array_v<Array>
auto make_device_input(Array& arr, const std::string& label = "DeviceInput")
{
    auto view = ulib::makeArrayView(arr);
    return DeviceInput<decltype(view), Layout, Space>(view, label);
}

template<typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space,
         typename T, std::size_t... Dims>
auto make_device_input(ulib::ArrayView<T, Dims...> view,
                       const std::string& label = "DeviceInput")
{
    return DeviceInput<ulib::ArrayView<T, Dims...>, Layout, Space>(view, label);
}

} // namespace bridge
```

### What to Notice

The device view stores mutable data (Kokkos requires mutable views as deep_copy destinations), but `view()` returns a `const`-element view. The implicit conversion from `View<double[64][64]>` to `View<const double[64][64]>` is built into Kokkos — the same mutable-to-const promotion that ArrayView supports.

The `createDeviceView` helper uses the same `index_sequence` expansion as `DeviceData::createDeviceView` — forwarding the dynamic extents from the host ArrayView to the device view constructor. This ensures `DeviceInput` works for dynamic shapes, not just all-static ones.

The partial specialization `DeviceInput<ulib::ArrayView<T, Dims...>, Layout, Space>` is necessary both for `KokkosDataType_t` (which needs the `Dims...` pack) and for `createDeviceView` (which needs `rank_dynamic_v<Dims...>`). The dimension pack is trapped inside the `ArrayViewT` type and must be pattern-matched out.

There is no `copy_back()`. This is deliberate — `DeviceInput` is input staging only. If the user wants to modify data in place, they should use `DeviceInOut`. The absence of `copy_back()` is an API-level enforcement of directionality.

---

## Phase 7: DeviceOutput — Allocate-Only Output Staging

### The Problem

The kernel produces an output whose shape differs from any input. There is no host source array to copy from — the device memory starts uninitialized and the kernel fills it. After the kernel, the results need to be copied to a host-side destination that the user provides.

### What Is the Compiler Figuring Out?

A **type** — the Kokkos::View type for the output shape. But unlike Phases 1–6, the shape is not deduced from an existing C array. It is specified directly as template parameters. This is the first time in the project that dimensions are provided explicitly rather than deduced.

### Why Explicit Dimensions?

There is no host array to deduce from. The output shape is determined by the computation, not by the input data. A matrix multiply of `[64×128]` × `[128×32]` produces `[64×32]` — the `64` and `32` come from the input shapes, but the output array does not exist yet. The dimensions must be stated explicitly, either as template parameters (static) or constructor arguments (dynamic).

### Solution

```cpp
namespace bridge {

template<typename T, typename Layout, typename Space, std::size_t... Dims>
class DeviceOutputImpl
{
    using data_type = ulib::detail::KokkosDataType_t<T, Dims...>;
    using device_view_type = Kokkos::View<data_type, Layout, Space>;

public:
    // All-static: no runtime extents needed
    explicit DeviceOutputImpl(const std::string& label = "DeviceOutput")
        requires (ulib::detail::all_static_v<Dims...>)
        : deviceView_(label)
    {}

    // Mixed/dynamic: runtime extents needed
    // Rejects bool extents (same discipline as ArrayView — prevents
    // silent bool→int conversion) and asserts positive values.
    template<typename... Extents>
        requires (sizeof...(Extents) > 0
                  && sizeof...(Extents) == ulib::detail::rank_dynamic_v<Dims...>
                  && (std::is_integral_v<Extents> && ...) 
                  && (!std::is_same_v<std::remove_cvref_t<Extents>, bool> && ...))
    explicit DeviceOutputImpl(const std::string& label, Extents... extents)
        : deviceView_(label, static_cast<std::size_t>(extents)...)
    {
        assert(((extents > 0) && ...) && "All dynamic extents must be positive");
    }

    // Writable access for kernels
    device_view_type& view() noexcept { return deviceView_; }
    const device_view_type& view() const noexcept { return deviceView_; }

    // Copy results to a host C array — with shape validation
    template<typename Array>
        requires std::is_array_v<Array>
    void copy_to(Array& dest)
    {
        auto destView = ulib::makeArrayView(dest);
        validateShape(destView);
        Kokkos::deep_copy(destView.to_kokkos(), deviceView_);
    }

    // Copy results to a host ArrayView — with shape validation
    template<typename U, std::size_t... DDims>
    void copy_to(ulib::ArrayView<U, DDims...> dest)
    {
        validateShape(dest);
        Kokkos::deep_copy(dest.to_kokkos(), deviceView_);
    }

private:
    device_view_type deviceView_;

    // Shape validation: compile-time check for all-static cases,
    // runtime check for dynamic cases.
    template<typename U, std::size_t... DDims>
    void validateShape(ulib::ArrayView<U, DDims...> dest) const
    {
        // Rank must always match
        static_assert(sizeof...(DDims) == sizeof...(Dims),
            "Destination rank does not match DeviceOutput rank");

        // For all-static shapes, check dimensions at compile time
        if constexpr (ulib::detail::all_static_v<Dims...>
                      && ulib::detail::all_static_v<DDims...>)
        {
            static_assert(((Dims == DDims) && ...),
                "Destination static dimensions do not match DeviceOutput dimensions");
        }
        else
        {
            // For dynamic shapes, check extents at runtime before deep_copy
            [[maybe_unused]] constexpr std::size_t src_dims[] = {Dims...};
            [[maybe_unused]] constexpr std::size_t dst_dims[] = {DDims...};
            for (std::size_t i = 0; i < sizeof...(Dims); ++i)
            {
                // If both are static, they were already checked above.
                // If either is dynamic, compare runtime extents.
                assert(dest.extent(i) == deviceView_.extent(i)
                       && "Destination extent does not match DeviceOutput extent");
            }
        }
    }
};

// Public alias with defaulted Layout and Space
template<typename T, std::size_t... Dims>
using DeviceOutput = DeviceOutputImpl<
    T,
    Kokkos::LayoutRight,
    typename Kokkos::DefaultExecutionSpace::memory_space,
    Dims...>;

// Factory for all-static output
template<typename T, std::size_t... Dims>
    requires (ulib::detail::all_static_v<Dims...>)
auto make_device_output(const std::string& label = "DeviceOutput")
{
    return DeviceOutput<T, Dims...>(label);
}

// Factory for dynamic output — constrained so it cannot match
// with an empty Extents pack (which would ambiguate with the
// all-static overload above)
template<typename T, std::size_t... Dims, std::integral... Extents>
    requires (ulib::detail::rank_dynamic_v<Dims...> > 0
              && sizeof...(Extents) == ulib::detail::rank_dynamic_v<Dims...>)
auto make_device_output(const std::string& label, Extents... extents)
{
    return DeviceOutputImpl<
        T, Kokkos::LayoutRight,
        typename Kokkos::DefaultExecutionSpace::memory_space,
        Dims...>(label, extents...);
}

} // namespace bridge
```

### Usage — Matrix Multiply With Different-Shaped Input and Output

```cpp
void matmul()
{
    double A[64][128];
    double B[128][32];
    double C[64][32];

    fill_matrix(A);
    fill_matrix(B);

    // Input staging — read-only on device
    auto devA = bridge::make_device_input(A, "A");
    auto devB = bridge::make_device_input(B, "B");

    // Output staging — different shape, no host source
    auto devC = bridge::make_device_output<double, 64, 32>("C");

    // Kernel
    auto a = devA.view();
    auto b = devB.view();
    auto c = devC.view();
    Kokkos::parallel_for("matmul", 64, KOKKOS_LAMBDA(int i) {
        for (int j = 0; j < 32; ++j) {
            double sum = 0.0;
            for (int k = 0; k < 128; ++k)
                sum += a(i, k) * b(k, j);
            c(i, j) = sum;
        }
    });
    Kokkos::fence();

    // Copy result to host — C is now filled
    devC.copy_to(C);
}
```

### Usage — Dynamic Output Shape

```cpp
void histogram(const double* samples, std::size_t n, int num_bins)
{
    ulib::ArrayView<const double, ulib::dynamic_extent> input(samples, n);
    auto devInput = bridge::make_device_input(input, "samples");

    // Output shape determined at runtime
    auto devHist = bridge::make_device_output<int, ulib::dynamic_extent>(
        "histogram", static_cast<std::size_t>(num_bins));

    // ... kernel fills devHist ...

    std::vector<int> result(num_bins);
    ulib::ArrayView<int, ulib::dynamic_extent> resultView(result.data(), num_bins);
    devHist.copy_to(resultView);
}
```

### What to Notice

`DeviceOutput` does not store a host view — there is no host source to refer back to. `copy_to()` takes the destination as an argument rather than remembering one from construction. The destination can be any C array or ArrayView with compatible shape. This is the fundamental difference from `DeviceInOut`: the source and destination of the data are decoupled.

The constructor has two overloads selected by a `requires` clause: one for all-static shapes (no runtime arguments), one for shapes with dynamic extents (runtime arguments required). This mirrors ArrayView's own constructor design — the all-static path takes only a pointer, the dynamic path takes the pointer plus extents. The same `all_static_v` and `rank_dynamic_v` pack utilities that ArrayView uses for its constructor constraints are reused here.

`copy_to()` now validates the destination shape before copying. The `validateShape` helper uses two levels of checking. First, a `static_assert` verifies that the rank (number of dimensions) matches — this is always a compile-time check. Second, for all-static shapes, a `static_assert` with a fold expression (`(Dims == DDims) && ...`) verifies that every dimension matches at compile time. For dynamic shapes, a runtime `assert` compares each extent before `deep_copy` runs. This catches the most likely real bug — copying a 64×32 result into a 32×64 destination — at the earliest possible point.

---

## Renaming DeviceData to DeviceInOut

The original `DeviceData` from Phases 1–4 is now `DeviceInOut` — same implementation, clearer name:

```cpp
namespace bridge {

// DeviceInOut: same shape in and out, copies both directions
template<typename ArrayViewT,
         typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space>
using DeviceInOut = DeviceData<ArrayViewT, Layout, Space>;

template<typename Layout = Kokkos::LayoutRight,
         typename Space  = Kokkos::DefaultExecutionSpace::memory_space,
         typename Array>
    requires (std::is_array_v<Array>
              && !std::is_const_v<std::remove_all_extents_t<Array>>)
auto make_device_inout(Array& arr, const std::string& label = "DeviceInOut")
{
    return make_device_data<Layout, Space>(arr, label);
}

} // namespace bridge
```

---

## What You Built

A complete Kokkos data staging library with three wrappers, each for a different data-flow pattern:

| Wrapper | Data flow | Host buffer needed at construction? | Copy direction |
|---------|-----------|-------------------------------------|----------------|
| `DeviceInput` | Host → Device (read-only) | Yes (source) | host→device on construction |
| `DeviceOutput` | Device → Host (write-only) | No (shape specified explicitly) | device→host on `copy_to()` |
| `DeviceInOut` | Host ↔ Device (read-write) | Yes (source and destination) | Both directions |

The TMP machinery is the same across all three — Peeler, KokkosDataType, DeviceViewType. The wrappers differ only in lifecycle semantics (which copies happen when) and API constraints (const views for input, no copy_back for input, no host source for output).

| Phase | What It Does | TMP Used | ArrayView Machinery Reused |
|-------|-------------|----------|---------------------------|
| 1. DeviceViewType | Compute Kokkos::View type | Type composition | KokkosDataType_t |
| 2. DeviceData | RAII lifecycle wrapper | Template deduction | ArrayView::to_kokkos() |
| 3. Factory | One-line construction | Peeler via makeArrayView | makeArrayView, Peeler |
| 4. Dynamic extents | Runtime-sized data | No new TMP pattern — same `index_sequence` expansion | `createDeviceView` via `rank_dynamic_v`, `extent()` |
| 5. Shape mismatch | Identify the design problem | — | — |
| 6. DeviceInput | Read-only input staging | Const propagation in DataType | KokkosDataType_t with const T |
| 7. DeviceOutput | Arbitrary-shape output | Explicit dims + constrained constructors | all_static_v, rank_dynamic_v |

The final lesson: the shape mismatch problem (Phase 5) was not a TMP problem — it was an API design problem. The TMP machinery from Phases 1–4 was already correct and general. The fix was separating responsibilities into three wrappers with different lifecycle contracts, not writing new template traits. Knowing when TMP is *not* the answer is as important as knowing how to write it.

### Where These Wrappers Belong — and Where They Don't

These three wrappers are **staging helpers for discrete transfers**: take host data, move it to the device, run a kernel, move results back. They are best used around individual kernel launches or short computational sequences where the data flow has a clear start, compute, and end.

They are *not* a replacement for `Kokkos::DualView`. `DualView` manages persistent mirrored host/device state with explicit `modify()` and `sync()` tracking — it knows which side was last written and only copies when necessary. If your data lives across many kernel launches and bounces back and forth between host and device over the lifetime of a simulation, `DualView` is the right tool. The staging wrappers here are for the common case where host data enters the device pipeline once, gets transformed, and comes back.

The most likely misuse is reaching for `DeviceInOut` everywhere because it feels "safe and general." It is not — it hides directionality, encourages unnecessary `copy_back()` calls, and prevents const-correctness from doing its job. Prefer `DeviceInput` for read-only staging and `DeviceOutput` for results with different shapes. Use `DeviceInOut` only when the kernel genuinely modifies data in place and the results go back to the same host buffer.

---

*TMP Project — DeviceData: Host↔Device Lifecycle for Kokkos*
