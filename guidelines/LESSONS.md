# Project lessons index

The mandatory assistant-accountability record is [DEMERITS](DEMERITS.md), read
during onboarding. This index and the historical cases below are task references;
they do not replace its categories, attribution or counts.

| Decision or failure distinction | Case, evidence, or decision boundary | Owning rule and when to read the case |
|---|---|---|
| An optimization must preserve the component's promised stability, separately from improving benchmark timing | [StableHashMap precedent](../postmortems/Conversation_Record_2026-02-13.md#stablehashmap-the-crucible) | [Engineering](modules/ENGINEERING.md), [semantic names](project/FATP_RULES.md#project-semantic-names); consult when an optimization would change guarantees |
| Optional competitor build costs and runtime requirements remain real even when build artifacts are cached | [Folly CI lessons](../postmortems/folly-ci-postmortem.md#lessons-learned) | [Dependency ownership](ARCHITECTURE.md#fp-a01-dependency-ownership), [workflow ownership](project/FATP_RULES.md#workflow-ownership); consult for benchmark dependency/cache changes |
| Elapsed-time measurement must survive wall-clock adjustment | A wall-clock adjustment can produce a false elapsed interval | [Benchmarking measurement machinery](modules/BENCHMARKING.md#5-measurement-honesty-machinery); consult when selecting or changing a timer |
| Isolated container operations can miss defects caused by their sequence | A mixed insert/erase/reserve sequence can expose failures that isolated calls miss | [Container operation sequences](cpp/TESTING.md#container-operation-sequences); consult for container test coverage |
| Existing CI jobs do not automatically establish coverage for a newly added component | A new workflow can omit instrumentation despite an existing green test matrix | [Required sanitizer coverage](project/FATP_RULES.md#component-sanitizer-coverage); consult for component CI changes |

The cases preserve reasoning and provenance. Their old measurements, package
versions, reviewer verdicts and scores are not current evidence. Recheck actual
referents before relying on them. Active rules live at the linked canonical homes;
a historical narrative does not create a new instruction or event-log requirement.

