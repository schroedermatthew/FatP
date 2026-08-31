---
doc_id: UM-TENSOR-001
doc_type: "User Manual"
title: "Tensor"
fatp_components: ["Tensor", "TensorLayout", "TensorSlice", "TensorView", "TensorAlgorithms", "TensorReductions", "TensorInterop", "TensorSelection", "TensorMatmul", "TensorEquality", "TensorSerializer"]
topics: ["dynamic tensor", "tensor owner", "tensor view", "signed strides", "slicing", "broadcasting", "reductions", "interop", "matrix multiplication", "indexed selection", "tensor serialization"]
constraints: ["C++20", "header-only", "canonical owning storage", "borrowed lifetime", "injective mutation"]
cxx_standard: "C++20"
std_equivalent: null
boost_equivalent: "Boost.MultiArray (partial overlap)"
build_modes: ["Debug", "Release"]
last_verified: "2026-08-31"
audience: ["C++ developers", "library maintainers"]
status: "in_work"
---

# User Manual - Tensor

`fat_p::Tensor<T, Allocator>` is the owning dynamic tensor type. Owners always
store values in canonical contiguous row-major order. Non-owning mappings use
`TensorView<T>`, `TensorView<const T>`, or `SharedTensorView<T>`.

The separation is deliberate:

| Type | Storage lifetime | Writable | Layout |
|---|---|---:|---|
| `Tensor<T, Allocator>` | Owned | Yes | Canonical contiguous |
| `TensorView<T>` | Borrowed | Yes, when layout is injective | Any validated injective layout |
| `TensorView<const T>` | Borrowed | No | Any validated layout |
| `SharedTensorView<T>` | Shared | Yes, when layout is injective | Any validated injective layout |
| `SharedTensorView<const T>` | Shared | No | Any validated layout |

## Headers

```cpp
#include "Tensor.h"             // owner plus view factories
#include "TensorView.h"         // explicit view-facing include
#include "TensorLayout.h"       // extents, axes, strides, layouts
#include "TensorAlgorithms.h"   // serial owner/view algorithms
#include "TensorReductions.h"   // deterministic axis reductions
#include "TensorInterop.h"      // span, descriptor, mdspan, and static conversion
#include "TensorMatmul.h"       // matmul, dot, outer, diagonal, and trace
#include "TensorSelection.h"    // stack, concatenate, take, and gather
#include "TensorSlice.h"        // extended slice vocabulary
#include "TensorEquality.h"     // EqualityComparisons integration
#include "TensorSerializer.h"   // portable logical-value serialization
```

All public facades live in `include/fat_p/`. Implementation-owned headers are
centralized under `include/fat_p/tensor/`.

## Construction

```cpp
using namespace fat_p;

Tensor<float> empty;                   // extents {0}, rank 1, size 0
Tensor<float> scalar({}, 3.5f);        // extents {}, rank 0, size 1
Tensor<float> matrix({2, 3}, 0.0f);    // rank 2, size 6

matrix(1, 2) = 7.0f;
float value = matrix.atLinear(5);
```

Any zero extent makes a tensor empty. Extent products and canonical strides
are checked against both `size_t` and `ptrdiff_t` before element allocation.
A moved-from owner is reset to the canonical empty state `{0}`.
Move construction is not declared `noexcept`: assertions-enabled lifetime
tracking and allocator state may require operations that can throw. Code that
requires a non-throwing relocation primitive should store an indirection to the
owner instead of assuming `Tensor` is nothrow-movable.

Allocator instances are explicit when needed:

```cpp
PoolAllocator<float> pool(/* state */);
Tensor<float, PoolAllocator<float>> values(
    std::allocator_arg, pool, DynamicExtents{100, 20}, 0.0f);
```

Copy construction uses `select_on_container_copy_construction`. Copy
assignment, move assignment, and swap honor POCCA, POCMA, and POCS. Storage
deleters retain the allocator instance that performed allocation, so destruction
does not depend on the current owner object's allocator state.

## Extents and layouts

`DynamicExtents` is the runtime shape vocabulary. `TensorLayout` is pointer-free
metadata containing:

- backing storage length in elements;
- logical origin offset;
- extents;
- signed element strides;
- checked minimum and maximum reachable offsets;
- a layout classification.

```cpp
TensorLayout reversed(
    6,                         // storage length
    2,                         // logical origin
    DynamicExtents{2, 3},
    TensorStrides{3, -1});

auto kind = reversed.kind();  // TensorLayoutKind::InjectiveStrided
```

The classifications are `Empty`, `Contiguous`, `InjectiveStrided`,
`Broadcast`, `Overlapping`, and `Indeterminate`. Empty layouts are contiguous
and injective. Singleton-axis strides do not constrain contiguity. Writable
views require a proven-injective layout; broadcast, overlapping, and
indeterminate mappings remain read-only.

Axes use `TensorAxis` (`ptrdiff_t`). `normalizeAxis` accepts negative axes, and
`normalizeAxes` rejects duplicates after normalization.

## Owner access

Owner storage is contiguous, so `data()`, pointer iterators, and ordinary STL
algorithms are valid:

```cpp
Tensor<int> values({2, 3});
std::iota(values.begin(), values.end(), 1);

assert(values(1, 2) == 6);
assert(values[3] == 4);
assert(values.data()[5] == 6);
```

Multidimensional access enforces exact rank and per-axis bounds. `operator[]`
is conventional unchecked contiguous owner access. `atLinear` is the checked
logical row-major form.

## Borrowed views

Borrowed factories are lvalue-qualified. A temporary owner cannot create a
borrowed mapping.

```cpp
Tensor<int> matrix({3, 4});
std::iota(matrix.begin(), matrix.end(), 1);

TensorView<int> all = matrix.asView();
TensorView<const int> readOnly = std::as_const(matrix).asConstView();
auto row = matrix.rowView(1);                    // extents {1, 4}
auto column = matrix.columnView(2);              // extents {3, 1}
auto interior = matrix.sliceView({1, 1}, {3, 4});
auto transposed = matrix.transposeView();         // extents {4, 3}
auto reshaped = matrix.reshapeView(DynamicExtents{2, 6});
```

Slice bounds are half-open. The original start/finish overload remains useful
for rectangular subviews. `TensorSlice.h` adds omitted endpoints, negative
indices and steps, integer-axis removal, `NewAxis`, and one `Ellipsis`:

```cpp
auto reversed = matrix.sliceView({All, Slice{std::nullopt, std::nullopt, -1}});
auto selected = cube.sliceView({-1, NewAxis, Ellipsis, Slice{0, std::nullopt, 2}});
auto permuted = cube.permuteView({2, 0, 1});
auto compactRank = singletonAxes.squeezeView();
auto expandedRank = compactRank.unsqueezeView(-1);
```

All of these operations are metadata-only. Integer indices are checked and
remove an axis. Negative steps produce signed-stride views. `squeezeView`
rejects a requested non-singleton axis, and permutation axes must be complete
and unique.

`reshapeView` requires a contiguous source and an unchanged logical element
count. `transposeView` is rank-two. These transforms modify metadata only and
allocate no element storage. Layout classification may use bounded temporary
metadata scratch for small higher-rank mappings.

For arbitrary external storage, construction is visibly non-owning:

```cpp
int storage[6]{};
auto view = TensorView<int>::borrow(
    storage,
    TensorLayout(6, 2, DynamicExtents{2, 3}, TensorStrides{3, -1}));
```

Mutable `borrow` and `share` reject layouts that do not prove injectivity.
Read-only `TensorView<const T>` and `SharedTensorView<const T>` may represent
broadcast, overlapping, or indeterminate layouts.

External ownership transfer is explicit and retains the supplied deleter:

```cpp
auto* storage = new int[3]{1, 2, 3};
auto owned = Tensor<int>::adopt(
    storage, DynamicExtents{3}, [](int* values) { delete[] values; });
```

The layout proves every reachable offset before the view is returned. A
nonempty layout rejects a null storage pointer.

## Shared views and lifetime

Use `asSharedView()` when the mapping must extend element-storage lifetime:

```cpp
SharedTensorView<int> saved;
{
    Tensor<int> owner({2}, 9);
    saved = owner.asSharedView();
}
assert(saved[1] == 9);
```

Borrowed views never extend lifetime. In assertions-enabled Debug builds,
owner-created borrowed views carry a weak lifetime token and throw
`std::runtime_error` when accessed after documented invalidation. Release builds
do not turn dangling access into a supported operation.

| Owner event | Borrowed view | Shared view |
|---|---|---|
| Element mutation | Remains valid; observes mutation | Remains valid; observes mutation |
| Owner destruction | Invalid | Remains valid |
| Owner move | Invalid | Remains valid |
| Owner copy/move assignment | Destination's previous borrowed views invalid | Previous shared storage remains alive |
| Owner swap | Borrowed views from both owners invalid | Shared storage remains alive |
| View copy/assignment | Copies/rebinds mapping metadata | Copies/rebinds mapping and lifetime handle |

## Constness and broadcasting

Element constness is represented in the element type. A const owner produces
only `TensorView<const T>`. Broadcast mappings are always read-only because an
expanded zero-stride axis aliases values:

```cpp
const Tensor<int> scalar({}, 5);
TensorView<const int> grid = scalar.broadcastView(DynamicExtents{2, 3});
```

Broadcast compatibility follows trailing-axis rules. Equal extents match;
extent one may expand; zero with one produces zero. Zero with an extent greater
than one is incompatible.

## Materialization

`clone` is the explicit owner-from-readable operation:

```cpp
auto column = matrix.columnView(1);
Tensor<int> compact = clone(column);

auto broadcast = scalar.broadcastView(DynamicExtents{100, 100});
Tensor<int> ownedBroadcast = clone(broadcast);
```

The result is canonical contiguous storage and never aliases the source. An
explicit allocator overload is available. Without one, owner inputs use SOCCC;
view-only calls use `TensorAllocator<T>`.

Existing `clone` overloads have different element requirements: `clone(owner)`
copy-constructs values and can handle a copy-constructible, non-default-initializable
type. `clone(view)` and `clone(source, allocator)` default-initialize result
elements and then copy-assign them, so those paths require both operations.
"Unconditional copy" describes independent ownership, not support for every
element type. This step does not change those existing overloads.

`TensorAlgorithms.h` also provides copy-forcing materialization and explicit
element transfer:

```cpp
auto packed = clone(matrix.transposeView()); // always independent
auto flattened = reshapeCopy(matrix.transposeView(), DynamicExtents{matrix.size()});

auto destination = matrix.columnView(0); // keep a named, non-const destination
auto source = matrix.columnView(1);
copyFrom(destination, source);          // preserves destination storage and shape

Tensor<int> square({2, 2});
// ... populate square ...
copyFrom(square, square.transposeView()); // safe even though the mappings overlap
```

`clone` copies even an already-contiguous input. `reshapeCopy` copies
the source's logical row-major sequence into the requested canonical shape;
unlike `reshapeView`, it accepts noncontiguous inputs. Only the logical element
counts must match: a scalar can reshape to `{1, 1}`, and `{2, 0}` can reshape to
`{0, 3}`. A count mismatch throws `std::invalid_argument` before element storage
is allocated. Both functions support an explicit allocator as the final
argument and otherwise use the same result-allocator rule as `clone`.

`copyFrom(destination, source[, scratchAllocator])` requires exactly matching
extents and value types. It does not broadcast implicitly, resize an owner,
rebind a view, replace storage, or invalidate existing views. The destination
must be a non-const lvalue with an injective mapping; broadcast and overlapping
read-only mappings are allowed as sources. Explicitly create a `broadcastView`
first if repetition is desired.

Mutable `borrow` and `share` factories reject noninjective layouts at view
construction. Supported destinations are therefore already injective; the copy
kernel rechecks this invariant defensively.

When reachable address intervals are disjoint, `copyFrom` writes directly and
allocates no scratch **elements**. When they overlap, or disjointness cannot be
proved, it first snapshots all logical source values. This includes self-copy
and conservative staging for interleaved mappings whose address intervals
overlap even though their individual elements do not. Empty copies allocate no
element storage, but still require exactly matching extents. Rank-sized
metadata may allocate separately; this is not a zero-allocation API promise.

Scratch uses the explicit allocator if provided; otherwise it uses the
destination owner's exact allocator instance **without SOCCC**, or
`TensorAllocator<T>` for a view destination. Source ownership never selects
scratch, and scratch is released before return. Validation, allocation, and
snapshot failure leave destination values unchanged. A user element assignment
that throws during the final transfer can leave partially copied values; the
storage and mapping remain intact, with element validity dependent on the
element's own assignment guarantee. There is no rollback. Borrowed-lifetime
diagnostics are assertions-enabled checks, not support for dangling views in
Release builds. Callers must synchronize concurrent access through aliases.

The two new helpers currently require default-initializable, copy-assignable
elements, including when a particular copy would not require staging. They do
not add support for move-only or non-default-initializable element copying.

The same rule applies to all free allocating algorithms in this chapter:
without an explicit allocator, the first owning argument from left to right
supplies its allocator through SOCCC. Type-changing operations such as integral
`sum` and `matmul` rebind that allocator to the result element type. A call made
only from views uses the result type's default `TensorAllocator`. Each operation
also provides an explicit result-allocator overload.

## Serial algorithms

`TensorAlgorithms.h` provides the current base-kernel surface:

```cpp
auto sum = add(left, right);
auto difference = subtract(left, right);
auto product = multiply(left, right);
auto quotient = divide(left, right);
auto negated = transform(input, [](auto value) { return -value; });

bool exact = exactEqual(left, right);
bool close = approxEqual(left, right, 1e-6, 1e-5);
```

Binary operations and `+`, `-`, `*`, `/` accept different arithmetic element types,
apply trailing-axis broadcasting, and produce a new canonical owner:

```cpp
Tensor<std::int32_t> signedValues({2}, -1);
Tensor<std::uint32_t> unsignedValues({2}, 1);
auto combined = signedValues + unsignedValues; // Tensor<int64_t>, values are zero
static_assert(std::same_as<TensorArithmeticType<std::int32_t, std::uint32_t>, std::int64_t>);
```

The promotion rules preserve input integer ranges:

| Inputs | Result type |
|---|---|
| Same non-bool type | Same type, including narrow integers |
| Different integers, same signedness | Wider range; equal-range ties use usual C++ arithmetic conversions |
| Signed + unsigned | Signed operand type if sufficient; otherwise a covering `int16_t`, `int32_t`, or `int64_t` |
| Signed + `uint64_t` | Rejected at compile time |
| `float` + integer | `float` if all integer value bits fit its binary significand; otherwise `double` |
| `double`/`long double` + integer | That floating type; conversion may round |
| Two floating types | Wider floating type |

`TensorArithmeticCompatible<A, B>` checks support for two element types.
Standard character types follow their numeric range/signedness, not text
semantics. `bool`, enums, complex numbers, and user-defined numeric types are
not accepted. Binary bool arithmetic is deliberately removed; use `all`/`any`
for truth reductions and `sum` to count true elements.

Character promotion can change with the platform. With 8-bit characters and
32-bit `int`, `char + unsigned char` produces `int16_t` when `char` is signed,
but `int` when it is unsigned (the equal-range tie uses integer promotion).
Likewise, `float + wchar_t` retains float for a 16-bit `wchar_t`, but produces
double for a 32-bit `wchar_t`. Use fixed-width integer element types when this
distinction matters to an interface.

Both inputs convert before computing. Integer results detect overflow or
underflow and throw `std::overflow_error`; same-type `int8_t` arithmetic still
checks the 8-bit range. Floating computation uses ordinary typed arithmetic:
`float + int32_t` computes in double, while `float + int16_t` stays float.
64-bit integers may round when converted to double. Long double is never
introduced unless supplied by an operand.

An explicit result allocator is used unchanged and must match the result type.
Without one, the first owner left-to-right supplies its allocator, rebound to
the result type and then SOCCC-selected. Views alone use
`TensorAllocator<Result>`. Inputs and broadcast compatibility are validated
before result element allocation; empty output allocates no result elements.
Failure leaves both inputs unchanged and publishes no partial result.
Borrowed lifetime checks remain Debug diagnostics, not Release lifetime safety.
Custom owner allocators must support the standard cross-type rebinding
constructor; supplying an explicit result allocator bypasses that selection.

`transform` still retains its source value type; `copyFrom`, `exactEqual`,
and `approxEqual` still require matching element types. `approxEqual` is also
restricted to floating-point element and tolerance types. Same-sign infinities
are equal; an infinity and a finite value are never approximately equal,
regardless of relative tolerance. NaNs are not approximately equal.
In-place arithmetic remains future work.

Scalar arithmetic works in both orders, with the same promotion and overflow
rules as tensor/tensor arithmetic:

```cpp
auto scaled = input * 2.0;
auto shifted = 10 - input;
auto explicitResult = subtract(10.0, input, std::allocator<double>{});
```

The result retains the tensor's exact extents; no scalar Tensor is allocated.
The scalar is snapshotted by value, so `input + input[0]` is supported.
The tensor operand supplies the owner allocator regardless of which side it
appears on. Literal types matter: `Tensor<int8_t> + 100` widens to int,
whereas an int8_t scalar retains the checked int8_t result. Supplied
long-double scalars are supported; bool scalars remain excluded.

For a float tensor, multiplying by `2.0` returns a double tensor; use `2.0f`
to retain float. An unsigned64 tensor plus `1` is rejected because the signed
and unsigned type domains have no covering signed result; `1u` is supported.
An explicit scalar-operation allocator must match this promoted result type:
it is not automatically rebound on the explicit-allocator path.

Generic owner/view materialization, fill, unary, binary, policy equality, exact
equality, and hash traversal use one `TensorIterationPlan`. Canonical owner copy
construction uses its contiguous storage invariant directly and does not run a
private layout walker. The plan normalizes ranks, coalesces compatible axes,
uses counted signed offsets, and never constructs an out-of-span sentinel
pointer. Owners expose contiguous random-access pointer iterators; views expose
counted logical forward iterators whose equality domain is the underlying
mapping, not the address of a particular view object.

## Division: integer quotients or floating results?

Division uses the same promotion rules as addition and multiplication. Two
integer operands produce an integer quotient truncated toward zero. To retain
fractional quotients, supply a floating operand or convert the tensor first.
This distinction also applies to scalar literals:

```cpp
Tensor<int> values({2}, 7);
values[1] = -7;
auto integerQuotients = values / 2;       // int values {3, -3}
auto fractionalQuotients = values / 2.0;  // double values {3.5, -3.5}
auto reverseQuotients = 14 / values;      // int values {2, -2}
auto allocated = divide(values, 2.0, std::allocator<double>{});
```

Tensor/tensor division broadcasts trailing axes; both scalar orders preserve
the tensor's exact extents. `divide(left, right, allocator)` accepts an explicit
result allocator in all three forms. Each call materializes independent
storage and leaves inputs unchanged, including when the scalar refers to an
input element.

With an integral result type, an evaluated zero divisor throws `std::domain_error`. A signed
result's minimum divided by negative one throws `std::overflow_error`, even
for int8_t or int16_t results. Promotion happens first: an int8_t tensor holding
`-128` divided by the int literal `-1` returns int `128`, while division by an
int8_t negative-one scalar throws. Integer division does not round through
double or silently wrap.

Floating results use native floating division. On supported IEC 559 platforms
in the default nontrapping environment, division by signed zero produces the
ordinary signed infinity or NaN, infinity/infinity produces NaN, and overflow
or underflow follows native arithmetic. Tensor does not translate these into
integer-domain exceptions, clear floating exception flags, or intercept
caller-enabled traps. Compiler options that discard NaN or signed-zero
semantics cannot preserve these guarantees.

Divisors are checked while computing the result, after result allocation; an
allocation failure can therefore occur before an invalid quotient is reached.
Any later failure releases the unpublished result. Empty output evaluates no
quotient: an empty integer tensor divided by zero returns empty without an
element allocation. A rank-zero tensor contains one value, so division of that
value by integer zero throws. Shape and Debug borrowed-lifetime checks still
precede result element allocation, including for empty inputs.

## Negation and absolute value

Use `negate`, unary minus, or `abs` for checked elementwise operations. Each
returns a new contiguous owner and preserves the source shape and element type,
including narrow integers. Owners, borrowed views, and shared views are accepted.

```cpp
fat_p::Tensor<int> values({2}, -3);
auto negative = -values;                        // {3, 3}, Tensor<int>
auto named = fat_p::negate(values.asConstView()); // {3, 3}, independent storage
auto magnitude = fat_p::abs(values);            // {3, 3}, Tensor<int>
auto allocated = fat_p::abs(values, std::allocator<int>{});

fat_p::Tensor<std::uint8_t> bytes({2}, std::uint8_t{255});
auto signedNegative = -fat_p::cast<std::int16_t>(bytes); // {-255, -255}
// -bytes throws std::overflow_error: negative nonzero values do not fit uint8_t.
```

Signed integer `lowest()` throws `std::overflow_error` for both negation and
absolute value; cast to a sufficiently wide type first if that result is
needed. Unsigned `abs` copies the values; unsigned negation accepts only zero.
There is no wraparound or implicit promotion to `int`. Boolean tensors do not
participate in these operations.

Floating negation uses native unary minus, including `+0` becoming `-0`.
Floating `abs` uses `std::fabs`: on IEC 559 platforms either zero becomes `+0`,
either infinity becomes positive infinity, and NaNs remain NaNs. NaN sign and
payload are not specified. The caller's floating environment is used without
trap interception or flag restoration; fast-math is outside this contract.

The default result allocator comes from the owner through copy selection;
view-only calls use `TensorAllocator<T>`. The named functions accept an explicit
allocator with the same `value_type`, used unchanged. Nonempty calls allocate
one result element buffer, with no element scratch buffer; metadata may allocate
separately. Empty shapes allocate no elements and evaluate no values. Rank-zero
inputs still evaluate their single value. Failures discard the unpublished
result without changing the source. Tracked borrowed lifetimes are checked
before element allocation, even for empty inputs.

## Square roots, exponentials, and logarithms

`sqrt`, `exp`, and `log` operate on floating-point tensors and materialize new
owners without changing dtype or shape. `log` means natural logarithm.

```cpp
fat_p::Tensor<double> values({3}, 4.0);
auto roots = fat_p::sqrt(values);                // {2, 2, 2}
auto exponentials = fat_p::exp(values.asConstView());
auto logarithms = fat_p::log(values.asSharedView());
auto allocated = fat_p::sqrt(values, std::allocator<double>{});

fat_p::Tensor<int> counts({2}, 9);
auto countRoots = fat_p::sqrt(fat_p::cast<double>(counts)); // {3, 3}
// fat_p::sqrt(counts) does not compile: cast integers explicitly first.
```

Only `float`, `double`, and `long double` participate. The matching scalar
standard-library function is called directly; no intermediate `double` tensor
or new dependency is introduced. Owners and mutable/const borrowed or shared
views use the same allocation and lifetime rules as `abs` above. Empty tensors
evaluate no values, whereas a rank-zero tensor evaluates its single value.

Floating domain errors are **not C++ exceptions**. On IEC 559 platforms with
traps disabled, negative square roots and logarithms return NaN, `log(0)` and
`log(-0)` return negative infinity, and `sqrt(-0)` preserves negative zero.
Exponential overflow/underflow follows native scalar math. NaN payloads and
cross-platform bitwise results are not promised. The scalar library may set
`errno` or floating flags; Tensor does not clear or restore them, suppress traps,
or promise fast-math behavior. Allocation and tracked-lifetime failures still
throw normally without changing the source.

## Updating values without replacing storage

Compound arithmetic updates an owner or a named writable view while keeping
its shape, storage address, allocator, and existing views intact:

```cpp
Tensor<int> counts({2, 3}, 8);
Tensor<int> rowOffsets({3}, 2);
counts += rowOffsets;                    // broadcast the row; all values become 10
auto row = counts.rowView(0);
row *= 3;                               // write through to the first row
divideAssign(counts, 2);                 // first row 15, second row 5
subtractAssign(counts, 1, std::allocator<int>{}); // explicit scratch allocator
```

All four operators, `+=`, `-=`, `*=`, and `/=`, accept tensor or scalar
RHS values. Their named equivalents are `addAssign`, `subtractAssign`,
`multiplyAssign`, and `divideAssign`. Each returns the original destination
by reference, allowing chaining. Bind a destination view to a non-const local
variable first: temporary views and const view handles are deliberately
rejected, matching `copyFrom`'s destination rule.

RHS broadcasting must produce exactly the destination shape. An update
cannot grow or shrink that shape, nor add a leading singleton axis. Overlapping
RHS views, including a transpose or a broadcast row of the destination, read
the original values throughout the operation.

The compute type follows the usual Tensor promotion table, but every result
must also pass checked conversion back to the destination type:

```cpp
Tensor<int> whole({1}, 7);
whole /= 2;                             // integral computation: stores 3
whole /= 2.0;                           // throws domain_error; whole remains 3
Tensor<std::int8_t> small({1}, std::int8_t{127});
small += 1;                             // int result 128 cannot fit: overflow_error
```

Unsigned subtraction cannot wrap. Literal types still affect compatibility:
a uint64_t tensor accepts `+= 1u` but rejects `+= 1` because the latter
would require an unsupported full-range signed/unsigned compute type.
Floating write-back allows normal rounding and tiny values rounding to zero;
finite overflow throws. Native floating NaN/infinity behavior is retained,
subject to the floating-environment restrictions described above.

The strong failure guarantee costs one destination-sized scratch element
buffer per nonempty call. Arithmetic, conversion, or allocation failure leaves
all destination values and metadata unchanged, including when an error occurs
on a later element. Completed scratch values are copied back without replacing
the destination storage. The scratch owner value-initializes elements before
computation and creates its usual lifetime metadata; metadata may allocate
separately. Allocator bookkeeping and floating status flags are not rolled
back; concurrent/reentrant alias mutation and hardware floating traps are
outside this guarantee.

The optional allocator must allocate the destination value type, not the
promoted compute type. Otherwise owners use their exact allocator without
SOCCC or rebinding, and views use `TensorAllocator<T>`. The RHS allocator is
never selected. Valid empty updates allocate no scratch elements and evaluate
no values, even for `empty /= 0`; shape and Debug lifetime checks still run.
Rank-zero updates evaluate their one element.

## Checked numeric conversion

Use `cast<To>` to convert values and obtain an independent canonical owner:

```cpp
Tensor<double> samples({3}, 2.0);
auto integers = cast<int>(samples);             // values {2, 2, 2}
auto scaled = 0.5 * integers;                   // double values {1, 1, 1}
auto extended = cast<long double>(samples);     // explicit precision selection
```

Shapes and logical order are preserved for owners and strided, broadcast, or
shared views. Same-type casts still copy; they never steal or alias storage.

| Destination | Conversion behavior |
|---|---|
| Integer | Must fit its range; floating inputs must also be finite and integral-valued |
| Bool | Only exact zero (including negative zero) and one are accepted |
| Floating | Rounding is allowed; finite overflow throws; tiny values may round to zero |

Out-of-range values throw `std::overflow_error`. Fractional/nonfinite inputs
to integers and values other than zero/one to bool throw `std::domain_error`.
Fractional rejection comes first: converting -0.5 to unsigned is a domain
error, not truncation to zero. Floating NaN/infinity conversions preserve
their category when the target supports it, without a cross-type NaN payload
or sign guarantee. Integer-to-floating casts may lose precision.
Different C++ floating types still count as a type-changing cast when their
representations happen to match, as double and long double do on MSVC.

Finite magnitude above a floating destination's maximum is rejected even if
hardware might round it down; underflow to zero is deliberately allowed.
This is not an exactness, saturation, or truncation API. Standard character
types follow numeric range/signedness, not text encodings. Target types must
be unqualified arithmetic types; enums, std::byte, pointers, and user types
are not accepted. Bool-to-integer casts provide 0/1 values for arithmetic.

An explicit allocator must have value_type `To` and is used unchanged.
Without one, an owner supplies its allocator rebound to `To` before SOCCC;
a view uses `TensorAllocator<To>`. Source lifetime is checked before element
allocation. Values are checked during copying; failure releases the partial
result and preserves the source. Empty results allocate no element buffer
and convert no values; rank-zero results convert exactly one.

## Reductions

Use reductions to summarize selected dimensions while keeping the others.
`TensorReductions.h` produces an independent owning result from owners or
validated strided views, including read-only broadcast/overlapping mappings.
It reduces all axes by default or with an empty axis list. Negative axes are
accepted; duplicates are rejected after normalization. `keepDimensions`
retains reduced axes with extent one. The following calls illustrate the
axis/output-shape choices and boolean aggregation:

```cpp
auto total = sum(values);                  // rank-zero result
auto rows = sum(values, {1});
auto columns = mean(values, {0}, true);    // shape {1, columns}
auto locations = argmax(values, {1});
auto every = all(values, {1});             // one bool per row
auto some = any(values);                   // rank-zero bool result
```

`sum` and `product` accumulate `bool` in `size_t`, smaller signed integers in
`int64_t`, and smaller unsigned integers in `uint64_t`. Other input types retain
their type, so a float sum accumulates in float. Every integral combine is
checked: even intermediate overflow throws before a later value can cancel it.
Floating overflow follows ordinary arithmetic rather than throwing that error.

`mean` converts each value to double before summing and dividing; long-double
inputs instead use long double throughout. Integer conversion can round, and
`mean(values) * count` need not equal `sum(values)`. Reductions fold values in
serial logical row-major order without compensation. That order is fixed, but
bitwise agreement across platforms or fast-math builds is not promised.

Empty sum/product domains use zero/one; empty `all`/`any` domains use true/false.
Mean and argument extrema reject a nonempty output with an empty domain.
`sum`, `product`, `min`, and `max` accept a final optional initial value that
participates once per domain, even when the domain is nonempty. An initial
value permits min/max on empty domains; without it those operations throw.
An empty output itself has no domains and does not trigger a domain error.

Extrema propagate the first NaN; tied finite values preserve the first value,
including its signed-zero representation. Argument reductions return the first
winning coordinate flattened row-major within the reduced axes, ordered by
source-axis number rather than the order of the supplied axis list. `all` and
`any` convert arithmetic values to bool: either signed zero is false; NaN,
infinity, and other nonzero values are true. They do not promise short-circuiting.

Each nonempty result allocates one element buffer; empty results allocate none.
Metadata and extrema scratch storage can allocate separately. Explicit result
allocators are used unchanged; defaults use rebound owner SOCCC or
`TensorAllocator<Result>` for view inputs. Failures leave the source unchanged.
Assertions-enabled builds reject expired borrowed sources, including empty ones;
Release callers must still ensure borrowed storage remains alive.

## Named linear algebra

`TensorMatmul.h` supports rank-one vectors, rank-two matrices, and trailing-axis
broadcasted batches:

```cpp
auto scalar = matmul(vectorA, vectorB);       // K @ K -> {}
auto output = matmul(matrixA, matrixB);       // MxK @ KxN -> MxN
auto batches = matmul(leftBatches, right);    // ...MxK @ ...KxN
auto inner = dot(vectorA, vectorB);          // K, K -> {}, read with inner()
auto pairs = outer(vectorA, vectorB);        // M, N -> MxN
auto entries = diagonal(matrixA);            // ...MxN -> ...min(M,N), copied
auto totals = trace(matrixA);                // ...MxN -> ..., rank two -> {}
```

Small integral inputs use the same widened result type as reductions, and all
integral multiply/add steps are checked. A zero contraction dimension produces
the additive identity. Contiguous matrix pairs and contiguous vector pairs use
the shared blocked serial kernel (a vector pair is the 1-by-K / K-by-1 case).
Mixed vector/matrix, batched, and other validated signed-stride layouts use the
general coordinate kernel. No external BLAS dependency is required.

`dot` and `outer` require exactly rank-one operands; neither flattens a matrix.
`matmul`, `dot`, and `outer` require matching arithmetic element types. Use
`cast<To>` explicitly for mixed types. Their result type is `TensorMatmulType<T>`,
an alias of `TensorSumType<T>`: signed/unsigned integers narrower than 64 bits
widen to `int64_t`/`uint64_t`, `bool` produces `size_t`, and other arithmetic
types retain their type. Widening happens before multiplication. Dot and matmul
fold from zero; outer performs just a product, so floating negative zero is
not lost to an extra addition. Floating operations use ordinary scalar semantics,
not compensated arithmetic or a bitwise cross-platform reproducibility promise.
Under the default floating environment, a dot product or trace containing only
negative-zero terms returns positive zero because its fold starts at positive zero.
Unlike the elementwise arithmetic family, these contractions accept bool and count
true values/products in `size_t`.

`diagonal` and `trace` use the main diagonal of the final two axes. Rectangular
matrices are accepted; batch axes are preserved without broadcasting them.
No offset or arbitrary-axis option is currently provided. Diagonal extraction
returns a same-type independent owner and supports default-initializable,
copy-assignable nonnumeric elements too. Trace is arithmetic-only, returns
`TensorSumType<T>`, and accumulates in increasing diagonal-index order.

Empty vectors give a scalar zero dot product. An outer product with either
length zero is empty but retains shape `{M,N}`. A trace of shape `{B,0,N}`
returns `B` zeros, while a trace of `{0,M,N}` is empty. All operations accept
validated signed-stride, padded, transposed, and overlapping read-only inputs.
They allocate no intermediate element buffers: diagonal and trace pass an
internal const mapping directly to the existing copy/reduction kernels.

Every operation accepts an explicit result allocator as its final argument.
Defaults select the first original owner, rebind to the result type, and apply
SOCCC once; a view-only call uses `TensorAllocator<Result>`. Invalid ranks and
shapes are rejected before element allocation. Integer overflow, allocation,
and element-copy failures publish no result and leave sources unchanged.
Borrowed lifetime checks also apply to empty inputs in assertions-enabled builds;
Release callers remain responsible for keeping borrowed storage alive.

### Replacing the retired einsum subset

`TensorEinsum.h`, its parser, and all `_einsum` wrappers have been removed.
There is no compatibility shim. Include `TensorMatmul.h` for the named linear
algebra and `TensorAlgorithms.h` / `TensorReductions.h` for the compositions:

| Retired pattern | Current operation |
|---|---|
| `ij,jk->ik`, `bij,bjk->bik` | `matmul(a, b)` |
| `i,i->`, `i,j->ij` | `dot(a, b)`, `outer(a, b)` |
| `ii->i`, `ii->` | `diagonal(a)`, `trace(a)` |
| `ij->ji` | `clone(a.transposeView())` |
| `ij->i`, `ij->j`, `ij->` | `sum(a, {1})`, `sum(a, {0})`, `sum(a)` |
| Elementwise product, `ij,ij->` | `multiply(a, b)`, `sum(multiply(a, b))` |

This is a semantic replacement, not source compatibility. Scalar results are
rank-zero owners, extracted with `result()`, rather than the old scalar-returning
dot/trace wrappers. Reductions and contractions now use the checked/widened
types above. `multiply` uses the separate binary-arithmetic promotion contract:
for a widened integer Frobenius product, cast both operands before multiplication.
Matmul batches and elementwise multiplication permit broadcasting; explicitly
check equal extents when the application requires the old exact-shape restriction.
Diagonal/trace now also accept rectangular matrices and batch prefixes.

## Composition and indexed selection

`TensorSelection.h` keeps operations with different index semantics separate:

```cpp
auto layered = stack(first, second, 0);
auto joined = concatenate(left, right, 1);
auto rows = take(matrix, rowIndices, 0);
auto chosen = takeAlongAxis(matrix, indexTensor, 1);
auto gathered = gatherND(cube, coordinateTuples);
```

`stack` inserts an axis; `concatenate` adds extents on an existing axis. The
two-operand overloads accept different readable mapping types with the same
value type. Homogeneous multi-input overloads accept an initializer list or a span of
`reference_wrapper<const Source>`. `take` accepts duplicates and negative
indices. `takeAlongAxis` requires equal ranks and matching non-axis extents.
For `gatherND`, the final indices extent is tuple depth, and unreplaced source
axes are appended to the output. Bounds are validated before result evaluation.

## Interop

`TensorInterop.h` makes layout restrictions visible:

```cpp
auto contiguous = contiguousSpan(owner);
auto descriptor = describeTensor(stridedView);
auto borrowedAgain = descriptor.borrow();
auto fixed = toStaticTensor<Shape<2, 3>>(owner);
auto dynamic = toTensor(fixed);
```

`contiguousSpan` rejects non-contiguous mappings. `StridedTensorDescriptor<T>`
owns its extent/stride metadata but borrows element storage; callers must keep
that storage alive. A descriptor created from `SharedTensorView` retains that
view's shared storage handle for the descriptor's lifetime; a view returned by
`borrow()` must not outlive the descriptor. Interop functions reject rvalues so they cannot immediately
return dangling borrowed objects. In Debug builds, descriptors preserve the
source owner's lifetime token when converted back to a view. A descriptor
preserves storage base, storage length, logical origin, extents, and signed
strides, and validates those public fields before pointer formation. When the
C++ standard library supplies C++23
`std::mdspan`, `asMdspan<Rank>` is available for injective non-negative-stride
mappings without changing the C++20 baseline.

## Hashing and policy equality

`std::hash<Tensor<T, Allocator>>` hashes extents and logical values, not physical
strides or allocator state. Equal owners therefore hash equally, including
positive and negative floating-point zero when the element hash follows the
standard equality law.

`TensorEquality.h` integrates owning tensors with `areEqual`. `exactEqual` and
`approxEqual` accept any readable owner/view pair with the same value type.

## Explicit execution contexts

Existing calls remain serial. Include the optional facade only when you want
context-aware matrix multiplication; the serial Tensor headers do not include
ThreadPool or create workers.

~~~cpp
#include "TensorExecution.h"

fat_p::ThreadPool workers(4); // application-owned; reuse across calls
auto context = fat_p::TensorExecutionContext::parallel(workers);
auto result = fat_p::matmul(left, right, context);
auto exactAllocator = fat_p::matmul(left, right, context, resultAllocator);

fat_p::TensorExecutionContext serial; // default construction is serial
auto same = fat_p::matmul(left, right, serial);

std::stop_source stop;
fat_p::TensorExecutionOptions options;
options.grainSize = 32;        // flattened batch-times-rows per cancellation tile
options.minimumWork = 1048576; // scalar products required before scheduling
options.maxTasks = 4;          // zero means no additional cap beyond pool size
options.cancellation = stop.get_token();
auto cancellable = fat_p::TensorExecutionContext::parallel(workers, options);
~~~

This increment supports context overloads of **matmul and dot only**.
The native backend partitions disjoint output rows, including batch rows.
It does not split the inner reduction: each output retains its increasing-inner
fold and checked integral arithmetic. A one-output dot, an unbatched vector
times matrix, or a one-row matrix multiplication remains serial. All other
Tensor algorithms retain their existing serial APIs.

The default scheduling cutoff is conservative, not a hardware-independent
performance guarantee. Calls with fewer than two row tiles, work below the
cutoff, a one-worker pool, or maxTasks=1 execute serially. At most
min(pool size, nonzero task cap, row-tile count) tasks are submitted per call.
There is no hidden pool and no process-wide thread-count setting. Calls from
**any Fat-P pool worker** run serially, including cross-pool calls; this prevents
saturated nested calls from waiting on another pool. Foreign worker pools are
not detected, so coordinate their concurrency in the application.

Every return or throw drains all tasks accepted for that call. Submission
failure wins over task failure; task failures are selected in increasing
submission index, not completion order. Cancellation is reported last as
TensorExecutionCancelled. No failed or cancelled call returns a partial owner.
Cancellation is checked after input validation but before element allocation,
between row tiles, and after draining. It does not interrupt a tile or promise
a maximum response time. A stop observed by the final check discards even a
fully computed result. Empty outputs and empty contractions still observe
cancellation. A stopped pool throws when scheduling is attempted; serial
fallback paths do not submit and can still succeed.

The context borrows its pool and scratch resource: keep both alive until the
call returns, and do not mutate, resize, or destroy inputs concurrently.
options.scratch must be nonnull and defaults to the standard default PMR resource.
It owns only the bounded future array, allocated and released on the calling
thread; it does not replace Tensor metadata, pool task/promise, or result-element
allocators. Shared contexts require a thread-safe scratch resource or external
serialization. Result allocator selection remains unchanged (first owner SOCCC,
or TensorAllocator for view-only operands), with an explicit final allocator
argument when needed.

Floating serial/parallel bitwise agreement requires the same build and floating
environment on the caller and workers. Contexts do not propagate caller thread-local
state, rounding modes, or exception flags. This is not cross-platform reproducibility
or a parallel reduction-combine policy. The only supported choices are serial and
deterministic row-parallel execution; no BLAS/GPU/backend placeholder flags are exposed.

See [execution measurements and validation](../results/2026-08-31-execution-contexts/README.md).
Linux ThreadSanitizer is a CI gate; this Windows implementation session did not run it.

## Serialization

```cpp
auto bytes = serialize_tensor(readable);
auto loaded = deserialize_tensor<float>(*bytes);

TensorDeserializationLimits limits;
limits.max_elements = 1'000'000;
limits.max_payload_bytes = 8 * 1024 * 1024;
auto bounded = deserialize_tensor<float>(*bytes, limits);
```

Serialization writes canonical logical values and accepts owners or views.
Deserialization always produces an owner. Version 2 distinguishes a rank-zero
scalar from an empty tensor and uses portable big-endian fields on supported
integer and IEEE-754 targets.

Deserializer rank, extent, element, and byte budgets are checked before Tensor
element storage allocation. A supplied-allocator overload lets applications
choose the result memory resource.

## Current boundaries

The separate `benchmark_TensorMatmul` executable covers the five named linear
algebra operations with floating inputs, result-buffer allocation probes,
independent scalar references, and size/layout variants. It supports the
canonical `FATP_BENCH_*` settings and optional `--filter substring`; quick mode
is a smoke test. Commands, raw outputs, variation, and the narrowly measured
contiguous-dot dispatch comparison are recorded in
[the benchmark report](../results/2026-08-31-linear-algebra/README.md).
These measurements are not a BLAS comparison or a cross-platform speed promise.

The following plan items are not yet current API promises:

- the remaining broad numeric operation families, including further unary operations;
- general tensor contraction planning and further measured kernel specialization;
- explicit parallel execution contexts;
- a complete einsum grammar.

The partial einsum API has been retired. `StaticTensor` remains a separate,
fixed-size type with its own checked/saturating arithmetic policies.

## API summary

| Category | Current API |
|---|---|
| Owner | `Tensor<T, Allocator>` |
| Runtime metadata | `DynamicExtents`, `TensorStrides`, `TensorLayout`, `TensorLayoutKind` |
| Ownership transfer | `Tensor::adopt` |
| Borrowing | `TensorView<T>::borrow`, `asView`, `asConstView` |
| Shared lifetime | `SharedTensorView<T>::share`, `asSharedView` |
| View transforms | `sliceView`, `rowView`, `columnView`, `transposeView`, `reshapeView`, `broadcastView` |
| Extended transforms | `Slice`, `All`, `NewAxis`, `Ellipsis`, `permuteView`, `squeezeView`, `unsqueezeView` |
| Materialization / transfer | `clone`, `reshapeCopy`, `copyFrom`, `cast<To>` |
| Base algorithms | `add`, `subtract`, `multiply`, `divide`, `transform`, `exactEqual`, `approxEqual` |
| Checked unary arithmetic | `negate`, unary `operator-`, `abs`; dtype-preserving materialized results |
| Floating unary math | `sqrt`, `exp`, `log`; float/double/long double, native scalar math behavior |
| Compound updates | `addAssign`, `subtractAssign`, `multiplyAssign`, `divideAssign`; `+=`, `-=`, `*=`, `/=` |
| Reductions | `sum`, `product`, `mean`, `min`, `max`, `argmin`, `argmax`, `all`, `any` |
| Linear algebra | `matmul`, `dot`, `outer`, `diagonal`, `trace` |
| Composition/selection | `stack`, `concatenate`, `take`, `takeAlongAxis`, `gatherND` |
| Interop | `contiguousSpan`, `describeTensor`, `StridedTensorDescriptor`, `asMdspan`, `toTensor`, `toStaticTensor` |
| Owner queries | `extents`, `layout`, `strides`, `rank`, `extent`, `size`, `empty`, `data` |
| Access | `operator[]`, `atLinear`, `operator()`, `at`, `begin`, `end` |
| Owner operations | `fill`, `clone`, `swap`, `get_allocator`, `operator==` |
| Arithmetic type queries | `TensorArithmeticType<A, B>`, `TensorArithmeticCompatible<A, B>` |
| Arithmetic operators | `operator+`, `operator-`, `operator*`, `operator/`; tensor/tensor and both scalar orders |
| Serialization | `serialize_tensor`, `deserialize_tensor`, `TensorDeserializationLimits` |
