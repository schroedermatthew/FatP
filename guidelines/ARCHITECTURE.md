# Architectural invariants

These are adopted FatP commitments, not inherited template architecture. Owner
direction is distinguished from technical decisions made within that authority.
The source references establish the decisions; they do not prove every file is
currently conformant. Current gate observations belong in
[CURRENT_VERIFICATION](CURRENT_VERIFICATION.md).

## Project definition

See [PROJECT_PHILOSOPHY](PROJECT_PHILOSOPHY.md) for recurring priorities and
[PROJECT_PROFILE](PROJECT_PROFILE.md) for actual build and inventory configuration.

## FP-A01 Dependency ownership

**Binding statement:** the core consumer target is header-only and requires C++20;
optional external integrations and benchmark competitors must not become transitive
dependencies of unrelated core headers or the fatp target.

- Scope: public includes, CMake usage requirements and optional backends.
- Operational test: compile a consumer of the affected core header with only the
  declared standard/system dependencies, inspect its include closure and fatp
  usage requirements, and compile each supported integration configuration separately.
- Consequence: a leaked dependency breaks the zero-third-party core contract and
  changes consumer build cost or availability.
- Rationale: optional interoperability should not tax unrelated users. Blanket
  imports through a universal consumer header are rejected by
  [Headers and linkage](cpp/HEADERS_AND_LINKAGE.md).
- Referents: [CMakeLists.txt](../CMakeLists.txt) and [Authors](../Authors.md).
- Decision basis/status: header-only, C++20 and zero core dependencies are recorded
  owner direction; optional integration separation is an adopted technical decision,
  governing consumer and integration boundaries.
- Verification/limits: target definitions were inspected during adoption. This is
  not a new external-consumer build or proof of every optional backend.

## FP-A02 Header ownership

**Binding statement:** flat public component headers may forward to owned nested
implementation headers; each definition has one canonical source and public headers
must compose under the supported configuration.

- Scope: include/fat_p, especially nested implementations such as tensor.
- Operational test: compile each public header alone, combine and reverse related
  header orders, and run multi-TU identity/link tests for shared definitions.
- Consequence: duplicate definitions or implicit prerequisites make otherwise valid
  consumer includes fail or create distinct state across translation units.
- Rationale: a component facade preserves discoverability without duplicating its
  implementation. The include-all test fixture is tooling, not a consumer API.
- Referents: [Tensor.h](../include/fat_p/Tensor.h),
  [tensor](../include/fat_p/tensor), [Header hygiene workflow](../.github/workflows/header-hygiene.yml).
- Decision basis/status: adopted technical ownership/layout decision reflected in
  source at 9ff6d855f5ce5d949387586fad9e74cc5e121349, retained 2026-09-06.
- Verification/limits: historical Tensor and full CI are linked in the current
  verification record; all-order and all-configuration composition is not implied.

## FP-A03 Layer boundaries

**Binding statement:** production header dependencies follow
Foundation < Containers < Concurrency < Domain < Integration < Testing and may
depend only on their own or lower layer. Build/tool metadata's Infrastructure
classification is outside this C++ dependency order.

- Scope: includes between authored library headers; do not infer a dependency from
  documentation examples or third-party internals.
- Operational test: resolve quoted includes with sibling precedence and the public
  include root, then compare the actual owners' layers. Run
  [validate_layers.py](../tools/validate_layers.py) and inspect unresolved edges.
- Consequence: higher-layer dependencies force unwanted feature or harness coupling.
- Rationale: low-level facilities remain usable independently. The reversed table in
  older guidance is retired; Foundation is below Containers.
- Referents: [validate_layers.py](../tools/validate_layers.py) and
  [Expected.h](../include/fat_p/Expected.h).
- Decision basis/status: adopted technical taxonomy reflected in the source and
  dependency checker; no claim of separate human authorship.
- Verification/limits: the scanner depends on truthful metadata and has lexical
  resolution limits. Unknown or conditional edges require review; a green scanner
  alone does not establish the full architecture.

## FP-A04 Feature configuration authorities

**Binding statement:** language/library, platform, SIMD and general configuration
decisions use their existing owners instead of per-component competing probes.

- Scope: compile-time feature selection and exported configuration controls.
- Operational test: trace a feature macro to its owner and exercise enabled/disabled
  supported configurations; inspect changed components for duplicated probes.
- Consequence: inconsistent feature decisions across translation units can change
  definitions, ABI or backend availability.
- Rationale/referents: [CppFeatureDetection.h](../include/fat_p/CppFeatureDetection.h),
  [PlatformDetection.h](../include/fat_p/PlatformDetection.h),
  [SimdDetection.h](../include/fat_p/SimdDetection.h), and
  [FatPConfig.h](../include/fat_p/FatPConfig.h). Their scopes are routed in
  [project rules](project/FATP_RULES.md).
- Decision basis/status: technical separation reflected in the configuration
  owners listed above.
- Verification/limits: source ownership inspected; no new cross-platform or hardware
  support result is established by source ownership alone.

