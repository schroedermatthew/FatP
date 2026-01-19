# Problem-Solving Session 4: The Invalid Operation

## Type-State Pattern: Compile-Time Protocol Enforcement

**Estimated time:** 45–60 minutes  
**Prerequisites:** Move semantics, templates  
**Fat-P components:** None (pure C++ design pattern)

---

## The Bug

Your team's file processing library has been deployed for months. Then a crash report comes in:

> "Application crashes with 'Bad file descriptor' when processing large batches."

You trace it to this code:

```cpp
class File {
    int fd_ = -1;
    bool is_open_ = false;
public:
    void open(const std::string& path) {
        fd_ = ::open(path.c_str(), O_RDONLY);
        is_open_ = (fd_ >= 0);
    }
    
    std::string read() {
        if (!is_open_) throw std::runtime_error("File not open");
        // ... read from fd_ ...
    }
    
    void close() {
        if (is_open_) {
            ::close(fd_);
            is_open_ = false;
        }
    }
};

void process_files(const std::vector<std::string>& paths) {
    File f;
    for (const auto& path : paths) {
        f.open(path);
        auto data = f.read();
        process(data);
        f.close();
    }
    f.read();  // BUG: file is closed, but compiles fine!
}
```

**The bugs:**
1. `read()` can be called before `open()` — runtime error
2. `read()` can be called after `close()` — runtime error
3. `open()` can be called twice without `close()` — resource leak
4. The compiler accepts all of these — they're only caught at runtime (maybe)

---

## Questions to Consider

Before reading further, think about:

1. **Q1:** Why can't we catch these bugs at compile time with the current design?
2. **Q2:** What if the state was encoded in the *type*, not a variable?
3. **Q3:** How can move semantics help enforce state transitions?
4. **Q4:** What are the trade-offs of this approach?

---

## Q1: The Runtime State Problem

The current design uses a **runtime flag** (`is_open_`) to track state:

```cpp
class File {
    bool is_open_ = false;  // State is a VALUE
    // ...
};
```

The compiler only sees one type: `File`. It has no way to know whether a particular `File` instance is open or closed. The `is_open_` flag is checked at runtime, not compile time.

**The fundamental problem:** The type system doesn't distinguish between a file that's open and one that's closed. They're both just `File`.

---

## Q2: State as Type

What if open and closed files were **different types**?

```cpp
class ClosedFile { /* ... */ };
class OpenFile { /* ... */ };
```

Now the compiler can enforce:
- `read()` only exists on `OpenFile`
- `close()` only exists on `OpenFile`
- `open()` only exists on `ClosedFile`

```cpp
ClosedFile f;
f.read();  // Compile error: ClosedFile has no member 'read'
```

**This is the Type-State Pattern:** States are types, and operations transform one type into another.

---

## Q3: The Type-State Pattern

### Basic Implementation

```cpp
#include <string>
#include <fcntl.h>
#include <unistd.h>

// Forward declarations
class OpenFile;
class ClosedFile;

class ClosedFile {
    std::string path_;
public:
    explicit ClosedFile(std::string path) : path_(std::move(path)) {}
    
    // open() consumes ClosedFile, returns OpenFile
    OpenFile open() &&;
    
    // Non-copyable, non-movable (or movable if you want)
    ClosedFile(const ClosedFile&) = delete;
    ClosedFile& operator=(const ClosedFile&) = delete;
};

class OpenFile {
    int fd_;
    
    // Private constructor — only ClosedFile::open() can create
    explicit OpenFile(int fd) : fd_(fd) {}
    friend class ClosedFile;
    
public:
    // close() consumes OpenFile, returns ClosedFile
    ClosedFile close() &&;
    
    // read() requires OpenFile — only available in this state
    std::string read() {
        char buf[4096];
        ssize_t n = ::read(fd_, buf, sizeof(buf));
        return std::string(buf, n > 0 ? n : 0);
    }
    
    // Non-copyable (can't duplicate file descriptor ownership)
    OpenFile(const OpenFile&) = delete;
    OpenFile& operator=(const OpenFile&) = delete;
    
    // Movable
    OpenFile(OpenFile&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    
    // Destructor closes if still open
    ~OpenFile() {
        if (fd_ >= 0) ::close(fd_);
    }
};

// Implementation of transitions
OpenFile ClosedFile::open() && {
    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0) throw std::runtime_error("Failed to open: " + path_);
    return OpenFile(fd);
}

ClosedFile OpenFile::close() && {
    ::close(fd_);
    fd_ = -1;
    return ClosedFile("");  // Could store path if needed
}
```

### Usage

```cpp
void process_file(const std::string& path) {
    ClosedFile closed(path);
    
    // Must call open() to get an OpenFile
    OpenFile open = std::move(closed).open();
    
    // Now we can read
    std::string data = open.read();
    process(data);
    
    // close() consumes the OpenFile
    ClosedFile closed2 = std::move(open).close();
    
    // Trying to read after close:
    open.read();  // Compile error? No — but open is moved-from
}
```

**Wait, that last line compiles!** The moved-from `open` is still in scope. This is a weakness of the basic pattern.

### Strengthening with `[[nodiscard]]` and Naming

```cpp
class OpenFile {
public:
    [[nodiscard]] ClosedFile close() &&;
    // ...
};

// Better: make the transition explicit in naming
void process_file(const std::string& path) {
    auto file = ClosedFile(path).open();  // Direct chaining
    std::string data = file.read();
    process(data);
    auto _ = std::move(file).close();  // Explicit consumption
    
    // file is now moved-from — using it is a logic error
    // (Some static analyzers can catch use-after-move)
}
```

---

## Q4: A More Complete Example

### Connection State Machine

```cpp
#include <memory>
#include <string>

// Forward declarations
class Disconnected;
class Connecting;
class Connected;

// Shared context (connection parameters, socket, etc.)
struct ConnectionContext {
    std::string host;
    int port;
    int socket_fd = -1;
};

class Disconnected {
    std::shared_ptr<ConnectionContext> ctx_;
public:
    explicit Disconnected(std::string host, int port)
        : ctx_(std::make_shared<ConnectionContext>()) {
        ctx_->host = std::move(host);
        ctx_->port = port;
    }
    
    // Transition: Disconnected → Connecting
    Connecting connect() &&;
    
    // No send/receive — these operations don't exist in this state
};

class Connecting {
    std::shared_ptr<ConnectionContext> ctx_;
    friend class Disconnected;
    explicit Connecting(std::shared_ptr<ConnectionContext> ctx) 
        : ctx_(std::move(ctx)) {}
public:
    // Transition: Connecting → Connected (on success)
    Connected wait_for_connection() &&;
    
    // Transition: Connecting → Disconnected (on cancel/failure)
    Disconnected cancel() &&;
};

class Connected {
    std::shared_ptr<ConnectionContext> ctx_;
    friend class Connecting;
    explicit Connected(std::shared_ptr<ConnectionContext> ctx) 
        : ctx_(std::move(ctx)) {}
public:
    // Operations only available when connected
    void send(const std::string& data) {
        // ... send on ctx_->socket_fd ...
    }
    
    std::string receive() {
        // ... receive from ctx_->socket_fd ...
        return "";
    }
    
    // Transition: Connected → Disconnected
    Disconnected disconnect() &&;
};

// Implementations
Connecting Disconnected::connect() && {
    // Start async connection...
    return Connecting(std::move(ctx_));
}

Connected Connecting::wait_for_connection() && {
    // Block until connected, throw on failure
    return Connected(std::move(ctx_));
}

Disconnected Connecting::cancel() && {
    // Cancel connection attempt
    return Disconnected(ctx_->host, ctx_->port);
}

Disconnected Connected::disconnect() && {
    // Close socket
    return Disconnected(ctx_->host, ctx_->port);
}
```

### What the Compiler Now Enforces

```cpp
void example() {
    Disconnected d("example.com", 80);
    
    d.send("hello");  // Compile error: Disconnected has no member 'send'
    
    auto connecting = std::move(d).connect();
    connecting.send("hello");  // Compile error: Connecting has no member 'send'
    
    auto connected = std::move(connecting).wait_for_connection();
    connected.send("hello");  // OK!
    
    auto disconnected = std::move(connected).disconnect();
    connected.send("hello");  // Compiles (moved-from), but:
                              // - Static analyzers flag this
                              // - Runtime: likely crash or no-op
}
```

---

## Template-Based Type-State

For more complex state machines, templates provide flexibility:

```cpp
// State tags
struct Closed {};
struct Open {};
struct Locked {};

template<typename State>
class File {
    int fd_ = -1;
    
    template<typename> friend class File;
    
    // Private constructor for state transitions
    explicit File(int fd) : fd_(fd) {}
    
public:
    // Public constructor only for initial state
    explicit File(const std::string& path) requires std::is_same_v<State, Closed>
        : fd_(-1) {}
    
    // Transitions
    File<Open> open() && requires std::is_same_v<State, Closed> {
        fd_ = ::open(/* ... */);
        return File<Open>(fd_);
    }
    
    File<Closed> close() && requires std::is_same_v<State, Open> {
        ::close(fd_);
        return File<Closed>(-1);
    }
    
    File<Locked> lock() && requires std::is_same_v<State, Open> {
        // ... acquire lock ...
        return File<Locked>(fd_);
    }
    
    File<Open> unlock() && requires std::is_same_v<State, Locked> {
        // ... release lock ...
        return File<Open>(fd_);
    }
    
    // Operations only in certain states
    std::string read() requires std::is_same_v<State, Open> {
        // ...
    }
    
    void write(const std::string& data) requires std::is_same_v<State, Locked> {
        // Only writable when locked
    }
};

// Usage
void example() {
    File<Closed> f("data.txt");
    
    auto open = std::move(f).open();
    auto data = open.read();  // OK
    
    auto locked = std::move(open).lock();
    locked.write("new data");  // OK
    locked.read();  // Compile error: read() not available in Locked state
    
    auto open2 = std::move(locked).unlock();
    auto closed = std::move(open2).close();
}
```

---

## Trade-offs

### Advantages

| Benefit | Description |
|---------|-------------|
| **Compile-time safety** | Invalid operations don't compile |
| **Self-documenting** | Type signatures show valid operations |
| **No runtime overhead** | States are compile-time only |
| **IDE support** | Autocomplete only shows valid operations |

### Disadvantages

| Drawback | Description |
|----------|-------------|
| **Verbose** | Multiple classes instead of one |
| **Move semantics required** | Must use `std::move` for transitions |
| **Moved-from objects** | Still in scope, can be misused |
| **Dynamic state difficult** | Hard when state isn't known at compile time |
| **Container challenges** | Can't have `vector<File>` with mixed states |

### When to Use Type-State

**Good fit:**
- Protocol enforcement (connect before send)
- Resource lifecycle (open/read/close)
- Builder pattern (required configuration steps)
- Security-sensitive workflows (authenticate before access)

**Poor fit:**
- State determined at runtime (user input)
- Many states with complex transitions
- Need to store objects of different states together
- Rapid prototyping (too much ceremony)

---

## Comparison with StateMachine

| Aspect | Type-State Pattern | Fat-P StateMachine |
|--------|-------------------|-------------------|
| State storage | Compile-time (in type) | Runtime (variant) |
| Invalid transitions | Won't compile | Runtime error or static_assert |
| Entry/exit actions | Manual | Automatic |
| Dynamic state | Difficult | Easy |
| Container storage | One type per state | Single type |
| Complexity | Simpler for linear flows | Better for complex graphs |

**Use Type-State when:** You want maximum compile-time safety and have a simple, linear protocol.

**Use StateMachine when:** You need runtime state inspection, automatic entry/exit actions, or complex transition graphs.

---

## Summary

| Problem | Solution |
|---------|----------|
| Operations valid only in certain states | Different types for each state |
| Runtime state checks | Compile-time type checks |
| Can forget to check state | Operations don't exist on wrong type |
| Protocol violations | State transitions return new types |

### Key Principles

1. **State is type, not value** — `OpenFile` vs `ClosedFile`, not `file.is_open`
2. **Transitions consume and produce** — `open()` takes `ClosedFile`, returns `OpenFile`
3. **Operations exist only on valid states** — `read()` only on `OpenFile`
4. **Move semantics enforce linearity** — can't use object after transition

### The Pattern in One Sentence

> Encode state in the type system so that invalid operations become compile errors, not runtime errors.

---

## Exercises

1. **Warm-up:** Implement a `Door` with states `Locked`, `Closed`, `Open`. Ensure you can't lock an open door or open a locked door without unlocking first.

2. **Builder pattern:** Create a `HttpRequestBuilder` where `build()` is only available after both `url()` and `method()` have been called. Use type-state to enforce this at compile time.

3. **Resource wrapper:** Implement a type-safe `MutexGuard` where the protected data is only accessible when the lock is held.

4. **Compare:** Implement the same state machine using both Type-State and Fat-P `StateMachine`. Compare the code size, safety guarantees, and usability.

---

## Further Reading

**Papers:**
- "Typestate: A Programming Language Concept for Enhancing Software Reliability" (Strom & Yemini, 1986)
- "Typestates for Objects" (DeLine & Fähndrich, 2004)

**Languages with built-in typestate:**
- Rust's ownership system enforces similar patterns
- Plaid (research language with first-class typestate)

**C++ resources:**
- "Enforcing Correct Mutex Usage with Synchronized Values" — Andrei Alexandrescu
- Matt Godbolt's CppCon talks on type safety
