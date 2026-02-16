---
doc_id: MG-STATEMACHINE-001
doc_type: "Migration Guide"
title: "Switch Statements to Type-Safe State Machines"
from_pattern: "switch on enum, scattered state transitions, state flags"
to_component: "StateMachine"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Medium"
breaking_changes: true
last_verified: "2025-01-08"
fatp_components: ["StateMachine"]
topics: ["c-to-cpp", "migration", "state-machines", "switch-statements", "type-state", "compile-time-transitions"]
constraints: ["missing transitions", "invalid states", "entry-exit guarantees", "compile-time validation"]
audience: ["C developers", "C++ developers", "AI assistants"]
status: "draft"
---

# Migration Guide - Switch Statements to Type-Safe State Machines

### *From `switch(state)` Spaghetti to Compile-Time Verified Transitions*

*FAT-P Library — January 2025*

---

## Scope

This guide targets C code that implements state machines via `switch(state)` on enum values, scattered state transitions, and state flags, and migrates those to `StateMachine<Context, TransitionList>` with compile-time transition validation.

## Not covered

- Hierarchical (HSM) or nested state machines
- UML statechart diagram-to-code generation
- Runtime-configurable state machines (dynamic transition tables)

## Prerequisites

- Familiarity with enum-based state machines in C
- Understanding of entry/exit actions and transition guards

## Migration Guide Card

**From:** `switch(state)` on enum, scattered state transitions, state flags  
**To:** `StateMachine<Context, TransitionList, Policy...>` with compile-time transition validation  
**Why migrate:** Switch-based state machines cannot enforce valid transitions at compile time; missing cases are silent bugs  
**Compatibility strategy:** Restructure — requires converting enum states to state types and transitions to a transition list  
**Mechanical steps:**
1. Identify `switch(state)` blocks and enumerate all states and transitions.
2. Define state types and a `TransitionList` encoding valid transitions.
3. Replace switch logic with state entry/exit handlers and transition guards.
4. Verify compile-time rejection of invalid transitions.
**Behavioral equivalence:** Same states, same transitions, same entry/exit side effects  
**Intentional differences:** Invalid transitions are compile-time errors; entry/exit actions are guaranteed to execute  
**Failure model:** Invalid transition → compile error (not runtime check)  
**Threading model:** Unchanged — state machine itself is not synchronized; external locking if shared  
**Lifetime model:** StateMachine owns the current state; context must outlive the machine  
**Alternatives:** Boost.MSM, Boost.SML, manual variant-based state machines  
**Verification:** Compile-time verification of transition validity; unit tests for state sequences and guard conditions  
**Rollback plan:** Replace state types with enum; replace transition list with `switch(state)` blocks

---

## Alternatives

Boost.MSM (heavy, macro-based), Boost.SML (lighter, C++14), `std::variant`-based state machine (manual, no compile-time transition check), coroutine-based state machines (C++20, different model).

## Mapping: From → To

| C Pattern | C++ Replacement | Notes |
|-----------|----------------|-------|
| `enum State { IDLE, ... };` | State types: `struct Idle {};` | Each state is a distinct type |
| `switch(state) { case IDLE: ... }` | `TransitionList` template parameter | Compiler verifies all transitions exist |
| `state = CONNECTED;` (direct assign) | `machine.transition<Connected>()` | Invalid transitions are compile errors |
| `if (state == X && cond)` guard | Guard function in transition definition | Guards are part of the transition declaration |

## Compatibility and ABI boundaries

No ABI concerns — state machines are typically internal. If state must cross a C API boundary, provide an `enum` accessor that maps the current type-state to a C-compatible enum value.

## Lifetime and ownership model

`StateMachine` owns the current state. The `Context` (user data) must outlive the machine. State entry/exit actions execute during transitions. Machine destruction runs the current state's exit action.

## Thread-safety and reentrancy

Not internally synchronized. If the state machine is accessed from multiple threads, external locking is required around transitions. State queries (`is<State>()`) are read-only but not atomic with respect to concurrent transitions.

## Error and failure model

Invalid transitions are compile-time errors (not runtime). Guard failures return `false` from `transition()` — the machine stays in the current state. No exceptions from the transition mechanism itself.

## Rollback plan

Replace state types with enum values. Replace `TransitionList` with `switch(state)` blocks. Restore direct state assignment. Compile-time transition validation is lost on rollback.

## Table of Contents

1. [The Problem with Switch-Based State Machines](#the-problem-with-switch-based-state-machines)
2. [Real-World State Machine Disasters](#real-world-state-machine-disasters)
3. [The C Patterns](#the-c-patterns)
4. [The StateMachine Solution](#the-statemachine-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When StateMachine Loses](#when-statemachine-loses)

---

## The Problem with Switch-Based State Machines

State machines are everywhere: parsers, protocol handlers, UI controllers, game logic, connection managers. The common C/C++ implementation:

```c
enum State { IDLE, CONNECTING, CONNECTED, DISCONNECTING, ERROR };
State state = IDLE;

void handle_event(Event event) {
    switch (state) {
        case IDLE:
            if (event == CONNECT_REQUEST) {
                state = CONNECTING;
                start_connection();
            }
            break;
        case CONNECTING:
            if (event == CONNECT_SUCCESS) {
                state = CONNECTED;
                on_connected();
            } else if (event == TIMEOUT) {
                state = ERROR;
                log_error("Connection timeout");
            }
            // What about DISCONNECT during CONNECTING? Forgot to handle it.
            break;
        case CONNECTED:
            // ... 50 more lines ...
            break;
        // ... more cases ...
    }
}
```

**The problems accumulate:**

| Problem | Consequence |
|---------|-------------|
| Missing case | Compiler warning (if lucky), undefined behavior |
| Missing transition | Silent bug—event ignored |
| Forgotten entry action | Resource not initialized |
| Forgotten exit action | Resource leaked |
| Invalid transition | No compile-time check |
| State scattered | Logic spread across file |
| Testing | Must simulate every path |

---

## Real-World State Machine Disasters

### The TCP State Machine

TCP has 11 states with complex transitions. The [RFC 793](https://tools.ietf.org/html/rfc793) state diagram is infamous. Every TCP implementation has had bugs from:

- Missing transitions (FIN during SYN_SENT)
- Race conditions between states
- Resource leaks on unexpected transitions

The Linux kernel's TCP implementation has had dozens of state-related CVEs over the years.

### SQLite's Parser States

SQLite's parser uses state machines for tokenizing and parsing SQL. From the code structure, states are managed with switches and explicit state variables:

```c
/* Simplified from SQLite's tokenizer */
int sqlite3GetToken(const unsigned char *z, int *tokenType){
  int i, c;
  switch( *z ){
    case ' ': case '\t': case '\n': case '\f': case '\r': {
      for(i=1; sqlite3Isspace(z[i]); i++){}
      *tokenType = TK_SPACE;
      return i;
    }
    case '-': {
      if( z[1]=='-' ){
        /* Comment state - but what if we're already in a string? */
        for(i=2; (c=z[i])!=0 && c!='\n'; i++){}
        *tokenType = TK_SPACE;
        return i;
      }
      /* ... */
    }
    /* ... 30+ more cases ... */
  }
}
```

### The Connection Manager Anti-Pattern

```c
/* Real code from a production system */
typedef enum {
    CONN_IDLE,
    CONN_CONNECTING,
    CONN_AUTHENTICATING,
    CONN_CONNECTED,
    CONN_RECONNECTING,
    CONN_DISCONNECTING,
    CONN_FAILED,
    CONN_CLOSED
} ConnState;

ConnState g_state = CONN_IDLE;

void on_socket_event(SocketEvent event) {
    switch (g_state) {
        case CONN_CONNECTING:
            if (event == SOCKET_CONNECTED) {
                g_state = CONN_AUTHENTICATING;
                send_auth();
            } else if (event == SOCKET_ERROR) {
                g_state = CONN_FAILED;
                // Forgot to close socket!
            }
            break;
        case CONN_AUTHENTICATING:
            if (event == SOCKET_DATA) {
                if (parse_auth_response()) {
                    g_state = CONN_CONNECTED;
                    // Forgot to notify listeners!
                }
            }
            // What if SOCKET_CLOSED during auth? Not handled!
            break;
        /* ... 200 more lines ... */
    }
}
```

**Bugs in this code:**
1. Socket not closed on CONN_FAILED
2. Listeners not notified on CONN_CONNECTED
3. SOCKET_CLOSED not handled in CONN_AUTHENTICATING
4. No entry/exit actions—everything inline
5. Global state—not testable

---

## The C Patterns

### Pattern 1: Simple Switch on Enum

```c
typedef enum { STATE_A, STATE_B, STATE_C } State;
State current = STATE_A;

void process(int input) {
    switch (current) {
        case STATE_A:
            if (input == 1) current = STATE_B;
            break;
        case STATE_B:
            if (input == 2) current = STATE_C;
            else if (input == 0) current = STATE_A;
            break;
        case STATE_C:
            if (input == 0) current = STATE_A;
            break;
    }
}
```

**Problems:**
- No compile-time check for missing cases
- Transitions scattered in conditions
- No entry/exit actions
- Invalid transitions silently ignored

### Pattern 2: State + Event Matrix

```c
typedef enum { S_IDLE, S_RUNNING, S_PAUSED, S_STOPPED, S_COUNT } State;
typedef enum { E_START, E_PAUSE, E_RESUME, E_STOP, E_COUNT } Event;

/* Transition table: [current_state][event] = next_state */
static const State transitions[S_COUNT][E_COUNT] = {
/*              E_START    E_PAUSE    E_RESUME   E_STOP    */
/* S_IDLE    */ {S_RUNNING, S_IDLE,    S_IDLE,    S_STOPPED },
/* S_RUNNING */ {S_RUNNING, S_PAUSED,  S_RUNNING, S_STOPPED },
/* S_PAUSED  */ {S_PAUSED,  S_PAUSED,  S_RUNNING, S_STOPPED },
/* S_STOPPED */ {S_STOPPED, S_STOPPED, S_STOPPED, S_STOPPED },
};

State current = S_IDLE;

void handle_event(Event e) {
    State next = transitions[current][e];
    if (next != current) {
        /* exit action? entry action? who calls them? */
        current = next;
    }
}
```

**Problems:**
- Entry/exit actions require separate dispatch
- Adding state requires updating 2D array (error-prone)
- No type safety on state/event values
- S_IDLE + 1 == S_RUNNING (no semantic protection)

### Pattern 3: Function Pointer Per State

```c
typedef void (*StateHandler)(int event);

void handle_idle(int event);
void handle_running(int event);
void handle_paused(int event);

StateHandler current_handler = handle_idle;

void handle_idle(int event) {
    if (event == START) {
        /* exit IDLE actions... */
        current_handler = handle_running;
        /* entry RUNNING actions... */
    }
}

void dispatch(int event) {
    current_handler(event);
}
```

**Problems:**
- Entry/exit still manual
- Transitions buried in handlers
- No transition validation
- Can set `current_handler` to anything (including NULL)

### Pattern 4: State Pattern (OOP)

```cpp
class State {
public:
    virtual void handle(Context& ctx, Event e) = 0;
    virtual void on_entry(Context& ctx) {}
    virtual void on_exit(Context& ctx) {}
};

class IdleState : public State {
public:
    void handle(Context& ctx, Event e) override {
        if (e == START) {
            ctx.transition_to(new RunningState());  // Memory leak if not careful
        }
    }
};

class Context {
    std::unique_ptr<State> state_;
public:
    void transition_to(State* s) {
        state_->on_exit(*this);
        state_.reset(s);
        state_->on_entry(*this);
    }
};
```

**Problems:**
- Heap allocation per transition (or complex pooling)
- Virtual dispatch overhead
- Invalid transitions still possible at runtime
- No compile-time transition validation

---

## The StateMachine Solution

### Core Concept

Fat-P's `StateMachine` uses **types as states** with compile-time transition validation:

```cpp
#include "StateMachine.h"
using namespace fat_p;

// States are types (can carry behavior)
struct Idle {
    void on_entry(Context& ctx) { ctx.log("Entered Idle"); }
    void on_exit(Context& ctx)  { ctx.log("Exiting Idle"); }
};

struct Running {
    void on_entry(Context& ctx) { ctx.start_timer(); }
    void on_exit(Context& ctx)  { ctx.stop_timer(); }
};

struct Stopped {
    void on_entry(Context& ctx) { ctx.cleanup(); }
    void on_exit(Context& ctx)  { }
};

// Context holds shared data
struct Context {
    void log(const char* msg);
    void start_timer();
    void stop_timer();
    void cleanup();
};

// Define allowed transitions as pairs
using Transitions = std::tuple<
    std::pair<Idle, Running>,     // Idle → Running
    std::pair<Running, Stopped>,  // Running → Stopped
    std::pair<Running, Idle>,     // Running → Idle
    std::pair<Stopped, Idle>      // Stopped → Idle
>;

// Create the state machine
using MyStateMachine = StateMachine<
    Context,
    Transitions,
    StrictTransitionPolicy,  // Only listed transitions allowed
    ThrowingActionPolicy,    // Actions can throw
    0,                       // Initial state index (Idle)
    Idle, Running, Stopped   // State types
>;

int main() {
    Context ctx;
    MyStateMachine sm(ctx);  // Enters Idle, calls Idle::on_entry
    
    sm.transition<Running>();  // Calls Idle::on_exit, then Running::on_entry
    sm.transition<Stopped>();  // Calls Running::on_exit, then Stopped::on_entry
    
    // sm.transition<Running>();  // Would THROW - not in transition list!
    
    sm.transition<Idle>();     // OK - Stopped → Idle is allowed
}
```

### Key Features

| Feature | Benefit |
|---------|---------|
| **States as types** | Each state has its own on_entry/on_exit |
| **Compile-time state list** | Can't transition to undefined state |
| **Transition list** | Explicit allowed transitions |
| **StrictTransitionPolicy** | Runtime check against transition list |
| **AnyToAnyPolicy** | Allow all transitions (for prototyping) |
| **NoExceptActionPolicy** | Enforce noexcept on actions and require nothrow state construction |
| **Zero overhead** | Compiles to equivalent switch |

### Transition Policies

```cpp
// Strict: Only listed transitions allowed (throws on violation)
using StrictSM = StateMachine<Context, Transitions, StrictTransitionPolicy, ...>;

// AnyToAny: All transitions allowed (for rapid prototyping)
using FlexibleSM = StateMachine<Context, std::tuple<>, AnyToAnyTransitionPolicy, ...>;
```

### Action Policies

```cpp
// Throwing: Actions can throw exceptions
using ThrowingSM = StateMachine<..., ThrowingActionPolicy, ...>;

// NoExcept: static_assert that all on_entry/on_exit are noexcept and state types are nothrow
// default-constructible (hooks are invoked on TState{} inside noexcept wrappers)
using SafeSM = StateMachine<..., NoExceptActionPolicy, ...>;
// Compile error if Idle::on_entry is not noexcept!
```

### API Overview

```cpp
template <
    typename Context,
    typename TransitionList,    // std::tuple<std::pair<From, To>...>
    typename TransitionPolicy,  // StrictTransitionPolicy or AnyToAnyTransitionPolicy
    typename ActionPolicy,      // ThrowingActionPolicy or NoExceptActionPolicy
    size_t InitialIndex = 0,
    typename... States
>
class StateMachine {
public:
    // Constructor enters initial state
    StateMachine(Context& context);
    
    // Transition to a state type
    template <typename TNextState>
    void transition();
    
    // Query current state
    int current_state_index() const noexcept;
    
    template <typename TState>
    bool is_in_state() const noexcept;
};
```

---

## Migration Steps

### Step 1: Identify State Enums and Switches

```bash
grep -rn "enum.*State\|enum.*Mode\|enum.*Phase" src/
grep -rn "switch.*state\|switch.*mode\|switch.*phase" src/
grep -rn "case.*STATE_\|case.*MODE_" src/
```

Document:
- All states
- All transitions (which states can go to which)
- Entry actions (what happens when entering a state)
- Exit actions (what happens when leaving a state)

### Step 2: Define State Types

Convert each enum value to a struct with entry/exit actions:

**Before:**
```c
enum State { IDLE, CONNECTING, CONNECTED, ERROR };

void enter_state(State s) {
    switch (s) {
        case IDLE: reset_connection(); break;
        case CONNECTING: start_timeout(); break;
        case CONNECTED: notify_listeners(); break;
        case ERROR: log_error(); break;
    }
}

void exit_state(State s) {
    switch (s) {
        case CONNECTING: cancel_timeout(); break;
        case CONNECTED: flush_buffers(); break;
        default: break;
    }
}
```

**After:**
```cpp
struct Context {
    void reset_connection();
    void start_timeout();
    void cancel_timeout();
    void notify_listeners();
    void flush_buffers();
    void log_error();
};

struct Idle {
    void on_entry(Context& ctx) { ctx.reset_connection(); }
    void on_exit(Context&) {}
};

struct Connecting {
    void on_entry(Context& ctx) { ctx.start_timeout(); }
    void on_exit(Context& ctx)  { ctx.cancel_timeout(); }
};

struct Connected {
    void on_entry(Context& ctx) { ctx.notify_listeners(); }
    void on_exit(Context& ctx)  { ctx.flush_buffers(); }
};

struct Error {
    void on_entry(Context& ctx) { ctx.log_error(); }
    void on_exit(Context&) {}
};
```

### Step 3: Define Transition List

Map your existing transition logic to explicit pairs:

**Before (implicit in switch):**
```c
void handle(Event e) {
    switch (state) {
        case IDLE:
            if (e == CONNECT) state = CONNECTING;
            break;
        case CONNECTING:
            if (e == SUCCESS) state = CONNECTED;
            else if (e == FAIL) state = ERROR;
            break;
        case CONNECTED:
            if (e == DISCONNECT) state = IDLE;
            break;
        case ERROR:
            if (e == RESET) state = IDLE;
            break;
    }
}
```

**After (explicit list):**
```cpp
using Transitions = std::tuple<
    std::pair<Idle, Connecting>,       // CONNECT event
    std::pair<Connecting, Connected>,  // SUCCESS event
    std::pair<Connecting, Error>,      // FAIL event
    std::pair<Connected, Idle>,        // DISCONNECT event
    std::pair<Error, Idle>             // RESET event
>;
```

### Step 4: Create the StateMachine Type

```cpp
using ConnectionSM = StateMachine<
    Context,
    Transitions,
    StrictTransitionPolicy,
    ThrowingActionPolicy,
    0,  // Start in Idle (index 0)
    Idle, Connecting, Connected, Error
>;
```

### Step 5: Replace Event Handling

**Before:**
```c
void on_event(Event e) {
    // Giant switch
}
```

**After:**
```cpp
class ConnectionManager {
    Context mContext;
    ConnectionSM mStateMachine;
    
public:
    ConnectionManager() : mStateMachine(mContext) {}
    
    void on_connect_request() {
        if (mStateMachine.is_in_state<Idle>()) {
            mStateMachine.transition<Connecting>();
            initiate_connection();
        }
    }
    
    void on_connect_success() {
        if (mStateMachine.is_in_state<Connecting>()) {
            mStateMachine.transition<Connected>();
        }
    }
    
    void on_connect_failure() {
        if (mStateMachine.is_in_state<Connecting>()) {
            mStateMachine.transition<Error>();
        }
    }
    
    void on_disconnect() {
        if (mStateMachine.is_in_state<Connected>()) {
            mStateMachine.transition<Idle>();
        }
    }
    
    void on_reset() {
        if (mStateMachine.is_in_state<Error>()) {
            mStateMachine.transition<Idle>();
        }
    }
};
```

### Step 6: Add Compile-Time Safety

Invalid transitions are now caught:

```cpp
void bad_transition() {
    // This compiles but throws at runtime (StrictTransitionPolicy)
    // Idle → Connected is not in the transition list
    mStateMachine.transition<Connected>();  
}
```

To catch at compile time, you can use `static_assert` with a helper:

```cpp
// Add to transition validation
template <typename From, typename To, typename List>
struct is_valid_transition;

// Usage in code review: document which transitions should be valid
static_assert(is_valid_transition<Idle, Connecting, Transitions>::value);
static_assert(!is_valid_transition<Idle, Connected, Transitions>::value);
```

---

## Before/After Examples

### Example 1: Simple Traffic Light

**Before (switch):**
```c
enum LightState { RED, YELLOW, GREEN };
LightState light = RED;

void tick() {
    static int timer = 0;
    timer++;
    
    switch (light) {
        case RED:
            if (timer >= 30) {
                timer = 0;
                light = GREEN;
                // Forgot to turn off red LED!
            }
            break;
        case GREEN:
            if (timer >= 25) {
                timer = 0;
                light = YELLOW;
            }
            break;
        case YELLOW:
            if (timer >= 5) {
                timer = 0;
                light = RED;
            }
            break;
    }
}
```

**After (StateMachine):**
```cpp
struct LightContext {
    int timer = 0;
    
    void red_on()    { /* GPIO */ }
    void red_off()   { /* GPIO */ }
    void yellow_on() { /* GPIO */ }
    void yellow_off(){ /* GPIO */ }
    void green_on()  { /* GPIO */ }
    void green_off() { /* GPIO */ }
};

struct Red {
    void on_entry(LightContext& ctx) { ctx.timer = 0; ctx.red_on(); }
    void on_exit(LightContext& ctx)  { ctx.red_off(); }  // Never forgotten!
};

struct Yellow {
    void on_entry(LightContext& ctx) { ctx.timer = 0; ctx.yellow_on(); }
    void on_exit(LightContext& ctx)  { ctx.yellow_off(); }
};

struct Green {
    void on_entry(LightContext& ctx) { ctx.timer = 0; ctx.green_on(); }
    void on_exit(LightContext& ctx)  { ctx.green_off(); }
};

using Transitions = std::tuple<
    std::pair<Red, Green>,
    std::pair<Green, Yellow>,
    std::pair<Yellow, Red>
>;

using TrafficLight = StateMachine<
    LightContext, Transitions, StrictTransitionPolicy, 
    ThrowingActionPolicy, 0, Red, Yellow, Green
>;

class TrafficController {
    LightContext mCtx;
    TrafficLight mLight;
    
public:
    TrafficController() : mLight(mCtx) {}
    
    void tick() {
        mCtx.timer++;
        
        if (mLight.is_in_state<Red>() && mCtx.timer >= 30) {
            mLight.transition<Green>();
        } else if (mLight.is_in_state<Green>() && mCtx.timer >= 25) {
            mLight.transition<Yellow>();
        } else if (mLight.is_in_state<Yellow>() && mCtx.timer >= 5) {
            mLight.transition<Red>();
        }
    }
};
```

### Example 2: Connection Manager

**Before (function pointers):**
```c
typedef void (*StateHandler)(Connection*, Event);

void idle_handler(Connection* c, Event e) {
    if (e == EV_CONNECT) {
        c->handler = connecting_handler;
        start_connect(c);
    }
}

void connecting_handler(Connection* c, Event e) {
    if (e == EV_CONNECTED) {
        c->handler = connected_handler;
        // Forgot to stop connection timeout!
    } else if (e == EV_TIMEOUT) {
        c->handler = idle_handler;
        report_error(c, "timeout");
    }
}

void connected_handler(Connection* c, Event e) {
    if (e == EV_DISCONNECT) {
        c->handler = idle_handler;
        close_socket(c);
    } else if (e == EV_DATA) {
        process_data(c);
    }
}
```

**After (StateMachine):**
```cpp
struct ConnContext {
    Socket socket;
    Timer timeout;
    
    void start_connect()    { socket.connect_async(); timeout.start(5000); }
    void stop_timeout()     { timeout.cancel(); }
    void close_socket()     { socket.close(); }
    void notify_connected() { /* callback */ }
    void notify_error(const char* msg) { /* callback */ }
};

struct Idle {
    void on_entry(ConnContext& ctx) { }
    void on_exit(ConnContext& ctx)  { }
};

struct Connecting {
    void on_entry(ConnContext& ctx) { ctx.start_connect(); }
    void on_exit(ConnContext& ctx)  { ctx.stop_timeout(); }  // Always stops timeout!
};

struct Connected {
    void on_entry(ConnContext& ctx) { ctx.notify_connected(); }
    void on_exit(ConnContext& ctx)  { ctx.close_socket(); }  // Always closes!
};

struct Failed {
    void on_entry(ConnContext& ctx) { ctx.notify_error("connection failed"); }
    void on_exit(ConnContext& ctx)  { }
};

using ConnTransitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connecting, Failed>,
    std::pair<Connecting, Idle>,      // Cancel during connect
    std::pair<Connected, Idle>,       // Disconnect
    std::pair<Connected, Failed>,     // Connection lost
    std::pair<Failed, Idle>           // Retry
>;

using ConnSM = StateMachine<
    ConnContext, ConnTransitions, StrictTransitionPolicy,
    ThrowingActionPolicy, 0, Idle, Connecting, Connected, Failed
>;
```

### Example 3: Parser States with NoExcept Guarantee

**Before:**
```c
enum ParseState { PARSE_START, PARSE_TAG, PARSE_ATTR, PARSE_VALUE, PARSE_ERROR };
ParseState state = PARSE_START;

int parse_char(char c) {
    switch (state) {
        case PARSE_START:
            if (c == '<') state = PARSE_TAG;
            break;
        case PARSE_TAG:
            if (c == ' ') state = PARSE_ATTR;
            else if (c == '>') state = PARSE_START;
            else if (c == '/') state = PARSE_START;
            break;
        /* ... */
    }
    return 0;  // No error handling
}
```

**After (with noexcept enforcement):**
```cpp
struct ParseContext {
    std::string current_tag;
    std::string current_attr;
    std::string current_value;
    
    void clear_tag() noexcept { current_tag.clear(); }
    void clear_attr() noexcept { current_attr.clear(); }
};

struct ParseStart {
    void on_entry(ParseContext&) noexcept {}
    void on_exit(ParseContext&) noexcept {}
};

struct ParseTag {
    void on_entry(ParseContext& ctx) noexcept { ctx.clear_tag(); }
    void on_exit(ParseContext&) noexcept {}
};

struct ParseAttr {
    void on_entry(ParseContext& ctx) noexcept { ctx.clear_attr(); }
    void on_exit(ParseContext&) noexcept {}
};

struct ParseError {
    void on_entry(ParseContext&) noexcept {}
    void on_exit(ParseContext&) noexcept {}
};

using ParseTransitions = std::tuple<
    std::pair<ParseStart, ParseTag>,
    std::pair<ParseTag, ParseAttr>,
    std::pair<ParseTag, ParseStart>,
    std::pair<ParseAttr, ParseStart>,
    std::pair<ParseTag, ParseError>,
    std::pair<ParseAttr, ParseError>
>;

// NoExceptActionPolicy: compile error if any on_entry/on_exit can throw, or if state
// default construction can throw (StateMachine invokes hooks on TState{} inside noexcept wrappers)
using Parser = StateMachine<
    ParseContext, ParseTransitions, StrictTransitionPolicy,
    NoExceptActionPolicy,  // <-- Enforces noexcept hooks + nothrow state construction
    0, ParseStart, ParseTag, ParseAttr, ParseError
>;
```

---

## Advanced Patterns

### Pattern: State-Specific Data

```cpp
// States can hold data (though instances are temporary)
struct Connecting {
    static constexpr int MAX_RETRIES = 3;
    
    void on_entry(ConnContext& ctx) {
        ctx.retry_count = 0;
        ctx.start_connect();
    }
    void on_exit(ConnContext& ctx) {
        ctx.stop_timeout();
    }
};

// Or use the context for persistent state data
struct ConnContext {
    int retry_count = 0;
    // ... other connection state ...
};
```

### Pattern: Hierarchical States (Manual)

```cpp
// Parent state behavior through composition
struct ActiveBase {
    void common_entry(Context& ctx) { ctx.activate(); }
    void common_exit(Context& ctx)  { ctx.deactivate(); }
};

struct Running : ActiveBase {
    void on_entry(Context& ctx) { 
        common_entry(ctx);
        ctx.start_work();
    }
    void on_exit(Context& ctx) {
        ctx.stop_work();
        common_exit(ctx);
    }
};

struct Paused : ActiveBase {
    void on_entry(Context& ctx) {
        common_entry(ctx);
        ctx.pause_work();
    }
    void on_exit(Context& ctx) {
        common_exit(ctx);
    }
};
```

### Pattern: Event-Driven Transitions

```cpp
class EventDrivenSM {
    Context mCtx;
    MySM mSM;
    
public:
    void dispatch(Event e) {
        // Table-driven event handling
        if (mSM.is_in_state<Idle>()) {
            switch (e) {
                case Event::Start: mSM.transition<Running>(); break;
                case Event::Error: mSM.transition<Failed>(); break;
                default: /* ignore */ break;
            }
        } else if (mSM.is_in_state<Running>()) {
            switch (e) {
                case Event::Pause: mSM.transition<Paused>(); break;
                case Event::Stop:  mSM.transition<Idle>(); break;
                case Event::Error: mSM.transition<Failed>(); break;
                default: break;
            }
        }
        // ...
    }
};
```

### Pattern: Async Transitions with Coroutines

```cpp
// Context for async operations
struct AsyncContext {
    std::function<void()> on_complete;
    
    void start_async_work(std::function<void()> callback) {
        on_complete = std::move(callback);
        // ... start work ...
    }
};

struct Working {
    void on_entry(AsyncContext& ctx) {
        ctx.start_async_work([&ctx]() {
            // Will be called when async work completes
            // Trigger transition from outside
        });
    }
    void on_exit(AsyncContext&) {}
};
```

---

## Verification

### Compile-Time Verification

The StateMachine provides several compile-time guarantees:

```cpp
// 1. State must be in the state list
sm.transition<UnknownState>();  // Compile error: "Target state type not found"

// 2. States must be unique
using BadSM = StateMachine<Ctx, Trans, Policy, Policy, 0, A, B, A>;
// Compile error: "State types must be unique"

// 3. Initial index must be valid
using BadSM = StateMachine<Ctx, Trans, Policy, Policy, 99, A, B, C>;
// Compile error: "InitialIndex must be within 0 to NumStates-1"

// 4. NoExceptActionPolicy enforces noexcept hooks and nothrow default construction
struct BadState {
    void on_entry(Context& ctx) { throw std::runtime_error("oops"); }  // Not noexcept!
    void on_exit(Context& ctx) noexcept {}
};
// Compile error: "NoExceptActionPolicy requires State::on_entry() to be noexcept"
```

### Runtime Verification

```cpp
TEST(StateMachine, TransitionsCorrectly) {
    Context ctx;
    MySM sm(ctx);
    
    EXPECT_TRUE(sm.is_in_state<Idle>());
    EXPECT_EQ(sm.current_state_index(), 0);
    
    sm.transition<Running>();
    EXPECT_TRUE(sm.is_in_state<Running>());
    EXPECT_FALSE(sm.is_in_state<Idle>());
}

TEST(StateMachine, CallsEntryExit) {
    MockContext ctx;
    
    EXPECT_CALL(ctx, idle_entry()).Times(1);
    MySM sm(ctx);  // Enters Idle
    
    EXPECT_CALL(ctx, idle_exit()).Times(1);
    EXPECT_CALL(ctx, running_entry()).Times(1);
    sm.transition<Running>();
    
    EXPECT_CALL(ctx, running_exit()).Times(1);
    EXPECT_CALL(ctx, idle_entry()).Times(1);
    sm.transition<Idle>();
}

TEST(StateMachine, StrictPolicyThrowsOnInvalidTransition) {
    Context ctx;
    StrictSM sm(ctx);  // Starts in Idle
    
    // Idle → Stopped not in transition list
    EXPECT_THROW(sm.transition<Stopped>(), std::runtime_error);
    
    // State unchanged after failed transition
    EXPECT_TRUE(sm.is_in_state<Idle>());
}

TEST(StateMachine, SelfTransitionNoOp) {
    Context ctx;
    MySM sm(ctx);
    
    sm.transition<Running>();
    
    // Self-transition does nothing
    sm.transition<Running>();  // No entry/exit called
    EXPECT_TRUE(sm.is_in_state<Running>());
}
```

---

## When StateMachine Loses

### 1. Dynamic State Creation

If states are defined at runtime (configuration-driven state machines):

```cpp
// Can't do this - states are template parameters
auto state_name = config.get("initial_state");
sm.transition_to(state_name);  // No runtime state lookup
```

**Mitigation:** Use a map of state handlers for fully dynamic state machines.

### 2. Very Large State Counts

With 50+ states, the transition tuple becomes unwieldy:

```cpp
using Transitions = std::tuple<
    std::pair<S1, S2>,
    std::pair<S1, S3>,
    // ... 200 more pairs ...
>;
```

**Mitigation:** Consider generating the transition list from a DSL or configuration.

### 3. Parallel States (Statecharts)

This StateMachine doesn't support orthogonal regions:

```
// Can't have two independent sub-state-machines
[Connection: Idle | Connecting | Connected]
[Authentication: LoggedOut | LoggingIn | LoggedIn]
```

**Mitigation:** Use composition—two separate StateMachine instances.

### 4. History States

No built-in support for "return to previous state":

```cpp
// No "history" transition
sm.transition_to_previous();  // Not supported
```

**Mitigation:** Track previous state in context manually.

### 5. Guard Conditions

Transitions are unconditional (once you call `transition<T>`):

```cpp
// Can't express: "transition to Running only if battery > 20%"
sm.transition<Running>();  // Always transitions if valid
```

**Mitigation:** Check guards before calling transition:

```cpp
if (ctx.battery > 20) {
    sm.transition<Running>();
}
```

---

## Summary

| Aspect | Switch Pattern | StateMachine |
|--------|---------------|--------------|
| Missing case | Compiler warning (maybe) | N/A - no switch |
| Invalid transition | Silent / crash | Compile error or exception |
| Entry/exit actions | Manual, scattered | Automatic, per-state |
| State list | Implicit in switch | Explicit template params |
| Transition list | Implicit in conditions | Explicit tuple |
| Testing | Simulate all paths | Verify at compile time |
| Runtime overhead | Baseline | Zero (same as switch) |
| Action exceptions | Manual handling | Policy-based |

**Migration ROI:**
- **Immediate:** Entry/exit actions never forgotten
- **Short-term:** Invalid transitions caught at compile time
- **Long-term:** Self-documenting state machines, safer refactoring

---

## References

- [RFC 793 - TCP State Machine](https://tools.ietf.org/html/rfc793) — Complex state machine example
- [Boost.MSM](https://www.boost.org/doc/libs/release/libs/msm/) — Full-featured state machine library (heavier)
- [Boost.SML](https://boost-ext.github.io/sml/) — Modern state machine library inspiration
- Fat-P User Manual: StateMachine — Complete API reference

---

*FAT-P Library Documentation — January 2025*
