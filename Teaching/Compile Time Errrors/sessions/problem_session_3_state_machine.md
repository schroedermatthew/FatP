# Problem-Solving Session 3: The Impossible Transition
## Facilitator Guide with Answers

---

## Threading Model (Read This First)

This session assumes **single-thread confinement**: all public methods and all socket callbacks execute on the same thread (e.g., an IO event loop). There are no data races; the bugs are about ordering, missing transitions, and forgotten cleanup.

If your team uses multi-threaded access to connection managers, the code shown here has additional problems (data races, UB) that require synchronization. `StateMachine` does not provide thread safety—it makes transition logic explicit, not atomic.

---

## The Bug

This code shipped. It passed code review. It has a bug.

```cpp
enum class ConnectionState { Idle, Connecting, Connected, Disconnecting, Failed };

class ConnectionManager {
    ConnectionState state_ = ConnectionState::Idle;
    Socket socket_;
    
public:
    void connect(const std::string& host) {
        if (state_ == ConnectionState::Idle) {
            state_ = ConnectionState::Connecting;
            socket_.async_connect(host, [this](bool success) {
                if (success) {
                    state_ = ConnectionState::Connected;
                    notify_connected();
                } else {
                    state_ = ConnectionState::Failed;
                }
            });
        }
    }
    
    void disconnect() {
        if (state_ == ConnectionState::Connecting) {
            // BUG: closes socket but doesn't cancel the async operation
            // or handle the callback that will still arrive
            socket_.close();
            state_ = ConnectionState::Idle;
            return;
        }
        if (state_ == ConnectionState::Connected) {
            state_ = ConnectionState::Disconnecting;
            socket_.async_close([this]() {
                state_ = ConnectionState::Idle;
            });
        }
    }
    
    void send(const std::string& data) {
        if (state_ == ConnectionState::Connected) {
            socket_.send(data);
        }
    }
    
    void on_socket_error() {
        state_ = ConnectionState::Failed;
        // Forgot to close socket!
    }
    
    void retry() {
        if (state_ == ConnectionState::Failed) {
            state_ = ConnectionState::Idle;
        }
    }
};
```

A customer reports: "Sometimes `send()` works, sometimes it silently fails. We also occasionally see 'connected' right before a failure."

---

### How This Bug Manifests (Interleaving Trace)

Here's the exact sequence that produces the symptom:

| Step | Event | State Before | Action | State After |
|------|-------|--------------|--------|-------------|
| 1 | `connect("host")` | Idle | Starts async_connect | Connecting |
| 2 | `disconnect()` | Connecting | Closes socket, sets Idle | Idle |
| 3 | Connect callback fires (success=true) | Idle | Sets Connected, calls notify_connected() | Connected |
| 4 | `send("data")` | Connected | Calls socket_.send() on closed socket | Connected |

**Result:** The state says `Connected`, but the socket is closed. `send()` either silently fails or throws deep in the socket layer. The customer sees "connected" notification followed by send failures.

**The "impossible" part:** The system is in `Connected` state while the underlying resource (socket) is not connected. State and reality have diverged.

**Resource invariants that should hold (but don't):**

| State | Socket | Outstanding Ops | Can Send? |
|-------|--------|-----------------|-----------|
| Idle | Closed | None | No |
| Connecting | Opening | connect pending | No |
| Connected | Open | None | Yes |
| Disconnecting | Closing | close pending | No |
| Failed | Closed | None | No |

Step 3 in the trace violates this: we're in `Connected` but the socket is closed. The enum doesn't enforce invariants—it just names states.

---

### Question 1: What's wrong with this code?

**[Facilitator: Pause here. Let participants find issues before revealing the answer.]**

**Answer:**

Separate the issues into three categories:

#### A. Concrete Bugs in This Code

1. **Late callback corrupts state**
   ```cpp
   void disconnect() {
       if (state_ == ConnectionState::Connecting) {
           socket_.close();
           state_ = ConnectionState::Idle;  // But the callback will still fire!
   ```
   The async_connect callback isn't cancelled. When it fires, it overwrites `Idle` → `Connected` on a closed socket.

2. **on_socket_error doesn't close socket**
   ```cpp
   void on_socket_error() {
       state_ = ConnectionState::Failed;
       // Socket still open! Resource leak.
   }
   ```

3. **send() silently drops data**
   ```cpp
   void send(const std::string& data) {
       if (state_ == ConnectionState::Connected) {
           socket_.send(data);
       }
       // Not Connected? Data vanishes. No error, no log.
   }
   ```
   
   *Note: Whether this is a "bug" depends on your API contract. Valid designs include: throw, return error code, log and drop, queue for later. The problem is that the contract isn't defined or enforced—silence is the worst option because callers can't detect failure.*

#### B. Design Hazards (Why This Pattern Breeds Bugs)

4. **Transition rules are implicit and scattered**
   - `connect()` checks for `Idle`
   - `disconnect()` checks for `Connecting` or `Connected`
   - `retry()` checks for `Failed`
   - The complete transition graph exists only by reading all the code

5. **Entry/exit actions are inconsistent**
   - Entering `Failed` should close the socket—but `on_socket_error()` forgets
   - Exiting `Connected` should notify listeners—sometimes it does, sometimes it doesn't
   - There's no single place that defines "what happens when entering/exiting a state"

6. **No enforcement of valid transitions**
   - Nothing prevents future code from writing `state_ = ConnectionState::Connected` directly
   - The enum defines what states *exist*, not which transitions are *allowed*

#### C. Multi-Threading Hazards (If Not Single-Thread Confined)

7. **Data races on state_**
   - If callbacks can race with public methods, reading/writing `state_` is UB
   - `StateMachine` doesn't fix this—you need a mutex or message queue

**The compiler warns about nothing.** All the code is type-correct. The bugs are in the *logic* of state transitions and resource lifecycle, which the type system doesn't encode.

**Key insight:** The enum defines *what states exist*, but not *which transitions are valid*, *what must happen during transitions*, or *how to correlate callbacks with current state*.

---

### Question 2: How would you normally test this?

**Answer:**

You'd need to test every possible state × event combination:

| State | connect() | disconnect() | send() | on_error() | retry() |
|-------|-----------|--------------|--------|------------|---------|
| Idle | → Connecting | ??? | ??? | ??? | ??? |
| Connecting | ??? | ??? | ??? | → Failed | ??? |
| Connected | ??? | → Disconnecting | OK | → Failed | ??? |
| Disconnecting | ??? | ??? | ??? | → Failed | ??? |
| Failed | ??? | ??? | ??? | ??? | → Idle |

That's 5 states × 5 events = **25 test cases** minimum, and that's before you consider:
- Async callback timing (the actual bug!)
- Resource state (socket open/closed)
- Callback-during-callback scenarios

**The testing burden grows quadratically** with states and events. And you're testing *behavior*, not *correctness*—the tests pass if the code does what it does, not what it should do.

**The real problem:** We're using tests to verify what should be structural invariants.

---

### Question 3: What's wrong with the "check state at the top of each function" pattern?

**Answer:**

The pattern looks like this:

```cpp
void send(const std::string& data) {
    if (state_ != ConnectionState::Connected) {
        return;  // or throw, or log
    }
    socket_.send(data);
}
```

**Problems:**

1. **It's scattered:** Every function has its own state check. Miss one, and you have a bug.

2. **It's easy to get wrong:**
   ```cpp
   void disconnect() {
       if (state_ == ConnectionState::Connected) {  // Should this also allow Connecting?
   ```

3. **Transitions are implicit:** Reading `disconnect()`, you can't tell that it transitions to `Disconnecting` without reading the whole function.

4. **Entry/exit actions are forgotten:**
   ```cpp
   void retry() {
       if (state_ == ConnectionState::Failed) {
           state_ = ConnectionState::Idle;
           // Should we notify listeners? Close resources? Who knows!
       }
   }
   ```

5. **It fails silently:** When `send()` is called in the wrong state, it just... does nothing. No error, no log, no indication. The bug hides.

6. **Discipline doesn't scale:** With 10 states and 20 methods, you need 200 correct decisions. One mistake = shipped bug.

---

### Question 4: How does StateMachine solve this?

**Answer:**

Fat-P's `StateMachine` inverts the model: instead of *checking* states, you *declare* them.

```cpp
#include <fat_p/StateMachine.h>
using namespace fat_p;

// 1. Context holds shared data
struct ConnContext {
    Socket socket;
    std::string host;
    std::function<void()> on_connected;
    std::function<void()> on_disconnected;
    std::function<void(const std::string&)> on_error;
};

// 2. States are types with entry/exit actions
struct Idle {
    void on_entry(ConnContext& ctx) noexcept {
        // Reset for next connection
    }
    void on_exit(ConnContext& ctx) noexcept {
        // Nothing to clean up
    }
};

struct Connecting {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.async_connect(ctx.host);
        // NOTE: Callback wiring shown in "Usage" section below.
        // Late callback correlation shown in "Note on late callbacks."
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Connected {
    void on_entry(ConnContext& ctx) noexcept {
        if (ctx.on_connected) ctx.on_connected();
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Disconnecting {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.async_close([](){ /* handled externally */ });
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Failed {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.close();  // Always close on failure!
        if (ctx.on_error) ctx.on_error("Connection failed");
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

// 3. Declare valid transitions
using Transitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connecting, Disconnecting>,  // Allow cancel during connect
    std::pair<Connecting, Failed>,
    std::pair<Connected, Disconnecting>,
    std::pair<Connected, Failed>,
    std::pair<Disconnecting, Idle>,
    std::pair<Disconnecting, Failed>,
    std::pair<Failed, Idle>
>;

// 4. Create the state machine
using ConnectionSM = StateMachine<
    ConnContext,
    Transitions,
    StrictTransitionPolicy,   // Invalid transitions throw
    NoExceptActionPolicy,     // Actions must be noexcept
    0,                        // Start in Idle (index 0)
    Idle, Connecting, Connected, Disconnecting, Failed
>;
```

**What this gives you:**

| Problem | Switch-based | StateMachine |
|---------|--------------|--------------|
| Missing transition | Silent bug | Runtime exception |
| Forgotten entry action | Bug | Automatic—just implement on_entry |
| Forgotten exit action | Bug | Automatic—just implement on_exit |
| Invalid state value | Possible (cast int) | Impossible (types) |
| Transition documentation | Comments (maybe) | Explicit tuple |

---

### Question 5: What does StrictTransitionPolicy actually do?

**Answer:**

`StrictTransitionPolicy` validates every transition at runtime against the declared list:

```cpp
// Behavior (conceptual — see StateMachine.h for actual implementation):
template <typename TNextState>
void transition() {
    // Compile-time: is TNextState even a valid state type?
    static_assert(contains_state<TNextState>, "Target state type not found");
    
    // Runtime: is this transition allowed from current state?
    if (!is_allowed(currentState, TNextState)) {
        throw std::runtime_error(
            "CTSM Error: Transition is not valid under StrictTransitionPolicy");
        // State unchanged — safe to retry or handle error
    }
    
    exit_current_state();
    update_to(TNextState);
    enter_new_state();
}
```

**The transition matrix for our ConnectionSM:**

```
                  Idle  Connecting  Connected  Disconnecting  Failed
Idle               -        ✓          -            -           -
Connecting         -        -          ✓            ✓           ✓
Connected          -        -          -            ✓           ✓
Disconnecting      ✓        -          -            -           ✓
Failed             ✓        -          -            -           -
```

Now if you write:

```cpp
sm.transition<Connected>();  // While in Idle
```

You get a **runtime exception**: `"CTSM Error: Transition is not valid under StrictTransitionPolicy"`. The bug is caught immediately, not weeks later in production.

**Note:** The error message doesn't include state names. In debugging, inspect `sm_.currentStateIndex()` or add logging around transition calls.

**Alternative: AnyToAnyTransitionPolicy**

For prototyping or when you genuinely want all transitions allowed:

```cpp
using FlexibleSM = StateMachine<
    ConnContext,
    std::tuple<>,              // Empty—no restrictions
    AnyToAnyTransitionPolicy,  // All transitions allowed
    NoExceptActionPolicy,
    0, Idle, Connecting, Connected, Disconnecting, Failed
>;
```

---

### Question 6: What does NoExceptActionPolicy enforce?

**Answer:**

`NoExceptActionPolicy` uses `static_assert` to verify that all `on_entry` and `on_exit` methods are `noexcept`:

```cpp
// Conceptual enforcement (exact implementation may differ):
static_assert(
    noexcept(std::declval<State>().on_entry(std::declval<Context&>())),
    "NoExceptActionPolicy requires State::on_entry() to be noexcept"
);
static_assert(
    noexcept(std::declval<State>().on_exit(std::declval<Context&>())),
    "NoExceptActionPolicy requires State::on_exit() to be noexcept"
);
```

If you write:

```cpp
struct BadState {
    void on_entry(ConnContext& ctx) {  // Not noexcept!
        throw std::runtime_error("oops");
    }
    void on_exit(ConnContext& ctx) noexcept {}
};
```

You get a **compile error**:

```
error: static assertion failed: NoExceptActionPolicy requires State::on_entry() to be noexcept
```

**Why enforce noexcept?**

1. **Exception safety:** If `on_exit` throws during a transition, the state machine is in an inconsistent state—we've left the old state but haven't entered the new one.

2. **Performance:** `noexcept` enables compiler optimizations.

3. **Design discipline:** If your entry action can fail, you probably need an intermediate state or error state.

**Alternative: ThrowingActionPolicy**

If your actions legitimately need to throw:

```cpp
using ThrowingSM = StateMachine<
    ConnContext,
    Transitions,
    StrictTransitionPolicy,
    ThrowingActionPolicy,  // Actions can throw
    0, Idle, Connecting, Connected, Disconnecting, Failed
>;
```

---

### Question 7: What are the compile-time guarantees?

**Answer:**

`StateMachine` enforces these at compile time:

**1. State types must be unique:**
```cpp
using BadSM = StateMachine<Ctx, Trans, Policy, Policy, 0, 
    Idle, Connecting, Idle  // Duplicate!
>;
// error: static assertion failed: State types must be unique in the variadic pack
```

**2. Initial index must be valid:**
```cpp
using BadSM = StateMachine<Ctx, Trans, Policy, Policy, 
    99,  // Only 5 states!
    Idle, Connecting, Connected, Disconnecting, Failed
>;
// error: static assertion failed: InitialIndex must be within 0 to NumStates-1
```

**3. Must have at least one state:**
```cpp
using BadSM = StateMachine<Ctx, Trans, Policy, Policy, 0>;  // No states!
// error: static assertion failed: StateMachine must have at least one state
```

**4. Transition target must exist:**
```cpp
struct UnknownState {};
sm.transition<UnknownState>();
// error: static assertion failed: Target state type not found in StateMachine
```

**5. Queried state must exist:**
```cpp
sm.isInState<UnknownState>();
// error: static assertion failed: Queried state type not found in StateMachine
```

**6. Actions must be noexcept (with NoExceptActionPolicy):**
```cpp
struct ThrowingState {
    void on_entry(Ctx&) { throw; }  // Not noexcept
    void on_exit(Ctx&) noexcept {}
};
// error: static assertion failed: NoExceptActionPolicy requires State::on_entry() to be noexcept
```

---

### Question 8: What's the runtime cost?

**Answer: Near-zero, but measure if it's in a hot path.**

A `StateMachine` transition is fundamentally the same shape as a carefully-written `switch`: a small amount of control-flow plus the work in your entry/exit actions. With `StrictTransitionPolicy`, you also pay an **O(1) transition validity check**.

1. **State storage:** Single integer (`mCurrentStateIndex`)
2. **Transition check:** Array lookup (with `StrictTransitionPolicy`)
3. **State query:** Integer comparison
4. **Action dispatch:** Indirect call via function-pointer table; overhead is small; measure if hot

Operationally, a transition does: (1) validate against allowed transitions (Strict only), (2) run current state's `on_exit`, (3) update state index, (4) run new state's `on_entry`.

**What you should claim:** The abstraction is designed to have minimal overhead—comparable to a well-written switch statement. If this state machine sits in a hot path (millions of transitions per second), benchmark it against your hand-rolled alternative with your actual compiler and optimization flags.

---

### Question 9: What does StateMachine NOT solve?

**Answer:**

Be explicit about the boundaries:

1. **Thread safety:** `StateMachine` is not thread-safe. If multiple threads can call `transition()` or query state concurrently, you need external synchronization (mutex, strand, message queue).

2. **Late callback correlation:** The original bug involved a callback firing after `disconnect()`. `StateMachine` makes the *transition* throw if invalid, but it doesn't prevent the callback from *trying*. You still need generation counters or cancellation tokens to correlate callbacks with the current "attempt."

3. **Business semantics:** You still decide what `disconnect()` during `Connecting` *should* do:
   - Option A: Throw (not allowed)
   - Option B: Transition to `Disconnecting` and cancel the connect
   - Option C: Record "disconnect requested" and handle in callback
   
   `StateMachine` enforces your decision; it doesn't make it for you.

4. **Destructor cleanup:** The destructor does NOT call `on_exit()` on the current state. If cleanup is required, explicitly transition to a terminal state before destruction.

---

## The Fix: Rewriting ConnectionManager

```cpp
#include <fat_p/StateMachine.h>

// Context
struct ConnContext {
    Socket socket;
    std::string host;
    std::function<void()> on_connected;
    std::function<void()> on_disconnected;
};

// States (forward declarations for transition list)
struct Idle;
struct Connecting;
struct Connected;
struct Disconnecting;
struct Failed;

// State implementations
struct Idle {
    void on_entry(ConnContext& ctx) noexcept {}
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Connecting {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.async_connect(ctx.host);
        // NOTE: This fires an async operation. The callback must be correlated
        // to the current attempt (generation counter) or cancelled on exit.
        // See "Note on late callbacks" below.
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Connected {
    void on_entry(ConnContext& ctx) noexcept {
        if (ctx.on_connected) ctx.on_connected();
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Disconnecting {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.cancel();      // Best-effort cancel; callback may still fire (see late callbacks note)
        ctx.socket.async_close(); // Start close
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Failed {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.close();  // Always cleanup!
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

// Transition list — includes disconnect during connect (common production requirement)
// Syntax: std::tuple of std::pair<From, To> (see User Manual for full API reference)
using Transitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connecting, Failed>,
    std::pair<Connecting, Disconnecting>,  // Allow disconnect during connect
    std::pair<Connected, Disconnecting>,
    std::pair<Connected, Failed>,
    std::pair<Disconnecting, Idle>,
    std::pair<Disconnecting, Failed>,
    std::pair<Failed, Idle>
>;

using ConnectionSM = fat_p::StateMachine<
    ConnContext,
    Transitions,
    fat_p::StrictTransitionPolicy,
    fat_p::NoExceptActionPolicy,  // Hooks can't throw; report errors via context or state
    0, Idle, Connecting, Connected, Disconnecting, Failed
>;
// We use NoExceptActionPolicy because connection lifecycle hooks should not throw.
// Errors are reported by transitioning to Failed, not by exceptions mid-transition.

class ConnectionManager {
    ConnContext ctx_;
    ConnectionSM sm_;
    
public:
    ConnectionManager() : sm_(ctx_) {}
    
    void connect(const std::string& host) {
        ctx_.host = host;
        sm_.transition<Connecting>();  // Throws if not in Idle
    }
    
    void on_connect_success() {
        sm_.transition<Connected>();
    }
    
    void on_connect_failure() {
        sm_.transition<Failed>();
    }
    
    void disconnect() {
        sm_.transition<Disconnecting>();  // Works from Connected or Connecting
    }
    
    void on_disconnect_complete() {
        sm_.transition<Idle>();
    }
    
    void on_socket_error() {
        sm_.transition<Failed>();  // Works from Connected, Connecting, or Disconnecting
    }
    
    void retry() {
        sm_.transition<Idle>();  // Only valid from Failed
    }
    
    void send(const std::string& data) {
        if (!sm_.isInState<Connected>()) {
            throw std::logic_error("Cannot send: not connected");
        }
        ctx_.socket.send(data);
    }
    
    bool is_connected() const {
        return sm_.isInState<Connected>();
    }
    
    // For wiring up callbacks (typically done once at construction)
    Socket& socket() { return ctx_.socket; }
};
```

**Usage: Wiring Up the Async Callbacks**

```cpp
// In your application code:
ConnectionManager conn;

void setup_connection(const std::string& host) {
    // Wire up the socket's async callbacks to drive the state machine
    conn.socket().set_connect_callback([&](bool success) {
        if (success) {
            conn.on_connect_success();
        } else {
            conn.on_connect_failure();
        }
    });
    
    conn.socket().set_close_callback([&]() {
        conn.on_disconnect_complete();
    });
    
    conn.socket().set_error_callback([&]() {
        conn.on_socket_error();
    });
    
    // Start connecting
    conn.connect(host);
}

// User clicks "Disconnect" while still connecting — this now works correctly:
void on_disconnect_button() {
    conn.disconnect();  // Connecting → Disconnecting: cancels connect, starts close
}
```

**The transition logic is fixed.** The original code closed the socket but let the callback corrupt state. Now `disconnect()` transitions to `Disconnecting`, which calls `cancel()` on pending operations. However, if your platform's `cancel()` doesn't guarantee callbacks won't fire, you also need generation correlation (see note below).

**Alternative policy: Disallow disconnect during Connecting**

If your product requires that users wait for connection to complete (or fail) before disconnecting, remove the transition:

```cpp
using StrictTransitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connecting, Failed>,
    // NO Connecting → Disconnecting
    std::pair<Connected, Disconnecting>,
    std::pair<Connected, Failed>,
    std::pair<Disconnecting, Idle>,
    std::pair<Disconnecting, Failed>,
    std::pair<Failed, Idle>
>;

// Now disconnect() during Connecting throws:
void on_disconnect_button() {
    try {
        conn.disconnect();
    } catch (const std::runtime_error& e) {
        // "CTSM Error: Transition is not valid under StrictTransitionPolicy"
        show_error("Please wait for connection to complete");
    }
}
```

The state machine enforces whichever policy you choose. The point is that you *made* a choice, and it's visible in the transition list.

---

**Bugs fixed:**

1. ✅ **disconnect during Connecting** → properly cancels and transitions to Disconnecting
2. ✅ **on_socket_error cleanup** → `Failed::on_entry` always closes socket
3. ✅ **Entry/exit actions** → guaranteed to run on every transition
4. ✅ **Invalid transitions** → throw immediately with clear error
5. ✅ **send() fails loudly** → explicit exception

**Still requires application-level handling:**

- ❗ Late callbacks need generation counter or cancellation check (see note below)
- ❗ Thread safety needs external synchronization
- ❗ Destructor doesn't call on_exit — don't rely on `on_exit` for mandatory cleanup; use explicit transitions before destruction or RAII members in Context

**Note on late callbacks:** If `cancel()` doesn't guarantee the callback won't fire, you need a generation counter:

```cpp
struct ConnContext {
    Socket socket;
    std::string host;
    uint64_t connect_generation = 0;  // Incremented on each connect attempt
    // ...
};

struct Connecting {
    void on_entry(ConnContext& ctx) noexcept {
        ++ctx.connect_generation;
        uint64_t my_gen = ctx.connect_generation;
        
        ctx.socket.async_connect(ctx.host, [&ctx, my_gen](bool success) {
            if (my_gen != ctx.connect_generation) return;  // Stale callback
            // Route to ConnectionManager event handler (which calls sm_.transition<...>())
            if (success) {
                ctx.on_connect_success();
            } else {
                ctx.on_connect_failure();
            }
        });
    }
};
```

In practice, `ConnContext` holds function pointers or a reference to the `ConnectionManager` so callbacks can route events back to the state machine owner.

**If you do nothing else, do this:** every async completion must prove it belongs to the current attempt before it can transition state.

---

## Discussion Points

### "Where do you have state machines implemented with switch statements?"

Common places to audit:

- Connection managers (TCP, HTTP, WebSocket)
- Protocol parsers (HTTP, SMTP, custom protocols)
- UI controllers (wizard flows, form states)
- Game logic (player states, AI states)
- Hardware drivers (initialization sequences)
- Workflow engines (order processing, approval flows)

### "What's the migration cost?"

The transition is straightforward but not trivial:

1. **Identify states** → existing enum values become struct types
2. **Identify transitions** → existing switch conditions become tuple entries
3. **Extract entry/exit actions** → scattered code moves to state structs
4. **Create context** → shared data moves to context struct
5. **Update callers** → state checks become `isInState<T>()`

Expect 2-4 hours for a typical 5-state machine. The payoff is immediate: entry/exit bugs vanish, invalid transitions throw.

### "When is StateMachine overkill?"

StateMachine might be overkill when:

- You have 2-3 states with trivial transitions
- No entry/exit actions needed
- The state machine is purely local to one function
- You're prototyping and will throw the code away

It's worth it when:

- You have 4+ states
- Invalid transitions would cause data corruption or security issues
- Entry/exit actions are critical (resource management, notifications)
- Multiple developers will maintain the code
- The state machine crosses API boundaries

---

## Summary of Key Points

1. **Switch-based state machines are bug magnets** — missing transitions, forgotten actions, silent failures

2. **StateMachine encodes transitions in the type system** — invalid states are compile errors; invalid transitions are runtime errors

3. **Entry/exit actions are automatic** — just implement `on_entry` and `on_exit`

4. **Policies give you control:**
   - `StrictTransitionPolicy` enforces the transition list
   - `NoExceptActionPolicy` enforces exception safety

5. **Compile-time guarantees include:** unique states, valid initial index, target state existence

6. **Runtime overhead is minimal** — comparable to a switch statement; benchmark if it's in a hot path

7. **StateMachine doesn't solve everything** — thread safety, late callbacks, and business semantics are still your responsibility

---

## When to Look Elsewhere

Fat-P StateMachine handles the common case: flat state machines with 3-15 states, explicit transitions, automatic entry/exit hooks. This covers connection managers, protocol handlers, UI flows, game entity states—the vast majority of state machines in production code.

**Boost.SML and Boost.MSM** exist for UML statechart features: hierarchical states, orthogonal regions, history states, built-in guards. If you genuinely need these, Boost is there.

But be honest about the tradeoffs:

| | Fat-P StateMachine | Boost.SML/MSM |
|---|---|---|
| Learning curve | 30 minutes | Days to weeks |
| Compile time impact | Negligible | Significant |
| Error messages | Clear static_asserts | Template vomit |
| Dependencies | None | Boost |
| Features you'll use | All of them | 20% |

Most teams that reach for Boost.SML don't need hierarchical states—they need explicit transitions and automatic cleanup. That's Fat-P.

If you later discover you need orthogonal regions or history states, migration to Boost.SML is straightforward: the mental model (states as types, transitions as declarations) is the same. Start simple.

---

## Further Reading

From your materials:
- `Migration Guide - Switch to StateMachine.md` — full migration walkthrough
- `fat_p/StateMachine.h` — implementation with detailed comments

External:
- [Boost.SML](https://boost-ext.github.io/sml/) — feature-rich state machine library
- [Boost.MSM](https://www.boost.org/doc/libs/release/libs/msm/) — full UML state chart support
- [RFC 793 TCP State Machine](https://tools.ietf.org/html/rfc793) — classic complex state machine example
- "Statecharts: A Visual Formalism for Complex Systems" (Harel, 1987) — the foundational paper on hierarchical state machines
