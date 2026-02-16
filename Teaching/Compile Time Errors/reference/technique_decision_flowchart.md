# Compile-Time Safety: Technique Decision Flowchart

## Visual Guide to Choosing the Right Technique

---

## How to Use This Guide

Start with your problem. Follow the flowchart based on your specific situation. The endpoints tell you which technique to use and which session covers it in detail.

---

## Master Decision Flowchart

```mermaid
flowchart TD
    START([What kind of bug<br/>are you preventing?]) --> TYPE{Type confusion?}
    
    TYPE -->|Yes| TYPE_Q["IDs? Units? States?"]
    TYPE -->|No| ENUM
    
    TYPE_Q --> TYPE_ID{IDs or handles?}
    TYPE_Q --> TYPE_UNIT{Physical units?}
    TYPE_Q --> TYPE_STATE{Object states?}
    
    TYPE_ID -->|Yes| S1["<b>Session 1: StrongId</b><br/>Zero-cost type distinction"]
    
    TYPE_UNIT -->|Yes| S11["<b>Session 11: Physical Units</b><br/>Dimensional analysis"]
    
    TYPE_STATE -->|Yes| STATE_Q{State known<br/>at compile time?}
    STATE_Q -->|"Yes, linear flow"| S4["<b>Session 4: Type-State</b><br/>State in the type"]
    STATE_Q -->|"No, runtime state"| S3["<b>Session 3: StateMachine</b><br/>Validated transitions"]
    
    ENUM{Missing case<br/>in dispatch?} -->|Yes| ENUM_Q{Enum or variant?}
    ENUM -->|No| NULL
    
    ENUM_Q -->|Enum| S2["<b>Session 2: -Werror=switch-enum</b><br/>Exhaustive switches"]
    ENUM_Q -->|std::variant| S9["<b>Session 9: overloaded{}</b><br/>Exhaustive visitors"]
    
    NULL{Null pointer<br/>dereference?} -->|Yes| NULL_Q{Null ever valid?}
    NULL -->|No| RETURN
    
    NULL_Q -->|Never| S6A["<b>Session 6: T& reference</b><br/>Cannot be null"]
    NULL_Q -->|Sometimes| S6B["<b>Session 6: std::optional</b><br/>Explicit nullability"]
    
    RETURN{Ignored<br/>return value?} -->|Yes| S7["<b>Session 7: [[nodiscard]]</b><br/>Force handling"]
    RETURN -->|No| MUTATION
    
    MUTATION{Accidental<br/>mutation?} -->|Yes| S5["<b>Session 5: const</b><br/>Immutability guarantee"]
    MUTATION -->|No| TEMPLATE
    
    TEMPLATE{Template<br/>type error?} -->|Yes| TEMPLATE_Q{C++ standard?}
    TEMPLATE -->|No| BUILDER
    
    TEMPLATE_Q -->|C++17| S8A["<b>Session 8: SFINAE</b><br/>enable_if constraints"]
    TEMPLATE_Q -->|C++20+| S8B["<b>Session 8: Concepts</b><br/>Clear constraints"]
    
    BUILDER{Incomplete<br/>builder?} -->|Yes| S10["<b>Session 10: Type Accumulation</b><br/>Phantom type states"]
    BUILDER -->|No| MINI["See mini-sessions"]
    
    style S1 fill:#90EE90
    style S2 fill:#90EE90
    style S3 fill:#90EE90
    style S4 fill:#90EE90
    style S5 fill:#90EE90
    style S6A fill:#90EE90
    style S6B fill:#90EE90
    style S7 fill:#90EE90
    style S8A fill:#90EE90
    style S8B fill:#90EE90
    style S9 fill:#90EE90
    style S10 fill:#90EE90
    style S11 fill:#90EE90
```

---

## Mini-Session Decision Flowchart

For smaller, focused techniques:

```mermaid
flowchart TD
    MINI([Other compile-time<br/>safety needs?]) --> NARROW{Narrowing<br/>conversion?}
    
    NARROW -->|Yes| M2["<b>Mini 2: Brace Init</b><br/>int x{value};"]
    NARROW -->|No| DELETE
    
    DELETE{Prevent unwanted<br/>operation?} -->|Yes| M1["<b>Mini 1: = delete</b><br/>Explicit prohibition"]
    DELETE -->|No| ASSERT
    
    ASSERT{Compile-time<br/>assumption check?} -->|Yes| M3["<b>Mini 3: static_assert</b><br/>Compile-time assertions"]
    ASSERT -->|No| EXCEPT
    
    EXCEPT{Exception<br/>safety contract?} -->|Yes| M4["<b>Mini 4: noexcept</b><br/>No-throw guarantee"]
    EXCEPT -->|No| BOUNDS
    
    BOUNDS{Buffer bounds<br/>safety?} -->|Yes| M5["<b>Mini 5: std::span</b><br/>Pointer+size bundle"]
    BOUNDS -->|No| INHERIT
    
    INHERIT{Prevent<br/>inheritance?} -->|Yes| M6["<b>Mini 6: final</b><br/>No subclassing"]
    INHERIT -->|No| DONE([No technique needed<br/>or see handbook])
    
    style M1 fill:#87CEEB
    style M2 fill:#87CEEB
    style M3 fill:#87CEEB
    style M4 fill:#87CEEB
    style M5 fill:#87CEEB
    style M6 fill:#87CEEB
```

---

## Specific Problem Flowcharts

### Pointer vs Reference Decision

```mermaid
flowchart TD
    A[Need to pass/return<br/>handle to object] --> B{Can it ever<br/>be absent?}
    
    B -->|No, always exists| C{Need reassignment?}
    C -->|No| D["<b>T&</b> reference"]
    C -->|Yes| E{C interop?}
    E -->|No| F["<b>gsl::not_null&lt;T*&gt;</b>"]
    E -->|Yes| G["<b>T*</b> with assert at boundary"]
    
    B -->|Yes, might be absent| H{Who owns the object?}
    H -->|Caller owns, I'm borrowing| I{Need to express<br/>nullability clearly?}
    I -->|Yes| J["<b>std::optional&lt;T*&gt;</b><br/>or<br/><b>optional&lt;reference_wrapper&lt;T&gt;&gt;</b>"]
    I -->|No| K["<b>T*</b> (documented nullable)"]
    
    H -->|I should own| L["<b>std::optional&lt;T&gt;</b>"]
    H -->|Shared ownership| M["<b>std::shared_ptr&lt;T&gt;</b><br/>(inherently nullable)"]
    
    style D fill:#90EE90
    style F fill:#90EE90
    style L fill:#90EE90
    style J fill:#FFFFE0
    style K fill:#FFB6C1
```

### Error Handling Decision

```mermaid
flowchart TD
    A[Function can fail] --> B{Can caller<br/>recover?}
    
    B -->|No, should propagate| C{Performance<br/>critical?}
    C -->|No| D["<b>Exceptions</b><br/>throw on error"]
    C -->|Yes| E["<b>Expected&lt;T,E&gt;</b><br/>with terminate policy"]
    
    B -->|Yes, caller handles| F{Need rich<br/>error info?}
    F -->|Yes| G["<b>Expected&lt;T, ErrorType&gt;</b>"]
    F -->|No| H{Just success/fail?}
    H -->|Yes| I["<b>[[nodiscard]] bool</b>"]
    H -->|Need absence, not error| J["<b>std::optional&lt;T&gt;</b>"]
    
    style D fill:#87CEEB
    style E fill:#90EE90
    style G fill:#90EE90
    style I fill:#FFFFE0
    style J fill:#FFFFE0
```

### State Machine Decision

```mermaid
flowchart TD
    A[Object has multiple<br/>states with transitions] --> B{How many states?}
    
    B -->|2-3, simple| C{State known<br/>at compile time?}
    C -->|Yes| D["<b>Type-State Pattern</b><br/>ClosedFile → OpenFile"]
    C -->|No| E["Simple enum + switch<br/>with -Werror=switch-enum"]
    
    B -->|4+, complex| F{Need entry/exit<br/>actions?}
    F -->|Yes| G["<b>StateMachine&lt;...&gt;</b>"]
    F -->|No| H{Invalid transitions<br/>are security risk?}
    H -->|Yes| G
    H -->|No| I["enum + switch<br/>or simple State pattern"]
    
    style D fill:#90EE90
    style G fill:#90EE90
    style E fill:#FFFFE0
    style I fill:#FFFFE0
```

---

## Technique Comparison Table

| Technique | Compile-Time? | Runtime Cost | Complexity | Best For |
|-----------|--------------|--------------|------------|----------|
| StrongId | ✅ Yes | Zero | Low | IDs, handles |
| -Werror=switch-enum | ✅ Yes | Zero | Low | Enum dispatch |
| StateMachine | ⚠ Partial | O(1) check | Medium | Complex protocols |
| Type-State | ✅ Yes | Zero | High | Linear workflows |
| const | ✅ Yes | Zero | Low | All code |
| T& reference | ✅ Yes | Zero | Low | Required params |
| std::optional | ⚠ Runtime | Tiny | Low | Nullable values |
| [[nodiscard]] | ✅ Yes | Zero | Low | Return values |
| Concepts | ✅ Yes | Zero | Medium | Templates |
| SFINAE | ✅ Yes | Zero | High | C++17 templates |

---

## Quick Decision Rules

### Rule 1: Type Confusion
> "If two things shouldn't be interchangeable, give them different types."

- Different IDs → `StrongId<DifferentTag>`
- Different units → Units library
- Different states → Different types or StateMachine

### Rule 2: Nullability
> "If null is not valid, use a type that can't be null."

- Never null → `T&`
- Maybe null, you own it → `std::optional<T>`
- Maybe null, borrowed → `T*` (documented) or `optional<reference_wrapper<T>>`

### Rule 3: Error Handling
> "If ignoring is a bug, make ignoring impossible."

- Must check return → `[[nodiscard]]`
- Must handle error → `Expected<T, E>`
- Can't check at runtime → Make invalid states unrepresentable

### Rule 4: Enum Safety
> "Never write `default:` in a switch on your own enums."

- Your enum → No default, use `-Werror=switch-enum`
- External enum → `default:` is OK
- std::variant → Use `overloaded{}` pattern

### Rule 5: Templates
> "Constrain at the interface, not deep inside."

- C++20 → Concepts
- C++17 → SFINAE with enable_if
- Error at call site, not in implementation

---

## See Also

| For This Topic | See This Document |
|----------------|-------------------|
| Full technique tutorials | `problem_session_01.md` – `problem_session_11.md` |
| Quick patterns | `quick_reference_card.md` |
| Compiler flags | `compiler_flags_reference.md` |
| Design philosophy | `compile_time_error_detection_overview.md` |

---

*FAT-P Library — Technique Decision Flowchart v1.0*
