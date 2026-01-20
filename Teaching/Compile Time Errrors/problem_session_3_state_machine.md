# Problem-Solving Session 3: The Impossible Transition
## Facilitator Guide with Answers

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
            // What if someone calls connect() during this transition?
        }
    }
};
```

A customer reports: "Sometimes `send()` works, sometimes it silently fails." After weeks of debugging, you find that `disconnect()` was called during `Connecting`, leaving the state machine in an inconsistent state.

---

### Question 1: How many bugs can you find? Would the compiler warn?

**Answer:**

There are at least **six bugs** in this code:

1. **Missing transition: disconnect during Connecting**
   ```cpp
   void disconnect() {
       if (state_ == ConnectionState::Connected) {  // Only handles Connected!
   ```
   If `disconnect()` is called while `Connecting`, nothing happens. The connection completes, but the caller thinks they disconnected.

2. **Missing transition: on_socket_error doesn't close socket**
   ```cpp
   void on_socket_error() {
       state_ = ConnectionState::Failed;
       // Socket still open! Resource leak.
   }
   ```

3. **Race in retry()**
   ```cpp
   void retry() {
       if (state_ == ConnectionState::Failed) {
           state_ = ConnectionState::Idle;  // Now Idle
           // If connect() is called here by another thread...
       }
   }
   ```

4. **No entry/exit actions**
   - When entering `Failed`, should we close the socket?
   - When exiting `Connected`, should we notify listeners?
   - These are scattered throughout the code or forgotten entirely.

5. **Invalid transition: Failed → Connecting is possible**
   ```cpp
   connect();  // State is now Failed
   retry();    // State is now Idle
   connect();  // OK, but what if retry() hasn't finished?
   ```
   There's no explicit check that prevents calling `connect()` from `Failed`.

6. **send() silently fails**
   ```cpp
   void send(const std::string& data) {
       if (state_ == ConnectionState::Connected) {
           socket_.send(data);
       }
       // If not Connected, data is silently dropped!
   }
   ```

**The compiler warns about nothing.** All the code is type-correct. The bugs are in the *logic* of state transitions, which the type system doesn't encode.

**Key insight:** The enum defines *what states exist*, but not *which transitions are valid* or *what must happen during transitions*.

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
- Async callback timing
- Thread interleaving
- Callback-during-callback scenarios
- Resource state (socket open/closed)

**The testing burden grows quadratically** with states and events. And you're testing *behavior*, not *correctness*—the tests pass if the code does what it does, not what it should do.

**The real problem:** We're using tests to verify what should be compile-time invariants.

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
        ctx.socket.async_connect(ctx.host, [](bool) {
            // Callback handled externally
        });
    }
    void on_exit(ConnContext& ctx) noexcept {
        // If exiting to Failed, socket cleanup happens in Failed::on_entry
    }
};

struct Connected {
    void on_entry(ConnContext& ctx) noexcept {
        if (ctx.on_connected) ctx.on_connected();
    }
    void on_exit(ConnContext& ctx) noexcept {
        // Notify listeners we're leaving Connected
    }
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
| Testing burden | 25+ tests | ~5 tests for transition logic |

---

### Question 5: What does StrictTransitionPolicy actually do?

**Answer:**

`StrictTransitionPolicy` builds a **transition matrix at compile time** and checks every transition at runtime:

```cpp
// From StateMachine.h (simplified)
static constexpr auto transition_matrix = build_transition_matrix<TransitionList>();

template <typename TNextState>
void transition() {
    constexpr int nextIndex = get_state_index<TNextState>();
    const int currentIndex = mCurrentStateIndex;
    
    // Compile-time: is TNextState even a valid state?
    static_assert(nextIndex != -1, "Target state type not found in StateMachine");
    
    // Runtime: is this transition allowed?
    if (!transition_matrix[currentIndex][nextIndex]) {
        throw std::runtime_error("Invalid transition");
    }
    
    dispatch_exit_action(currentIndex);
    mCurrentStateIndex = nextIndex;
    dispatch_entry_action(nextIndex);
}
```

**The transition matrix for our ConnectionSM:**

```
                  Idle  Connecting  Connected  Disconnecting  Failed
Idle               -        ✓          -            -           -
Connecting         -        -          ✓            -           ✓
Connected          -        -          -            ✓           ✓
Disconnecting      ✓        -          -            -           ✓
Failed             ✓        -          -            -           -
```

Now if you write:

```cpp
sm.transition<Connected>();  // While in Idle
```

You get a **runtime exception**: "Invalid transition". The bug is caught immediately, not weeks later in production.

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
// From StateMachine.h
template <typename State, typename Context>
struct ActionPolicyEnforcer<NoExceptActionPolicy, State, Context> {
    static constexpr void validate() noexcept {
        static_assert(
            noexcept(std::declval<State>().on_entry(std::declval<Context&>())),
            "NoExceptActionPolicy requires State::on_entry() to be noexcept"
        );
        static_assert(
            noexcept(std::declval<State>().on_exit(std::declval<Context&>())),
            "NoExceptActionPolicy requires State::on_exit() to be noexcept"
        );
    }
};
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
sm.is_in_state<UnknownState>();
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

**Answer: usually negligible, but measure it.**

A `StateMachine` transition is fundamentally the same shape as a carefully-written `switch`: a small amount of control-flow plus the work in your entry/exit actions. With `StrictTransitionPolicy`, you also pay an **O(1) transition validity check** (a lookup in a `constexpr` matrix).

1. **State storage:** Single integer (`mCurrentStateIndex`)
2. **Transition check:** Array lookup (with `StrictTransitionPolicy`)
3. **State query:** Integer comparison
4. **Action dispatch:** Compile-time resolved to direct calls

The transition matrix is `constexpr`—it's computed at compile time and embedded as constant data.

```cpp
// This:
sm.transition<Connected>();

// Compiles to approximately:
if (!transition_matrix[current][CONNECTED_INDEX]) throw ...;
current_state_exit_actions[current](context);
current = CONNECTED_INDEX;
Connected{}.on_entry(context);
```

**What you should claim:** the abstraction is designed to be “near-zero overhead” for the dispatch/validation, and it makes correctness and maintainability explicit. If this state machine sits in a hot path, include a micro-benchmark in your codebase and compare against your best hand-written `switch` for your actual compiler + optimization flags.

**Thread-safety note:** `StateMachine` does not automatically make transitions “atomic” across threads. If multiple threads can call `transition()` or query state concurrently, you still need external synchronization (mutex, message passing, confinement to one thread, etc.).

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
        ctx.socket.async_close();
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

struct Failed {
    void on_entry(ConnContext& ctx) noexcept {
        ctx.socket.close();  // Always cleanup!
    }
    void on_exit(ConnContext& ctx) noexcept {}
};

// Transition list
using Transitions = std::tuple<
    std::pair<Idle, Connecting>,
    std::pair<Connecting, Connected>,
    std::pair<Connecting, Failed>,
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
    fat_p::NoExceptActionPolicy,
    0, Idle, Connecting, Connected, Disconnecting, Failed
>;

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
        sm_.transition<Disconnecting>();  // Throws if not in Connected
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
        if (!sm_.is_in_state<Connected>()) {
            throw std::logic_error("Cannot send: not connected");
        }
        ctx_.socket.send(data);
    }
    
    bool is_connected() const {
        return sm_.is_in_state<Connected>();
    }
};
```

**Bugs fixed:**

1. ✅ **disconnect during Connecting** → throws immediately
2. ✅ **on_socket_error cleanup** → `Failed::on_entry` always closes socket
3. ✅ **Retry logic centralized** → the only way to change state is through `transition<...>()` (still requires external synchronization if used from multiple threads)
4. ✅ **Entry/exit actions** → guaranteed to run
5. ✅ **Invalid transitions** → throw immediately
6. ✅ **send() fails loudly** → explicit exception

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
5. **Update callers** → state checks become `is_in_state<T>()`

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

2. **StateMachine encodes transitions in the type system** — invalid states and transitions become compile or runtime errors

3. **Entry/exit actions are automatic** — just implement `on_entry` and `on_exit`

4. **Policies give you control:**
   - `StrictTransitionPolicy` enforces the transition list
   - `NoExceptActionPolicy` enforces exception safety

5. **Compile-time guarantees include:** unique states, valid initial index, target state existence

6. **Runtime cost is zero** — compiles to equivalent switch/jump table

---

## Alternatives to Fat-P StateMachine

### When StateMachine Is the Right Choice

Fat-P `StateMachine` excels when you need:
- **Explicit transition validation** — only listed transitions are allowed
- **Automatic entry/exit actions** — no forgotten cleanup
- **Zero runtime overhead** — compiles to equivalent switch
- **Simple adoption** — no external dependencies, header-only
- **Moderate complexity** — typically 3-15 states

### When to Consider Alternatives

| Requirement | StateMachine | Alternative |
|-------------|--------------|-------------|
| Dynamic state creation at runtime | ❌ | Boost.SML, custom |
| 50+ states | Unwieldy | Boost.SML, code generation |
| Parallel/orthogonal regions | ❌ | Boost.SML, Boost.MSM |
| History states (remember sub-state) | ❌ | Boost.SML, Boost.MSM |
| Guard conditions on transitions | Manual | Boost.SML (built-in) |
| Hierarchical states | ❌ | Boost.SML, Boost.MSM |
| UML state chart compliance | Partial | Boost.MSM |
| Event queuing | Manual | Boost.SML |

### Alternative Libraries

**Boost.SML (State Machine Language)**
```cpp
// Boost.SML example - more features, steeper learning curve
#include <boost/sml.hpp>
namespace sml = boost::sml;

struct connect {};
struct disconnect {};
struct timeout {};

struct ConnectionSM {
  auto operator()() const {
    using namespace sml;
    return make_transition_table(
      *"idle"_s + event<connect> / start_connecting = "connecting"_s,
      "connecting"_s + event<timeout> = "failed"_s,
      "connecting"_s + on_entry<_> / [] { log("entering connecting"); },
      "connected"_s + event<disconnect> = "disconnecting"_s
    );
  }
};
```

**Pros:**
- Guards, actions, and events are first-class
- Hierarchical and orthogonal states
- UML-compliant
- Zero runtime overhead (like Fat-P)
- Excellent error messages

**Cons:**
- Heavy template metaprogramming (longer compile times)
- Steeper learning curve
- Larger dependency (Boost)

**Boost.MSM (Meta State Machine)**
- Full UML state chart support
- Even heavier compile times than SML
- More verbose syntax
- Best for complex, formally-specified state machines

**Hand-Rolled State Pattern (OOP)**
```cpp
// Classic GoF State pattern
class State {
public:
    virtual void handle(Context&, Event) = 0;
    virtual void on_entry(Context&) {}
    virtual void on_exit(Context&) {}
};

class Idle : public State { /* ... */ };
class Connecting : public State { /* ... */ };
```

**Pros:**
- Familiar OOP pattern
- Easy to understand
- No template complexity

**Cons:**
- Heap allocation per state (or complex pooling)
- Virtual dispatch overhead
- No compile-time transition validation
- Easy to forget entry/exit actions

### Decision Matrix

```
Start here:
    │
    ▼
Do you need hierarchical or parallel states?
    │
    ├─ Yes → Boost.SML or Boost.MSM
    │
    ▼ No
    │
Do you need guard conditions on many transitions?
    │
    ├─ Yes → Boost.SML (or add guards manually to Fat-P)
    │
    ▼ No
    │
More than ~20 states?
    │
    ├─ Yes → Consider Boost.SML or code generation
    │
    ▼ No
    │
Need minimal dependencies and fast compile?
    │
    ├─ Yes → Fat-P StateMachine ✓
    │
    ▼ No
    │
Boost.SML (more features, longer compile)
```

### Migration Path

If you start with Fat-P `StateMachine` and later need more features:

1. **Adding guards**: Implement in your transition methods
   ```cpp
   void request_connect() {
       if (!can_connect()) return;  // Manual guard
       sm_.transition<Connecting>();
   }
   ```

2. **Moving to Boost.SML**: The mental model is similar—states as types, transitions as declarations. The main work is learning SML's DSL syntax.

3. **Keeping both**: Use Fat-P for simple state machines, Boost.SML for complex ones. They can coexist in the same codebase.

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
