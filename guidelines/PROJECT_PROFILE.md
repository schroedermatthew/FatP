# Project profile

This is FatP's canonical configuration record. It cannot override CORE, DEMERITS
or mandatory C++ STYLE. Command records describe procedures; validators never
execute them. Current observations belong in [CURRENT_VERIFICATION](CURRENT_VERIFICATION.md).

<!-- project-profile -->
```json
{
  "schema_version": 2,
  "status": "instantiated",
  "identity": {
    "name": "FatP",
    "purpose": "Header-only C++ utilities for high-performance and scientific software.",
    "owner": "Matthew Schroeder",
    "namespace": "fat_p",
    "macro_prefix": "FATP"
  },
  "languages": [
    "cpp",
    "c",
    "cuda",
    "python",
    "cmake",
    "powershell",
    "shell",
    "batch"
  ],
  "cpp": {
    "standard": 20,
    "layout": "header-only",
    "harness": "FatPTest.h custom harness, with ENABLE_TEST_APPLICATION standalone runners and discovery through cmake/FatPComponents.cmake. Intended test-only use; current public-header exposure is recorded as a conformance gap, not an approved exception.",
    "exception_policy": "Throw documented typed exceptions for structural violations by default; retain explicit component error-result contracts (Expected and policy-based errors). No repository-wide exception-disabled build contract is adopted.",
    "toolchains": [
      "Linux x86-64: GCC 12/13 with libstdc++, C++20; GCC 14 with libstdc++, C++23; see .github/workflows/fatp-test.yml.",
      "Linux x86-64: Clang 16 with libstdc++, C++20; Clang 17 with libstdc++, C++23; component workflows also exercise newer versions.",
      "Windows x64: MSVC with the Microsoft STL, C++20 and the configured C++23/latest mode; x86 MSVC is rejected by CMake.",
      "Windows x86-64 local: MSYS2 UCRT64 GCC 16.1 and Clang 22.1.8 using libstdc++/UCRT; use quoted FatP include search to avoid Signal.h shadowing the C header.",
      "Formatting and AST tooling: clang-format 22.1.8 and Clang 22.1.8; Python 3.10+ with tools/requirements.txt. These tool versions do not replace the product compiler matrix.",
      "Guidelines CI: Ubuntu24.04 Clang18 for synthetic AST controls, clang-format22.1.8 from its pinned Python distribution; this is a tooling check, not product matrix coverage."
    ],
    "authored_globs": [
      "include/fat_p/**/*.h",
      "components/**/*.h",
      "components/**/*.cpp",
      "Teaching/**/*.h",
      "Teaching/**/*.cpp"
    ],
    "excluded_globs": [],
    "exclusion_reason": "Positive globs select owned C++ including tests, examples and frozen authored teaching copies. ThirdParty is vendor-owned; build/output directories are generated. Teaching/GPU .cu and .c sources need specialist checks outside the style tool's supported extensions; they remain authored and are listed in the conformance report.",
    "component_headers_file": "guidelines/data/component_headers.json"
  },
  "build": {
    "ci": "live",
    "support_contract": "Existing .github/workflows/fatp-test.yml and component workflows define the supported product jobs. Preserve their actual compiler/library/architecture and warning settings; a green compiler does not establish all backends or optional hardware. guidelines.yml adds corpus and checker gates. These tooling gates do not establish ARM/GPU hardware, package ABI, libc++ or exception-disabled support.",
    "commands": [
      {
        "name": "guidelines",
        "command": "python tools/lint_guidelines.py guidelines --repo-root . --instantiated",
        "cwd": ".",
        "property": "Corpus routes, profile schema, links, formatter integrity and current ledger arithmetic.",
        "when": "Guidelines/profile/formatter changes; when editing the ledger also supply --previous-ledger with its pre-change copy.",
        "evidence": "Exit status plus complete diagnostic output; CURRENT_VERIFICATION records measured runs."
      },
      {
        "name": "guideline-tool-tests",
        "command": "python -m unittest discover -s tools/tests -v",
        "cwd": ".",
        "property": "Checker positive and negative controls with real formatter/compiler executables.",
        "when": "Tooling changes and guidelines CI; FATP_GUIDELINE_FORMATTER and FATP_GUIDELINE_CLANG select actual tools.",
        "evidence": "Unittest case discovery and result output, including tool availability."
      },
      {
        "name": "metadata",
        "command": "python tools/lint_metadata.py --repo-root . --guidelines guidelines",
        "cwd": ".",
        "property": "Strict schema, metadata coverage, placement and computed supported hygiene fields.",
        "when": "Changes to covered code or metadata and full conformance inventories.",
        "evidence": "Nonzero findings remain failures; report distinguishes historical drift from introduced violations."
      },
      {
        "name": "style-windows",
        "command": "python tools/check_style.py --repo-root . --guidelines guidelines --inventory -- -I include -iquote include/fat_p -DENABLE_TEST_APPLICATION",
        "cwd": ".",
        "property": "Full supported-extension authored C++ lexical/format/AST checks for the configured Windows GNU frontend.",
        "when": "Every authored C++ delivery requires the full inventory under modules/WORKFLOW.md. Use --file only for focused feedback, not as a substitute for that gate; supply actual per-target definitions.",
        "evidence": "Record every file and parse/timeout limit; this does not replace builds or semantic review."
      },
      {
        "name": "style-linux",
        "command": "python tools/check_style.py --repo-root . --guidelines guidelines --inventory -- -I include -I include/fat_p -DENABLE_TEST_APPLICATION",
        "cwd": ".",
        "property": "Same mandatory style inventory under Linux Clang with libstdc++.",
        "when": "Full inventory for authored C++ delivery under modules/WORKFLOW.md in this configuration; --file provides focused feedback only. Supply real target/backend flags where needed.",
        "evidence": "Diagnostic output and exit status; intentional negative fixtures and unavailable backends remain explicit."
      },
      {
        "name": "configure-tests",
        "command": "cmake -S . -B build/guidelines-tests -DFATP_BUILD_TESTS=ON -DFATP_BUILD_BENCHMARKS=OFF -DCMAKE_BUILD_TYPE=Release",
        "cwd": ".",
        "property": "Configure the existing CMake test targets with the environment's selected supported compiler.",
        "when": "C++ changes; on Windows run from the configured x64 toolchain shell or supply a supported generator/compiler.",
        "evidence": "Configuration output only; no claim of compilation."
      },
      {
        "name": "build-tests",
        "command": "cmake --build build/guidelines-tests --config Release --parallel 2",
        "cwd": ".",
        "property": "Compile and link discovered test targets.",
        "when": "After configuring the matching build tree for C++ changes.",
        "evidence": "Build output and exit status."
      },
      {
        "name": "run-tests",
        "command": "ctest --test-dir build/guidelines-tests -C Release --output-on-failure",
        "cwd": ".",
        "property": "Execute registered tests.",
        "when": "After a successful corresponding build.",
        "evidence": "Discovered/selected test counts and failures; do not infer all configurations."
      },
      {
        "name": "layers",
        "command": "python tools/validate_layers.py",
        "cwd": ".",
        "property": "Lexical layer/dependency inventory across nested public and implementation headers.",
        "when": "Header ownership/include changes; review unresolved and conditional edges.",
        "evidence": "Scanner output; not complete semantic architecture proof."
      },
      {
        "name": "legacy-metadata",
        "command": "python tools/fatp_meta_inventory.py --json-out meta_report.json --print-missing --print-path-mismatch --print-schema-fail --print-layout-fail --print-version-bad --fail-on-issues",
        "cwd": ".",
        "property": "Existing FatP metadata CI contract during adoption.",
        "when": "Covered repository changes while legacy tooling remains in service.",
        "evidence": "Existing scanner result is separate from the stricter new metadata gate."
      }
    ]
  },
  "compatibility": {
    "stage": "pre-release",
    "policy": "no-shims"
  },
  "metadata": {
    "enabled": true,
    "reason": "Continue FatP's FATP_META inventories and adopt the strict template schema for owned comment-compatible code/tooling. Existing schema/layout/hygiene drift is reported for remediation; enabled does not mean fully conformant.",
    "authored_globs": [
      "include/fat_p/**/*.h",
      "components/**/*.h",
      "components/**/*.cpp",
      "Teaching/**/*.h",
      "Teaching/**/*.cpp",
      "Teaching/**/*.cu",
      "Teaching/**/*.c",
      "tools/**/*.py",
      "tools/*.ps1",
      "tools/*.sh",
      "tools/*.bat",
      "tools/*.cmake",
      "cmake/*.cmake",
      "CMakeLists.txt"
    ],
    "excluded_globs": [],
    "exclusion_reason": "Vendor files, generated build/output artifacts, Markdown/JSON data, YAML workflows, requirements and formatter configuration are outside this code-comment metadata surface. The teaching C/CUDA source remains covered by metadata even where Clang C++ style cannot inspect it.",
    "file_roles": [
      "public_header",
      "internal_header",
      "test",
      "header_self_contained_test",
      "benchmark",
      "example",
      "port_module",
      "tooling",
      "build_script"
    ],
    "layers": [
      "Foundation",
      "Containers",
      "Concurrency",
      "Domain",
      "Integration",
      "Testing",
      "Infrastructure",
      "Examples"
    ]
  },
  "peer_routes": [],
  "local_rules": [
    {
      "path": "guidelines/project/FATP_RULES.md",
      "scope": "FatP source, test, build, benchmark and metadata work; read applicable sections with the owning generic module.",
      "owner": "FatP maintainer; adopted technical rules maintained by contributors within project authority."
    },
    {
      "path": "guidelines/project/TEACHING_TYPES.md",
      "scope": "Authoring or revising FatP teaching documents and their front matter; read with modules/TEACHING.md.",
      "owner": "FatP maintainer; adopted documentation conventions."
    }
  ]
}
```
<!-- /project-profile -->

## Additional decisions

- [PROJECT_PHILOSOPHY](PROJECT_PHILOSOPHY.md) records direction and recurring choices;
  [ARCHITECTURE](ARCHITECTURE.md) owns dependency, header and configuration invariants.
- The project remains pre-release. No compatibility shims or aliases are introduced
  to preserve renamed pre-release interfaces; update affected users under the actual
  task scope. No released ABI or serialization compatibility promise is invented.
- Distribution is header-only under [MIT](../LICENSE). Optional external integrations
  and benchmark competitors have separate dependency boundaries. Check their actual
  licenses when introducing or redistributing them.
- [FatP rules](project/FATP_RULES.md) own feature-header responsibilities, component
  discovery, benchmark policy/configurations and the metadata checks.
  [Teaching types](project/TEACHING_TYPES.md) own the established document taxonomy.
- [Project semantic names](project/FATP_RULES.md#project-semantic-names) is the additional naming decision record required by C++ STYLE. [Benchmark defaults and output locations](project/FATP_RULES.md#benchmark-defaults-and-output-locations) record the actual helper/workflow configurations.
- Formatter selection is file-based with the root [.clang-format](../.clang-format).
  Use actual compiler include paths and defines in style checks; the Windows GNU
  command deliberately uses quoted search for include/fat_p.
- No external model bridge is configured as a portable repository capability.
  Session tools and prior peer results do not grant a new endpoint or write authority.
- Commit, push, merge and release actions follow explicit session authorization and
  [Workflow](modules/WORKFLOW.md). Guidelines do not grant publishing permission.

## Inventory boundaries

The [corresponding-header data](data/component_headers.json) is the single mapping
used by the validators. Its schema-2 reference above is relative to the repository
root. It is not part of mandatory onboarding: inspect the relevant entries when
working on mapped sources or changing the inventory. Validators load and check
the complete file automatically, including during focused checks.

The profile inventories all owned C++ headers and sources in include, components
and Teaching using supported checker extensions. The corresponding-header mapping
is reviewed from each source's actual production dependencies, not inferred from an
already incorrect first include. Multi-component, aggregate and deliberately reversed
include fixtures without a single first-header contract are enumerated in the
[conformance report](reports/CPP_CONFORMANCE.md).

ThirdParty and generated build artifacts are outside authored policy. Frozen
authored teaching copies remain included. C and CUDA teaching sources are included
in metadata coverage but need separate compiler/style review; a C++ AST pass does
not establish their conformance.

Strict metadata adoption retains FatP's actual roles and six production dependency
layers, plus the existing Infrastructure tooling and Examples classifications.
Accepted enum membership is not proof that an individual file's classification is
correct. Legacy extra keys and unsupported hygiene fields are reported, not silently
accepted by the new checker. Existing tooling remains a separately named gate until
its contract is fully superseded; its pass does not mean the strict gate passed.






