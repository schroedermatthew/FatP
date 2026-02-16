---
doc_id: MG-STRONGID-001
doc_type: "Migration Guide"
title: "Integer Handles to Type-Safe IDs"
from_pattern: "Raw integer handles, file descriptors, opaque int IDs"
to_component: "StrongId"
fatp_version: "1.0"
cxx_standard: "C++20"
migration_complexity: "Low"
breaking_changes: false
last_verified: "2025-01-08"
fatp_components: ["StrongId"]
topics: ["c-to-cpp", "migration", "integer-handles", "type-safety", "handle-confusion", "phantom-types"]
constraints: ["cross-domain handle confusion", "implicit conversion", "type-level safety"]
audience: ["C developers", "C++ developers", "AI assistants"]
status: "draft"
---

# Migration Guide - Integer Handles to Type-Safe IDs

### *From `int`, `size_t`, `HANDLE` to `StrongId<T, Tag>`*

*FAT-P Library — January 2025*

---

## Scope

This guide targets C code that uses raw integers or typedef'd integers as handles (file descriptors, connection IDs, resource handles) and migrates those to `StrongId<T, Tag>` for compile-time type distinction.

## Not covered

- Handle lifetime management (open/close pairing — see ScopeGuard or RAII guide)
- Handle serialization across process boundaries
- OS-specific handle types (`HANDLE`, `SOCKET`) with platform abstraction

## Prerequisites

- Familiarity with integer handle patterns in C APIs
- Basic understanding of C++ templates and tag types

## Migration Guide Card

**From:** Integer handles (`int fd`, `HANDLE`), typedef'd integers  
**To:** `StrongId<T, Tag, CheckPolicy, OpPolicy>` for type-distinguished handles  
**Why migrate:** Typedef'd integers allow cross-type confusion — passing a file descriptor where a socket handle is expected compiles silently  
**Compatibility strategy:** Incremental — replace `typedef int` with `StrongId`; explicit construction prevents implicit conversion  
**Mechanical steps:**
1. Identify integer handle types and their distinct domains.
2. Create `StrongId` aliases with distinct tag types.
3. Replace integer declarations with `StrongId` at declaration and call sites.
4. Fix compilation errors from implicit conversions (these are the bugs being caught).
**Behavioral equivalence:** Same handle values, same operations; zero runtime overhead  
**Intentional differences:** Cross-domain handle confusion is a compile-time error  
**Failure model:** Invalid handle use → compile error (not runtime check)  
**Threading model:** Unchanged — `StrongId` is a value type with no synchronization requirements  
**Lifetime model:** Value semantics; same lifetime rules as the underlying integer  
**Alternatives:** Scoped enums, `enum class`-based handles, manual wrapper structs  
**Verification:** Compile-time verification — wrong-handle-type bugs become compilation failures  
**Rollback plan:** Replace `StrongId` aliases with `typedef int`; remove explicit construction

---

## Alternatives

`enum class` (provides type distinction but no policy control), manual wrapper `struct` (boilerplate-heavy, no compile-time policy), Boost.StrongTypedef (similar concept, Boost dependency).

## Mapping: From → To

| C Pattern | C++ Replacement | Notes |
|-----------|----------------|-------|
| `typedef int FileId;` | `using FileId = StrongId<int, FileIdTag>;` | Cross-type confusion is compile error |
| `int fd` parameter | `FileId fd` parameter | Wrong-handle-type is compile error |
| `handle == other_handle` | Same — `StrongId` supports `==` | Cross-type comparison is compile error |
| `handle + 1` (arithmetic) | Blocked by default policy | Accidental arithmetic is compile error |

## Compatibility and ABI boundaries

At C API boundaries, use `.value()` to extract the underlying integer for C function calls. Wrap returned integers with explicit `StrongId` construction at the boundary.

## Lifetime and ownership model

Value semantics. Same lifetime as the underlying integer. No ownership, no teardown ordering. Copies are independent values.

## Thread-safety and reentrancy

`StrongId` is a value type with no internal state beyond the integer. Thread-safe to the same degree as `int` — concurrent reads are safe; concurrent writes require synchronization.

## Rollback plan

Replace `StrongId` aliases with `typedef int`. Remove explicit construction. Restore implicit integer conversions. Type-safety guarantees are lost on rollback.

## Table of Contents

1. [The Problem with Integer Handles](#the-problem-with-integer-handles)
2. [Real-World Bugs from Handle Confusion](#real-world-bugs-from-handle-confusion)
3. [The C Patterns](#the-c-patterns)
4. [The StrongId Solution](#the-strongid-solution)
5. [Migration Steps](#migration-steps)
6. [Before/After Examples](#beforeafter-examples)
7. [Advanced Patterns](#advanced-patterns)
8. [Verification](#verification)
9. [When StrongId Loses](#when-strongid-loses)

---

## The Problem with Integer Handles

Integer handles are ubiquitous in systems programming:

```c
int fd = open("/etc/passwd", O_RDONLY);
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
sqlite3* db;  // Actually an opaque pointer, but often treated like a handle
HANDLE file = CreateFile(...);  // Windows: typedef void* HANDLE
```

The problem: **they're all just numbers**. The compiler can't distinguish between them:

```c
int log_fd = open("app.log", O_WRONLY);
int config_fd = open("config.txt", O_RDONLY);
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

// Compiles fine. All are just 'int'. All are bugs.
close(socket_fd);
read(log_fd, buf, sizeof(buf));   // Oops: meant config_fd
write(config_fd, data, len);       // Oops: meant log_fd
close(socket_fd);                  // Double close!
```

This class of bug is:
- **Silent** — No compiler warning, no runtime error (usually)
- **Intermittent** — Depends on which file descriptors are allocated
- **Devastating** — Can corrupt unrelated files, leak connections, or crash

---

## Real-World Bugs from Handle Confusion

### The Android fdsan Story

Android introduced "fdsan" (file descriptor sanitizer) because handle confusion bugs were so common and damaging. From the [Android documentation](https://android.googlesource.com/platform/bionic/+/master/docs/fdsan.md):

```c
// Thread 1                          // Thread 2
int fd = open("/dev/null", O_RDONLY);
// fd = 123
close(fd);
                                      int fd2 = open("log", O_WRONLY);
                                      // fd2 = 123 (reused!)
close(fd);  // Double close!
                                      write(fd2, "foo", 3);  // EBADF!
                                      // Or worse: writes to wrong file
```

The quote from Android's documentation: "Assertion failures are probably the most innocuous result that can arise from these bugs: silent data corruption or security vulnerabilities are also possible."

### The POSIX close() Race

From the [Linux man pages](https://man7.org/linux/man-pages/man2/close.2.html):

> "Retrying the close() after a failure return is the wrong thing to do, since this may cause a reused file descriptor from another thread to be closed."

This is a fundamental design issue with integer handles: the number can be reused immediately after close, making double-close bugs catastrophic in multithreaded code.

### SQLite's Solution

SQLite uses opaque pointer handles (`sqlite3*`, `sqlite3_stmt*`) precisely to prevent this:

```c
/* From sqlite3.h */
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;
typedef struct sqlite3_context sqlite3_context;
```

You can't accidentally pass a `sqlite3*` where a `sqlite3_stmt*` is expected—they're different pointer types. This is the same principle as `StrongId`, applied to pointers.

---

## The C Patterns

### Pattern 1: Raw Integer File Descriptors

**Source:** POSIX systems programming

```c
/* File operations */
int fd = open("data.txt", O_RDONLY);
ssize_t n = read(fd, buffer, sizeof(buffer));
close(fd);

/* Socket operations */
int sock = socket(AF_INET, SOCK_STREAM, 0);
connect(sock, &addr, sizeof(addr));
send(sock, data, len, 0);
close(sock);  // Same 'close' for completely different resource types!
```

**Problems:**
- `fd` and `sock` are interchangeable at the type level
- No distinction between "file" and "socket" handles
- Integer arithmetic is allowed (`fd + 1` compiles)
- Magic numbers everywhere (`-1` for invalid)

### Pattern 2: Typedef'd Integer IDs

```c
typedef int UserId;
typedef int ProductId;
typedef int OrderId;

void process_order(OrderId order, UserId user, ProductId product) {
    // ...
}

// Compiles fine, but arguments are swapped:
process_order(user_id, product_id, order_id);  // Bug!
```

**Problems:**
- `typedef` creates an alias, not a new type
- Compiler treats `UserId`, `ProductId`, `OrderId` as identical
- No protection against argument swapping

### Pattern 3: Handle + Invalid Sentinel

```c
#define INVALID_HANDLE -1

int acquire_resource() {
    int handle = allocate();
    if (handle < 0) return INVALID_HANDLE;
    return handle;
}

void use_resource(int handle) {
    if (handle == INVALID_HANDLE) return;  // Must check everywhere
    // ...
}
```

**Problems:**
- Sentinel value is just a convention, not enforced
- Every function must check for invalid
- `-1` is a valid result for some operations

### Pattern 4: Windows HANDLE

```c
/* Windows handles are typedef'd void* */
HANDLE file = CreateFile(...);
HANDLE process = OpenProcess(...);
HANDLE thread = CreateThread(...);

/* All have the same type! */
CloseHandle(file);
CloseHandle(process);  // Different resource, same function
CloseHandle(thread);   // Different resource, same function
```

**Problems:**
- Three completely different resource types share the same type
- Passing a file handle to a process function compiles without warning

---

## The StrongId Solution

### Basic Concept

`StrongId` wraps an integer with a unique tag type, creating a distinct type at compile time:

```cpp
#include "StrongId.h"

// Define tag types (empty structs)
struct FileDescriptorTag {};
struct SocketTag {};
struct UserIdTag {};
struct OrderIdTag {};

// Create distinct ID types
using FileDescriptor = fat_p::StrongId<int, FileDescriptorTag>;
using SocketHandle = fat_p::StrongId<int, SocketTag>;
using UserId = fat_p::StrongId<int, UserIdTag>;
using OrderId = fat_p::StrongId<int, OrderIdTag>;

void process_file(FileDescriptor fd);
void process_socket(SocketHandle sock);

int main() {
    FileDescriptor fd{3};
    SocketHandle sock{4};
    
    process_file(fd);     // OK
    process_file(sock);   // COMPILE ERROR: no matching function
    
    // Arithmetic is still available when needed
    FileDescriptor next = fd + 1;  // OK, returns FileDescriptor
    
    // But no implicit conversion
    int raw = fd;         // COMPILE ERROR: no implicit conversion
    int raw = fd.get();   // OK: explicit extraction
}
```

### Zero Runtime Overhead

`StrongId` compiles away completely. These produce identical machine code:

```cpp
// Raw integer
int add_raw(int a, int b) {
    return a + b;
}

// StrongId
using MyId = fat_p::StrongId<int, struct MyTag>;
MyId add_strong(MyId a, MyId b) {
    return a + b;
}
```

Both compile to:
```asm
lea eax, [rdi + rsi]
ret
```

The tag type exists only at compile time. At runtime, it's just an `int`.

### API Overview

```cpp
template <typename T, typename Tag, 
          typename CheckPolicy = NoCheckPolicy,
          template <typename> class OpPolicy = DefaultOpPolicy>
class StrongId {
public:
    using value_type = T;
    
    // Construction
    constexpr StrongId();                    // Default: initializes to 0
    explicit constexpr StrongId(T value);    // Explicit from raw value
    static Expected<StrongId, std::string> create(T value);  // Safe factory
    
    // Access
    [[nodiscard]] constexpr T get() const noexcept;
    [[nodiscard]] constexpr T value() const noexcept;
    [[nodiscard]] explicit constexpr operator T() const noexcept;
    
    // Comparison (full set)
    friend constexpr bool operator==(const StrongId&, const StrongId&);
    friend constexpr bool operator!=(const StrongId&, const StrongId&);
    friend constexpr bool operator<(const StrongId&, const StrongId&);
    // ... etc, including <=> for C++20
    
    // Arithmetic (checked by default)
    constexpr StrongId& operator++();
    constexpr StrongId& operator--();
    constexpr StrongId& operator+=(const StrongId&);
    // ... full arithmetic support
    
    // Hashable (works with std::unordered_map)
};
```

---

## Migration Steps

### Step 1: Identify Handle Types

Find all integer handles in your codebase:

```bash
# Common patterns to search for
grep -rn "typedef.*int.*Id" src/
grep -rn "typedef.*int.*Handle" src/
grep -rn "int fd" src/
grep -rn "int socket" src/
grep -rn "= open(" src/
grep -rn "= socket(" src/
```

Group handles by semantic meaning:
- File descriptors
- Socket handles
- Database connection IDs
- User/Order/Product IDs
- Session tokens
- etc.

### Step 2: Define Tag Types and Aliases

Create a header for your ID types:

```cpp
// ids.h
#pragma once
#include "StrongId.h"

namespace myapp {

// Tag types (can be empty structs or forward declarations)
struct FileDescriptorTag {};
struct SocketHandleTag {};
struct UserIdTag {};
struct SessionIdTag {};

// ID type aliases
using FileDescriptor = fat_p::StrongId<int, FileDescriptorTag>;
using SocketHandle = fat_p::StrongId<int, SocketHandleTag>;
using UserId = fat_p::StrongId<uint64_t, UserIdTag>;
using SessionId = fat_p::StrongId<uint64_t, SessionIdTag>;

// Invalid handle constants
inline constexpr FileDescriptor InvalidFd{-1};
inline constexpr SocketHandle InvalidSocket{-1};
inline constexpr UserId InvalidUserId{0};

}  // namespace myapp
```

### Step 3: Update Function Signatures

Change function parameters and return types:

```cpp
// Before
int open_file(const char* path);
void close_file(int fd);
ssize_t read_file(int fd, void* buf, size_t len);

// After
FileDescriptor open_file(const char* path);
void close_file(FileDescriptor fd);
ssize_t read_file(FileDescriptor fd, void* buf, size_t len);
```

### Step 4: Fix Compilation Errors

The compiler will flag every type mismatch:

```cpp
// This now fails to compile:
FileDescriptor fd = open_file("data.txt");
close_file(socket);  // Error: SocketHandle != FileDescriptor

// Explicit construction required at boundaries:
FileDescriptor fd{::open("data.txt", O_RDONLY)};  // Wrap raw syscall result
::close(fd.get());  // Unwrap for raw syscall
```

### Step 5: Add Validation Policies (Optional)

For IDs with invariants, add a check policy:

```cpp
// User IDs must be positive
struct PositiveUserIdPolicy {
    static constexpr void check(uint64_t value) {
        if (value == 0) {
            throw std::invalid_argument("User ID cannot be zero");
        }
    }
};

using UserId = fat_p::StrongId<uint64_t, UserIdTag, PositiveUserIdPolicy>;

UserId valid{42};    // OK
UserId invalid{0};   // Throws std::invalid_argument
```

### Step 6: Use Safe Factory for Untrusted Input

```cpp
// User input or deserialization
Expected<UserId, std::string> result = UserId::create(user_input);
if (!result) {
    log_error("Invalid user ID: {}", result.error());
    return;
}
UserId id = *result;
```

---

## Before/After Examples

### Example 1: File Descriptor Management

**Before (C-style):**
```c
typedef int FileHandle;
#define INVALID_HANDLE -1

FileHandle open_log(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT, 0644);
    return fd;  // -1 on error
}

void write_log(FileHandle log, const char* msg) {
    if (log == INVALID_HANDLE) return;
    write(log, msg, strlen(msg));
}

void close_log(FileHandle log) {
    if (log != INVALID_HANDLE) {
        close(log);
    }
}

// Bug: socket and log_fd are both 'int'
void handle_request(int socket, FileHandle log_fd) {
    write_log(socket, "Processing...");  // BUG: compiles fine!
}
```

**After (StrongId):**
```cpp
#include "StrongId.h"

struct LogFileTag {};
struct ClientSocketTag {};

using LogFile = fat_p::StrongId<int, LogFileTag>;
using ClientSocket = fat_p::StrongId<int, ClientSocketTag>;

inline constexpr LogFile InvalidLogFile{-1};
inline constexpr ClientSocket InvalidSocket{-1};

LogFile open_log(const char* path) {
    int fd = ::open(path, O_WRONLY | O_CREAT, 0644);
    return LogFile{fd};
}

void write_log(LogFile log, const char* msg) {
    if (log == InvalidLogFile) return;
    ::write(log.get(), msg, strlen(msg));
}

void close_log(LogFile log) {
    if (log != InvalidLogFile) {
        ::close(log.get());
    }
}

// Now a compile error:
void handle_request(ClientSocket socket, LogFile log_fd) {
    write_log(socket, "Processing...");  // ERROR: no conversion
    write_log(log_fd, "Processing...");  // OK
}
```

### Example 2: Database Entity IDs

**Before (typedef):**
```cpp
typedef int64_t UserId;
typedef int64_t ProductId;
typedef int64_t OrderId;

void create_order(UserId user, ProductId product, int quantity);

// Bug: argument order swapped
create_order(product_id, user_id, 1);  // Compiles!
```

**After (StrongId):**
```cpp
struct UserIdTag {};
struct ProductIdTag {};
struct OrderIdTag {};

using UserId = fat_p::StrongId<int64_t, UserIdTag>;
using ProductId = fat_p::StrongId<int64_t, ProductIdTag>;
using OrderId = fat_p::StrongId<int64_t, OrderIdTag>;

void create_order(UserId user, ProductId product, int quantity);

// Now a compile error:
UserId user{42};
ProductId product{100};
create_order(product, user, 1);  // ERROR: ProductId != UserId
create_order(user, product, 1);  // OK
```

### Example 3: Connection Pool with Validation

**Before:**
```cpp
class ConnectionPool {
    std::vector<Connection> connections_;
public:
    int acquire() {
        // Returns index into connections_
        for (size_t i = 0; i < connections_.size(); ++i) {
            if (connections_[i].available) {
                connections_[i].available = false;
                return static_cast<int>(i);
            }
        }
        return -1;  // No connection available
    }
    
    void release(int handle) {
        if (handle >= 0 && handle < connections_.size()) {
            connections_[handle].available = true;
        }
    }
    
    Connection& get(int handle) {
        return connections_[handle];  // No bounds check!
    }
};

// Bug: using wrong handle type
pool.release(socket_fd);  // Oops, socket_fd is not a pool handle
```

**After (StrongId with validation):**
```cpp
struct ConnectionHandleTag {};

// Policy: handle must be non-negative
struct ValidConnectionPolicy {
    static void check(int value) {
        if (value < -1) {  // -1 is allowed as "invalid"
            throw std::invalid_argument("Invalid connection handle");
        }
    }
};

using ConnectionHandle = fat_p::StrongId<int, ConnectionHandleTag, ValidConnectionPolicy>;
inline constexpr ConnectionHandle InvalidConnection{-1};

class ConnectionPool {
    std::vector<Connection> connections_;
public:
    ConnectionHandle acquire() {
        for (size_t i = 0; i < connections_.size(); ++i) {
            if (connections_[i].available) {
                connections_[i].available = false;
                return ConnectionHandle{static_cast<int>(i)};
            }
        }
        return InvalidConnection;
    }
    
    void release(ConnectionHandle handle) {
        if (handle != InvalidConnection) {
            size_t idx = static_cast<size_t>(handle.get());
            if (idx < connections_.size()) {
                connections_[idx].available = true;
            }
        }
    }
    
    Connection& get(ConnectionHandle handle) {
        FATP_ENFORCE(handle != InvalidConnection, "Invalid handle");
        return connections_.at(static_cast<size_t>(handle.get()));
    }
};

// Now a compile error:
pool.release(socket_fd);  // ERROR: int != ConnectionHandle
```

---

## Advanced Patterns

### Pattern: ID Generation with AtomicStrongId

```cpp
struct EntityIdTag {};
using EntityId = fat_p::StrongId<uint64_t, EntityIdTag>;
using AtomicEntityId = fat_p::AtomicStrongId<uint64_t, EntityIdTag>;

class EntityIdGenerator {
    AtomicEntityId mNextId{EntityId{1}};
public:
    EntityId next() {
        EntityId current = mNextId.load();
        while (!mNextId.compare_exchange_weak(current, current + 1)) {
            // Retry on contention
        }
        return current;
    }
};
```

### Pattern: Wrapping System Calls

```cpp
// safe_fd.h
#include "StrongId.h"
#include "ScopeGuard.h"

struct FileDescriptorTag {};
using Fd = fat_p::StrongId<int, FileDescriptorTag>;
inline constexpr Fd InvalidFd{-1};

// RAII wrapper combining StrongId + ScopeGuard
class OwnedFd {
    Fd mFd;
public:
    explicit OwnedFd(Fd fd) : mFd(fd) {}
    ~OwnedFd() { if (mFd != InvalidFd) ::close(mFd.get()); }
    
    OwnedFd(OwnedFd&& other) noexcept : mFd(other.mFd) { other.mFd = InvalidFd; }
    OwnedFd& operator=(OwnedFd&&) noexcept;
    
    OwnedFd(const OwnedFd&) = delete;
    OwnedFd& operator=(const OwnedFd&) = delete;
    
    [[nodiscard]] Fd get() const noexcept { return mFd; }
    [[nodiscard]] Fd release() noexcept { Fd f = mFd; mFd = InvalidFd; return f; }
};

// Safe open that returns RAII handle
Expected<OwnedFd, int> safe_open(const char* path, int flags) {
    int raw = ::open(path, flags);
    if (raw < 0) return make_unexpected(errno);
    return OwnedFd{Fd{raw}};
}
```

### Pattern: Serialization-Friendly IDs

```cpp
// IDs often need to be serialized (JSON, protobuf, database)
struct UserIdTag {};
using UserId = fat_p::StrongId<uint64_t, UserIdTag>;

// JSON serialization (example with nlohmann::json)
void to_json(nlohmann::json& j, const UserId& id) {
    j = id.get();
}

void from_json(const nlohmann::json& j, UserId& id) {
    id = UserId{j.get<uint64_t>()};
}

// Database binding
void bind_param(Statement& stmt, int index, UserId id) {
    stmt.bind_int64(index, id.get());
}

UserId get_user_id(Statement& stmt, int column) {
    return UserId{stmt.get_int64(column)};
}
```

### Pattern: StrongId with Unchecked Operations for Performance

```cpp
// When you've validated inputs and need maximum performance
struct HotPathIdTag {};
using HotPathId = fat_p::StrongId<uint32_t, HotPathIdTag, 
                                   fat_p::NoCheckPolicy,
                                   fat_p::UncheckedOpPolicy>;

void process_batch(const std::vector<HotPathId>& ids) {
    for (auto id : ids) {
        // Arithmetic compiles to raw instructions, no overflow checks
        HotPathId next = id + 1;
        // ...
    }
}
```

---

## Verification

### Compile-Time Verification

The primary verification is **compilation**. After migration, these should fail:

```cpp
UserId user{1};
ProductId product{2};

// Must not compile:
user = product;                        // Different types
void f(UserId); f(product);           // Wrong argument type
std::map<UserId, Data> m; m[product]; // Wrong key type
user + product;                        // Can't mix types
```

### Runtime Verification

Test ID generation and comparison:

```cpp
TEST(StrongIdMigration, BasicOperations) {
    UserId a{1};
    UserId b{2};
    UserId c{1};
    
    EXPECT_EQ(a, c);
    EXPECT_NE(a, b);
    EXPECT_LT(a, b);
    
    // Increment
    UserId d = a + 1;
    EXPECT_EQ(d, b);
    
    // Hashable
    std::unordered_set<UserId> set;
    set.insert(a);
    set.insert(b);
    EXPECT_EQ(set.size(), 2);
    EXPECT_TRUE(set.count(a));
}

TEST(StrongIdMigration, ValidationPolicy) {
    using ValidUserId = fat_p::StrongId<uint64_t, UserIdTag, PositiveCheckPolicy>;
    
    EXPECT_NO_THROW(ValidUserId{1});
    EXPECT_THROW(ValidUserId{0}, std::invalid_argument);
    
    auto result = ValidUserId::create(0);
    EXPECT_FALSE(result.has_value());
}

TEST(StrongIdMigration, NoMixing) {
    // This test documents compile-time guarantees
    // If any of these compile, the test fails
    
    // UserId u{1};
    // ProductId p{2};
    // u = p;  // Should not compile
    // auto x = u + p;  // Should not compile
    
    SUCCEED();  // Compile-time check only
}
```

### Performance Verification

```cpp
// Benchmark: StrongId vs raw int
static void BM_RawInt(benchmark::State& state) {
    int sum = 0;
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            sum += i;
        }
        benchmark::DoNotOptimize(sum);
    }
}

static void BM_StrongId(benchmark::State& state) {
    using Id = fat_p::StrongId<int, struct BenchTag>;
    Id sum{0};
    for (auto _ : state) {
        for (int i = 0; i < 1000; ++i) {
            sum += i;
        }
        benchmark::DoNotOptimize(sum);
    }
}

// Both should produce identical results (within noise)
```

---

## When StrongId Loses

### 1. Interop with C APIs

System calls and C libraries expect raw integers:

```cpp
Fd fd{::open("file.txt", O_RDONLY)};  // Must wrap result
::read(fd.get(), buf, len);            // Must unwrap for call
```

**Mitigation:** Create wrapper functions or use the `OwnedFd` RAII pattern shown above.

### 2. Serialization Overhead

Every serialization point needs explicit conversion:

```cpp
// JSON: must explicitly convert
json["user_id"] = user.get();
UserId user{json["user_id"].get<uint64_t>()};
```

**Mitigation:** Add serialization overloads (see Pattern above).

### 3. Very Hot Loops with Overflow Checks

Default arithmetic includes overflow checking:

```cpp
// Each increment calls checked_add
for (MyId i{0}; i < MyId{1000000}; ++i) {  // Slight overhead
    // ...
}
```

**Mitigation:** Use `UncheckedOpPolicy` for validated hot paths.

### 4. Existing Codebase with Heavy Integer Arithmetic

If your code does complex arithmetic on IDs (unusual for handles, but possible for sequence numbers), migration requires touching many lines.

**Mitigation:** Migrate incrementally—start with the most bug-prone handle types.

---

## Summary

| Aspect | Before (int) | After (StrongId) |
|--------|-------------|------------------|
| Type safety | None | Full compile-time |
| Wrong-handle bugs | Silent runtime | Compile error |
| Double-close detection | None (crash/corruption) | Same, but distinct types help |
| Argument swapping | Silent runtime | Compile error |
| Performance | Baseline | Identical |
| Serialization | Direct | Explicit `.get()` |
| C interop | Direct | Explicit `.get()` |

The migration cost is low: mostly typedef changes and adding explicit construction at boundaries. The benefit is eliminating an entire class of silent, devastating bugs at compile time.

---

## References

- [Android fdsan documentation](https://android.googlesource.com/platform/bionic/+/master/docs/fdsan.md) — Motivation for handle tracking
- [POSIX close() issues](https://man7.org/linux/man-pages/man2/close.2.html) — Why integer handle reuse is dangerous
- [SQLite opaque handles](https://github.com/sqlite/sqlite/blob/master/src/sqlite3.h) — C-style type safety via distinct pointer types
- Fat-P User Manual: StrongId — Complete API reference

---

*FAT-P Library Documentation — January 2025*
