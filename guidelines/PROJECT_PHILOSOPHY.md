# Project philosophy

FatP is a C++20 header-only library for HPC and scientific software. Identity and
configuration live in [PROJECT_PROFILE](PROJECT_PROFILE.md); structural commitments
live in [ARCHITECTURE](ARCHITECTURE.md). This compact judgment record cannot override
CORE or the owning rules.

## Direction and boundaries

The maintainer supplies direction and judgment; AI authors the implementation,
architecture and operational guidance within that direction. The actual provenance
is recorded in [Authors](../Authors.md). Agreement among assistants is evidence to
examine, not owner ratification.

FatP is not a standard-library polyfill, a compiled framework, or a vehicle for
forcing optional third-party dependencies on core users. Header-only and C++20 are
owner-directed constraints. Current license and contribution terms are in
[LICENSE](../LICENSE) and [CONTRIBUTING](../CONTRIBUTING.md).

## Decision priorities

| When this choice arises | Priority and why | Canonical rule or actual decision source |
|---|---|---|
| Convenience would widen every consumer's dependency set | Preserve component independence; optional integration costs belong at the integration boundary | [Dependency ownership](ARCHITECTURE.md#fp-a01-dependency-ownership) |
| A common utility already exists | Inspect and use its actual contract before creating a competing implementation; local duplication drifts | [Engineering](modules/ENGINEERING.md), [feature authorities](project/FATP_RULES.md) |
| A customization parameter has no concrete user | Prefer the smaller interface; policy axes need distinct use cases and costs | [C++ authoring](cpp/AUTHORING.md) |
| A pre-release change would otherwise need a compatibility alias | Update the canonical API and affected users coherently under the registered no-shims policy | [Project profile](PROJECT_PROFILE.md#additional-decisions), [CORE](CORE.md) |
| A benchmark favors FatP only under selected conditions | Explain mechanisms, alternatives and losing regimes; measurable contracts take priority over marketing claims | [Benchmarking](modules/BENCHMARKING.md), [project rules](project/FATP_RULES.md) |
| Existing source differs from newly adopted style | Identify the drift and follow the authorized change scope; new work must use the mandatory guide | [Style adoption](cpp/STYLE.md#enforcement-and-adoption) |

## Decisive precedents

| Situation | Accepted/rejected distinction and litmus test | Owning rule or decision |
|---|---|---|
| Public facade over nested implementation headers | One owned definition behind a component facade is compatible with header-only delivery; a duplicate implementation or an unrelated include-all consumer facade is not | [Header ownership](ARCHITECTURE.md#fp-a02-header-ownership) |
| Language feature versus standard-library availability | Use guaranteed C++20 directly; gate a real platform, SIMD or supported-library gap through its actual configuration owner | [Project rules](project/FATP_RULES.md), [C++ authoring](cpp/AUTHORING.md) |
| Runtime success alongside structural drift | A passing test does not verify include composition, layout identity or a dependency boundary; exercise the specific invariant | [Architecture](ARCHITECTURE.md), [Workflow](modules/WORKFLOW.md) |

## Unresolved choices

Existing code gaps and their evidence are in the
[conformance report](reports/CPP_CONFORMANCE.md). The mandatory rules remain active;
remediation follows the authorized scope of each task.

Any binary ABI, package-release or accelerator support promise must define and
verify the actual consumer configuration. Continue unrelated work within existing
authority.

