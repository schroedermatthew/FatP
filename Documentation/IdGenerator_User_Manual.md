# IdGenerator User Manual

**Version:** 2.0  
**Library:** C++ Utilities Library (fat_p)  
**Standard:** C++17  
**Type:** Header-only

---

## Table of Contents

1. [Overview](#overview)
2. [Key Features](#key-features)
3. [Quick Start](#quick-start)
4. [Core Concepts](#core-concepts)
5. [Policy System](#policy-system)
6. [API Reference](#api-reference)
7. [Usage Examples](#usage-examples)
8. [Thread Safety](#thread-safety)
9. [Error Handling](#error-handling)
10. [Performance Considerations](#performance-considerations)
11. [Best Practices](#best-practices)
12. [Integration with Other Components](#integration-with-other-components)

---

## Overview

`IdGenerator` is a policy-based unique identifier generator that provides type-safe ID generation with customizable allocation strategies, recycling policies, error handling, and concurrency control. The library follows a safety-first design philosophy with zero external dependencies.

### Design Philosophy

- **Policy-based design** for maximum flexibility
- **Type safety** through StrongId integration
- **Exception-free** error handling via Expected
- **Zero-cost abstractions** through template metaprogramming
- **Header-only** for ease of integration
- **Thread-safe** options available

### When to Use IdGenerator

Use `IdGenerator` when you need:
- Unique identifier generation for resources, sessions, or entities
- Type-safe IDs that prevent mixing different ID types
- Automatic ID recycling to prevent resource exhaustion
- Thread-safe concurrent ID generation
- Deterministic sequential or random ID allocation
- Integration with modern C++ error handling (Expected)

---

## Key Features

### ✅ Multiple Allocation Strategies

- **Sequential Allocation**: Monotonically increasing IDs
- **Random Allocation**: Cryptographically unpredictable IDs
- **Custom Policies**: Implement your own allocation strategy

### ✅ Flexible Recycling

- **Immediate Recycling**: Released IDs available for reuse
- **No Recycling**: Always generate fresh IDs
- **Custom Policies**: Implement delayed or priority-based recycling

### ✅ Type Safety

- Integration with `StrongId` for compile-time type checking
- Prevents accidental mixing of different ID types
- Works with raw integral types

### ✅ Thread Safety

- Single-threaded (no overhead)
- Mutex-based synchronization
- Custom concurrency policies supported

### ✅ Robust Error Handling

- `Expected<T, E>` based error reporting
- No exceptions thrown in normal operation
- Comprehensive error types

### ✅ RAII Support

- `IdGuard` for automatic ID release
- Move semantics support
- Exception-safe cleanup

---

## Quick Start

### Basic Sequential Generator

```cpp
#include "IdGenerator.h"

using namespace fat_p;

// Create a simple sequential ID generator starting at 1
SimpleIdGenerator<uint64_t> gen(1);

// Generate IDs
auto id1 = gen.generate();
if (id1) {
    std::cout << "Generated ID: " << *id1 << "\n";  // 1
}

auto id2 = gen.generate();  // 2
auto id3 = gen.generate();  // 3

// Release an ID for recycling
gen.release(*id1);

// Next generation reuses the recycled ID
auto id4 = gen.generate();  // 1 (recycled)
```

### Thread-Safe Generator

```cpp
// Use ThreadSafeIdGenerator for concurrent access
ThreadSafeIdGenerator<uint64_t> safe_gen(100);

// Safe to call from multiple threads
auto id = safe_gen.generate();
```

### Type-Safe IDs with StrongId

```cpp
// Define strongly-typed IDs
using UserId = StrongId<uint64_t, struct UserTag>;
using SessionId = StrongId<uint64_t, struct SessionTag>;

IdGenerator<UserId> user_gen(1000);
IdGenerator<SessionId> session_gen(5000);

auto user = user_gen.generate();      // UserId(1000)
auto session = session_gen.generate(); // SessionId(5000)

// Compile error: cannot mix types
// session_gen.release(*user);  // ❌ Type mismatch
```

### RAII with IdGuard

```cpp
SimpleIdGenerator<uint64_t> gen(1);

{
    auto guard_result = gen.scoped_id();
    if (guard_result) {
        auto& guard = *guard_result;
        uint64_t id = guard.get();
        // Use id...
    } // ID automatically released here
}
```

---

## Core Concepts

### ID Lifecycle

```
┌─────────────┐
│  Generate   │ ──────> ID enters "active" state
└─────────────┘
       │
       ├──> Use ID in application
       │
       v
┌─────────────┐
│   Release   │ ──────> ID enters "recycled" state (if policy allows)
└─────────────┘
       │
       v
┌─────────────┐
│   Reuse     │ ──────> ID returned to "active" state
└─────────────┘
```

### State Management

The generator maintains two primary sets:

1. **Active IDs** (`ids_in_use_`): Currently allocated and in use
2. **Recycled IDs**: Released IDs available for reuse

### Base ID

The `base_id` parameter specifies the starting point for ID generation:

```cpp
SimpleIdGenerator<uint64_t> gen(100);
// First ID will be 100, then 101, 102, ...
```

---

## Policy System

`IdGenerator` is highly customizable through four policy dimensions:

### Template Signature

```cpp
template <
    typename IdType_,
    typename AllocationPolicy = SequentialAllocationPolicy<...>,
    typename RecyclingPolicy = ImmediateRecyclingPolicy<...>,
    typename ErrorPolicy = ExpectedErrorPolicy<...>,
    typename ConcurrencyPolicy = SingleThreadedPolicy
>
class IdGenerator;
```

### 1. Allocation Policy

Controls how new IDs are generated.

#### SequentialAllocationPolicy

Generates IDs in monotonically increasing order.

```cpp
template <typename IdType = uint64_t>
class SequentialAllocationPolicy {
public:
    explicit SequentialAllocationPolicy(IdType base_id = 0);
    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept;
    void reset(IdType base_id = 0) noexcept;
};
```

**Behavior:**
- On first call: returns `base_id`
- On subsequent calls: returns `max(next_id, max_existing_id + 1)`
- Ensures no backwards movement in the ID space
- Respects recycling gaps

**Example:**
```cpp
SimpleIdGenerator<uint64_t> gen(1);
auto id1 = gen.generate();  // 1
auto id2 = gen.generate();  // 2
auto id3 = gen.generate();  // 3
gen.release(*id2);          // Release 2
auto id4 = gen.generate();  // 2 (recycled)
auto id5 = gen.generate();  // 4 (continues sequence)
```

#### RandomAllocationPolicy

Generates IDs using uniform random distribution.

```cpp
template <typename IdType = uint64_t>
class RandomAllocationPolicy {
public:
    explicit RandomAllocationPolicy(IdType = 0);
    std::optional<IdType> next_id(IdType, bool = false) noexcept;
    void reset(IdType = 0) noexcept;
};
```

**Behavior:**
- Generates random IDs from 0 to `std::numeric_limits<IdType>::max()`
- Collision detection via `ids_in_use_` set
- Returns `AlreadyInUse` error on collision
- Suitable for security-sensitive applications

**Example:**
```cpp
RandomIdGenerator<uint64_t> rand_gen;
auto id1 = rand_gen.generate();  // Random value, e.g., 9823764123
auto id2 = rand_gen.generate();  // Different random value
```

#### Custom Allocation Policy

Implement your own allocation strategy:

```cpp
template <typename IdType = uint64_t>
class CustomAllocationPolicy {
public:
    explicit CustomAllocationPolicy(IdType base_id = 0);
    
    // Must provide this interface
    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept {
        // Your custom logic here
        return /* computed ID */;
    }
    
    void reset(IdType base_id = 0) noexcept {
        // Reset to initial state
    }
};
```

### 2. Recycling Policy

Controls how released IDs are stored and reused.

#### ImmediateRecyclingPolicy

Released IDs are immediately available for reuse in FIFO order.

```cpp
template <typename IdType = uint64_t>
class ImmediateRecyclingPolicy {
public:
    std::optional<IdType> get_recycled() noexcept;
    void add_recycled(IdType id) noexcept;
    size_t recycled_count() const noexcept;
    void clear() noexcept;
};
```

**Example:**
```cpp
SimpleIdGenerator<uint64_t> gen(1);
auto id1 = gen.generate();  // 1
auto id2 = gen.generate();  // 2
auto id3 = gen.generate();  // 3

gen.release(*id1);  // Recycle 1
gen.release(*id3);  // Recycle 3

auto id4 = gen.generate();  // 1 (first recycled)
auto id5 = gen.generate();  // 3 (second recycled)
auto id6 = gen.generate();  // 4 (fresh)
```

#### NoRecyclingPolicy

Released IDs are never reused. Always generates fresh IDs.

```cpp
template <typename IdType = uint64_t>
class NoRecyclingPolicy {
public:
    std::optional<IdType> get_recycled() noexcept { return std::nullopt; }
    void add_recycled(IdType) noexcept {}
    size_t recycled_count() const noexcept { return 0; }
    void clear() noexcept {}
};
```

**Example:**
```cpp
using NoRecycleGen = IdGenerator<uint64_t,
    SequentialAllocationPolicy<uint64_t>,
    NoRecyclingPolicy<uint64_t>>;

NoRecycleGen gen(1);
auto id1 = gen.generate();  // 1
auto id2 = gen.generate();  // 2
gen.release(*id1);          // Release but don't recycle
auto id3 = gen.generate();  // 3 (not 1)
```

#### Custom Recycling Policy

```cpp
template <typename IdType = uint64_t>
class PriorityRecyclingPolicy {
    std::priority_queue<IdType, std::vector<IdType>, std::greater<IdType>> queue_;
    
public:
    std::optional<IdType> get_recycled() noexcept {
        if (queue_.empty()) return std::nullopt;
        IdType id = queue_.top();
        queue_.pop();
        return id;
    }
    
    void add_recycled(IdType id) noexcept {
        queue_.push(id);
    }
    
    // ... other required methods
};
```

### 3. Error Policy

Controls how errors are reported.

#### ExpectedErrorPolicy

Returns `Expected<T, E>` for error handling without exceptions.

```cpp
template <typename IdType, typename ErrorType = IdError>
class ExpectedErrorPolicy {
public:
    using result_type = Expected<IdType, ErrorType>;
    using void_result_type = Expected<void, ErrorType>;
    
    static result_type report_success(IdType id) noexcept;
    static result_type report_error(ErrorType error) noexcept;
};
```

**Error Types:**

```cpp
enum class IdError {
    Overflow,         // ID space exhausted
    InvalidRelease,   // Attempted to release non-active ID
    Corruption,       // Internal state corruption detected
    AlreadyInUse,     // Generated ID already in use (random collision)
    NotInitialized    // Generator not properly initialized
};
```

**Example:**
```cpp
SimpleIdGenerator<uint8_t> small_gen(250);

// Generate until overflow
for (int i = 0; i < 10; ++i) {
    auto id = small_gen.generate();
    if (!id) {
        if (id.error() == IdError::Overflow) {
            std::cerr << "ID space exhausted\n";
        }
        break;
    }
    // Use id...
}

// Invalid release
auto result = small_gen.release(200);  // Not active
if (!result) {
    // result.error() == IdError::InvalidRelease
}
```

### 4. Concurrency Policy

Controls thread safety and synchronization.

From `ConcurrencyPolicies.h`:

#### SingleThreadedPolicy

No synchronization overhead. Not thread-safe.

```cpp
struct SingleThreadedPolicy {
    struct LockGuard {
        explicit LockGuard(std::mutex&) noexcept {}
    };
};
```

#### MutexSynchronizationPolicy

Thread-safe using `std::mutex`.

```cpp
struct MutexSynchronizationPolicy {
    struct LockGuard {
        explicit LockGuard(std::mutex& mtx) : lock_(mtx) {}
    private:
        std::lock_guard<std::mutex> lock_;
    };
};
```

**Example:**
```cpp
// Single-threaded (fast, no overhead)
SimpleIdGenerator<uint64_t> gen(1);

// Thread-safe (slightly slower)
ThreadSafeIdGenerator<uint64_t> safe_gen(1);

// Can be used from multiple threads
std::thread t1([&]() { safe_gen.generate(); });
std::thread t2([&]() { safe_gen.generate(); });
```

---

## API Reference

### IdGenerator Class

```cpp
template <
    typename IdType_,
    typename AllocationPolicy = SequentialAllocationPolicy<underlying_id_type_t<IdType_>>,
    typename RecyclingPolicy = ImmediateRecyclingPolicy<underlying_id_type_t<IdType_>>,
    typename ErrorPolicy = ExpectedErrorPolicy<IdType_, IdError>,
    typename ConcurrencyPolicy = SingleThreadedPolicy
>
class IdGenerator;
```

### Type Aliases

```cpp
using id_type = IdType_;
using result_type = typename ErrorPolicy::result_type;
using underlying_type = underlying_id_type_t<IdType_>;
```

### Construction

#### Constructor

```cpp
explicit IdGenerator(underlying_type base_id = 0)
```

**Parameters:**
- `base_id`: Starting ID value (default: 0)

**Example:**
```cpp
SimpleIdGenerator<uint64_t> gen(100);  // Start from 100
SimpleIdGenerator<uint64_t> gen2;      // Start from 0
```

**Notes:**
- Non-copyable (deleted copy constructor/assignment)
- Movable (defaulted move constructor/assignment)

### ID Generation and Release

#### generate()

```cpp
result_type generate()
```

**Returns:** `Expected<IdType, IdError>` containing:
- **Success**: Generated ID
- **Error**: `IdError::Overflow` if ID space exhausted
- **Error**: `IdError::AlreadyInUse` if random collision detected

**Behavior:**
1. Checks recycled IDs first
2. If none available, generates new ID via allocation policy
3. Validates ID is not in use
4. Adds to active set

**Example:**
```cpp
auto id = gen.generate();
if (id) {
    std::cout << "Generated: " << *id << "\n";
} else {
    std::cerr << "Error: " << static_cast<int>(id.error()) << "\n";
}
```

#### release()

```cpp
Expected<void, IdError> release(IdType_ id) noexcept
```

**Parameters:**
- `id`: ID to release

**Returns:** `Expected<void, IdError>` indicating:
- **Success**: ID successfully released
- **Error**: `IdError::InvalidRelease` if ID not active

**Behavior:**
1. Validates ID is in active set
2. Removes from active set
3. Adds to recycling policy

**Example:**
```cpp
auto id = gen.generate();
if (id) {
    // Use the ID...
    
    auto result = gen.release(*id);
    if (!result) {
        std::cerr << "Failed to release ID\n";
    }
}
```

### Query Operations

#### is_active()

```cpp
bool is_active(IdType_ id) const noexcept
```

**Parameters:**
- `id`: ID to check

**Returns:** `true` if ID is currently active, `false` otherwise

**Example:**
```cpp
auto id = gen.generate();
assert(gen.is_active(*id));
gen.release(*id);
assert(!gen.is_active(*id));
```

#### active_count()

```cpp
size_t active_count() const noexcept
```

**Returns:** Number of currently active IDs

**Example:**
```cpp
std::cout << "Active IDs: " << gen.active_count() << "\n";
```

#### recycled_count()

```cpp
size_t recycled_count() const noexcept
```

**Returns:** Number of IDs available for recycling

**Example:**
```cpp
gen.release(id1);
gen.release(id2);
std::cout << "Recycled IDs: " << gen.recycled_count() << "\n";  // 2
```

#### reset()

```cpp
void reset() noexcept
```

**Behavior:**
- Clears all active IDs
- Clears all recycled IDs
- Resets allocation policy to `base_id`

**Example:**
```cpp
gen.generate();
gen.generate();
assert(gen.active_count() == 2);

gen.reset();
assert(gen.active_count() == 0);
assert(gen.recycled_count() == 0);
```

### RAII Helper: IdGuard

```cpp
class IdGuard {
public:
    explicit IdGuard(IdGenerator& gen, IdType_ id);
    ~IdGuard();
    
    IdGuard(const IdGuard&) = delete;
    IdGuard& operator=(const IdGuard&) = delete;
    IdGuard(IdGuard&& other) noexcept;
    IdGuard& operator=(IdGuard&& other) noexcept;
    
    IdType_ get() const noexcept;
    IdType_ operator*() const noexcept;
    void release_ownership() noexcept;
};
```

#### scoped_id()

```cpp
Expected<IdGuard, IdError> scoped_id()
```

**Returns:** `Expected<IdGuard, IdError>` containing:
- **Success**: RAII guard that automatically releases ID
- **Error**: Same as `generate()`

**Example:**
```cpp
{
    auto guard = gen.scoped_id();
    if (guard) {
        uint64_t id = guard->get();
        // Use id...
    }
    // ID automatically released here
}
```

#### IdGuard Methods

**get() / operator*()**
```cpp
IdType_ get() const noexcept;
IdType_ operator*() const noexcept;
```

Returns the held ID.

**release_ownership()**
```cpp
void release_ownership() noexcept;
```

Prevents automatic release on destruction. Use with caution.

**Example:**
```cpp
auto guard = gen.scoped_id();
if (guard) {
    uint64_t id = **guard;  // Using operator*
    // or
    uint64_t id2 = guard->get();
    
    // Prevent automatic release
    guard->release_ownership();
}
```

### Convenience Aliases

```cpp
// Simple sequential generator (single-threaded)
template <typename IdType = uint64_t>
using SimpleIdGenerator = IdGenerator<IdType,
    SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
    ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
    ExpectedErrorPolicy<IdType, IdError>,
    SingleThreadedPolicy>;

// Thread-safe sequential generator
template <typename IdType = uint64_t>
using ThreadSafeIdGenerator = IdGenerator<IdType,
    SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
    ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
    ExpectedErrorPolicy<IdType, IdError>,
    MutexSynchronizationPolicy>;

// Random ID generator (no recycling)
template <typename IdType = uint64_t>
using RandomIdGenerator = IdGenerator<IdType,
    RandomAllocationPolicy<underlying_id_type_t<IdType>>,
    NoRecyclingPolicy<underlying_id_type_t<IdType>>,
    ExpectedErrorPolicy<IdType, IdError>,
    SingleThreadedPolicy>;
```

---

## Usage Examples

### Example 1: Resource Management

```cpp
#include "IdGenerator.h"
#include <unordered_map>
#include <string>

class ResourceManager {
    SimpleIdGenerator<uint64_t> id_gen_{1};
    std::unordered_map<uint64_t, std::string> resources_;
    
public:
    Expected<uint64_t, IdError> create_resource(const std::string& data) {
        auto id = id_gen_.generate();
        if (!id) return id;
        
        resources_[*id] = data;
        return id;
    }
    
    Expected<void, IdError> destroy_resource(uint64_t id) {
        if (resources_.erase(id) == 0) {
            return make_unexpected(IdError::InvalidRelease);
        }
        return id_gen_.release(id);
    }
    
    std::optional<std::string> get_resource(uint64_t id) const {
        auto it = resources_.find(id);
        if (it != resources_.end()) {
            return it->second;
        }
        return std::nullopt;
    }
};

// Usage
ResourceManager rm;
auto res_id = rm.create_resource("Important Data");
if (res_id) {
    auto data = rm.get_resource(*res_id);
    rm.destroy_resource(*res_id);
}
```

### Example 2: Session Management with Type Safety

```cpp
#include "IdGenerator.h"
#include "StrongId.h"
#include <unordered_map>
#include <chrono>

using SessionId = StrongId<uint64_t, struct SessionTag>;
using UserId = StrongId<uint64_t, struct UserTag>;

struct Session {
    UserId user_id;
    std::chrono::system_clock::time_point created;
    std::string ip_address;
};

class SessionManager {
    IdGenerator<SessionId> session_gen_{10000};
    IdGenerator<UserId> user_gen_{1};
    std::unordered_map<SessionId, Session> sessions_;
    
public:
    Expected<SessionId, IdError> create_session(UserId user_id, 
                                                  const std::string& ip) {
        auto session_id = session_gen_.generate();
        if (!session_id) return session_id;
        
        sessions_[*session_id] = Session{
            user_id,
            std::chrono::system_clock::now(),
            ip
        };
        
        return session_id;
    }
    
    Expected<void, IdError> destroy_session(SessionId session_id) {
        sessions_.erase(session_id);
        return session_gen_.release(session_id);
    }
    
    bool validate_session(SessionId session_id) const {
        return sessions_.count(session_id) > 0;
    }
};

// Usage
SessionManager sm;
auto user_id = UserId(42);
auto session = sm.create_session(user_id, "192.168.1.1");
if (session) {
    if (sm.validate_session(*session)) {
        // Session is valid
    }
    sm.destroy_session(*session);
}
```

### Example 3: Connection Pool

```cpp
#include "IdGenerator.h"
#include <vector>
#include <memory>

class Connection {
public:
    void execute(const std::string& query) { /* ... */ }
    bool is_healthy() const { return true; }
};

class ConnectionPool {
    ThreadSafeIdGenerator<uint64_t> id_gen_{1};
    std::unordered_map<uint64_t, std::unique_ptr<Connection>> connections_;
    std::mutex pool_mutex_;
    
public:
    Expected<uint64_t, IdError> acquire_connection() {
        auto id = id_gen_.generate();
        if (!id) return id;
        
        std::lock_guard<std::mutex> lock(pool_mutex_);
        connections_[*id] = std::make_unique<Connection>();
        return id;
    }
    
    Expected<void, IdError> release_connection(uint64_t id) {
        {
            std::lock_guard<std::mutex> lock(pool_mutex_);
            if (connections_.erase(id) == 0) {
                return make_unexpected(IdError::InvalidRelease);
            }
        }
        return id_gen_.release(id);
    }
    
    Connection* get_connection(uint64_t id) {
        std::lock_guard<std::mutex> lock(pool_mutex_);
        auto it = connections_.find(id);
        return (it != connections_.end()) ? it->second.get() : nullptr;
    }
};

// Usage with RAII
void execute_query(ConnectionPool& pool, const std::string& query) {
    auto conn_id = pool.acquire_connection();
    if (!conn_id) {
        std::cerr << "Failed to acquire connection\n";
        return;
    }
    
    // Use RAII to ensure release
    struct ConnectionGuard {
        ConnectionPool& pool;
        uint64_t id;
        ~ConnectionGuard() { pool.release_connection(id); }
    } guard{pool, *conn_id};
    
    if (auto* conn = pool.get_connection(*conn_id)) {
        conn->execute(query);
    }
}
```

### Example 4: Multi-threaded ID Generation

```cpp
#include "IdGenerator.h"
#include <thread>
#include <vector>
#include <set>
#include <iostream>

void parallel_id_generation() {
    ThreadSafeIdGenerator<uint64_t> gen(1);
    
    const size_t num_threads = 4;
    const size_t ids_per_thread = 1000;
    
    std::vector<std::set<uint64_t>> thread_ids(num_threads);
    std::vector<std::thread> threads;
    
    // Launch threads
    for (size_t i = 0; i < num_threads; ++i) {
        threads.emplace_back([&gen, &thread_ids, i, ids_per_thread]() {
            for (size_t j = 0; j < ids_per_thread; ++j) {
                auto id = gen.generate();
                if (id) {
                    thread_ids[i].insert(*id);
                }
            }
        });
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify no collisions
    std::set<uint64_t> all_ids;
    for (const auto& thread_set : thread_ids) {
        for (uint64_t id : thread_set) {
            assert(all_ids.find(id) == all_ids.end());
            all_ids.insert(id);
        }
    }
    
    std::cout << "Generated " << all_ids.size() 
              << " unique IDs across " << num_threads << " threads\n";
}
```

### Example 5: Custom Allocation Policy - Even Numbers Only

```cpp
#include "IdGenerator.h"

template <typename IdType = uint64_t>
class EvenAllocationPolicy {
    IdType base_id_;
    IdType next_id_;
    
public:
    explicit EvenAllocationPolicy(IdType base_id = 0)
        : base_id_(base_id % 2 == 0 ? base_id : base_id + 1)
        , next_id_(base_id_) {}
    
    std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept {
        IdType candidate;
        
        if (first_call) {
            candidate = next_id_;
        } else {
            // Find next even number after max_id
            candidate = (max_id % 2 == 0) ? max_id + 2 : max_id + 1;
            if (candidate < next_id_) {
                candidate = next_id_;
            }
        }
        
        // Check for overflow
        if (candidate >= std::numeric_limits<IdType>::max() - 1) {
            return std::nullopt;
        }
        
        next_id_ = candidate + 2;
        return candidate;
    }
    
    void reset(IdType base_id = 0) noexcept {
        base_id_ = base_id % 2 == 0 ? base_id : base_id + 1;
        next_id_ = base_id_;
    }
};

// Use custom policy
using EvenIdGenerator = IdGenerator<uint64_t,
    EvenAllocationPolicy<uint64_t>,
    ImmediateRecyclingPolicy<uint64_t>,
    ExpectedErrorPolicy<uint64_t, IdError>,
    SingleThreadedPolicy>;

// Usage
EvenIdGenerator even_gen(1);  // Starts at 2 (next even)
auto id1 = even_gen.generate();  // 2
auto id2 = even_gen.generate();  // 4
auto id3 = even_gen.generate();  // 6
```

### Example 6: Delayed Recycling Policy

```cpp
#include "IdGenerator.h"
#include <deque>
#include <chrono>

template <typename IdType = uint64_t>
class DelayedRecyclingPolicy {
    struct RecycledId {
        IdType id;
        std::chrono::steady_clock::time_point released_at;
    };
    
    std::deque<RecycledId> recycled_;
    std::chrono::milliseconds delay_{1000};  // 1 second delay
    
public:
    std::optional<IdType> get_recycled() noexcept {
        if (recycled_.empty()) return std::nullopt;
        
        auto now = std::chrono::steady_clock::now();
        auto& front = recycled_.front();
        
        // Only return if delay has passed
        if (now - front.released_at >= delay_) {
            IdType id = front.id;
            recycled_.pop_front();
            return id;
        }
        
        return std::nullopt;
    }
    
    void add_recycled(IdType id) noexcept {
        recycled_.push_back({id, std::chrono::steady_clock::now()});
    }
    
    size_t recycled_count() const noexcept {
        return recycled_.size();
    }
    
    void clear() noexcept {
        recycled_.clear();
    }
    
    void set_delay(std::chrono::milliseconds delay) {
        delay_ = delay;
    }
};

// Usage
using DelayedIdGen = IdGenerator<uint64_t,
    SequentialAllocationPolicy<uint64_t>,
    DelayedRecyclingPolicy<uint64_t>>;

DelayedIdGen gen(1);
auto id = gen.generate();
gen.release(*id);

// Immediately trying to generate won't reuse the ID
auto id2 = gen.generate();  // Fresh ID

// After delay, will reuse
std::this_thread::sleep_for(std::chrono::seconds(1));
auto id3 = gen.generate();  // Reuses first ID
```

---

## Thread Safety

### Thread Safety Guarantees

The thread safety of `IdGenerator` depends entirely on the chosen `ConcurrencyPolicy`:

| Policy | Thread Safety | Performance | Use Case |
|--------|--------------|-------------|----------|
| `SingleThreadedPolicy` | ❌ Not thread-safe | Fastest | Single-threaded applications |
| `MutexSynchronizationPolicy` | ✅ Thread-safe | Slightly slower | Multi-threaded applications |

### Thread-Safe Operations

When using `MutexSynchronizationPolicy` (via `ThreadSafeIdGenerator`), all operations are thread-safe:

```cpp
ThreadSafeIdGenerator<uint64_t> gen(1);

// All safe to call concurrently
std::thread t1([&]() { gen.generate(); });
std::thread t2([&]() { gen.generate(); });
std::thread t3([&]() { gen.release(some_id); });
std::thread t4([&]() { bool active = gen.is_active(some_id); });
```

### Internal Locking Strategy

All methods acquire the lock for their entire duration:

```cpp
result_type generate() {
    typename ConcurrencyPolicy::LockGuard lock(mutex_);  // Acquire lock
    // ... entire operation ...
}  // Release lock
```

### Lock Scope

The `mutable` mutex allows locking in const methods:

```cpp
bool is_active(IdType_ id) const noexcept {
    typename ConcurrencyPolicy::LockGuard lock(mutex_);  // OK: mutable mutex
    return ids_in_use_.count(raw_id) > 0;
}
```

### Performance Considerations

**Single-threaded vs Thread-safe:**

Benchmark on Intel Core i7-8850H @ 2.60GHz:
- `SimpleIdGenerator`: ~20 ns/operation
- `ThreadSafeIdGenerator`: ~35 ns/operation (75% overhead)

**Recommendation:** Use `SimpleIdGenerator` when possible for maximum performance.

---

## Error Handling

### Error Types

```cpp
enum class IdError {
    Overflow,         // ID space exhausted
    InvalidRelease,   // Attempted to release non-active ID
    Corruption,       // Internal state corruption detected
    AlreadyInUse,     // Generated ID already in use (random collision)
    NotInitialized    // Generator not properly initialized
};
```

### Expected-Based Error Handling

All fallible operations return `Expected<T, IdError>`:

```cpp
auto id = gen.generate();
if (id) {
    // Success: use *id
} else {
    // Error: handle id.error()
    switch (id.error()) {
        case IdError::Overflow:
            std::cerr << "ID space exhausted\n";
            break;
        case IdError::AlreadyInUse:
            std::cerr << "Random collision detected\n";
            break;
        // ...
    }
}
```

### Common Error Scenarios

#### 1. Overflow

Occurs when ID type cannot accommodate more IDs:

```cpp
SimpleIdGenerator<uint8_t> gen(250);

for (int i = 0; i < 10; ++i) {
    auto id = gen.generate();
    if (!id) {
        // After generating 250, 251, 252, 253, 254, 255
        assert(id.error() == IdError::Overflow);
        break;
    }
}
```

**Prevention:**
- Use larger ID types (`uint64_t` instead of `uint8_t`)
- Implement ID recycling
- Monitor `active_count()`

#### 2. InvalidRelease

Occurs when releasing an ID that's not active:

```cpp
SimpleIdGenerator<uint64_t> gen(1);
auto result = gen.release(999);  // Never generated
assert(!result);
assert(result.error() == IdError::InvalidRelease);

// Or double-release
auto id = gen.generate();
gen.release(*id);
auto result2 = gen.release(*id);  // Already released
assert(result2.error() == IdError::InvalidRelease);
```

**Prevention:**
- Track ID ownership
- Use `is_active()` before release
- Use `IdGuard` for RAII

#### 3. AlreadyInUse

Occurs with `RandomAllocationPolicy` on collision:

```cpp
RandomIdGenerator<uint8_t> rand_gen;  // Small space

// With only 256 possible values, collisions are likely
for (int i = 0; i < 300; ++i) {
    auto id = rand_gen.generate();
    if (!id && id.error() == IdError::AlreadyInUse) {
        // Collision detected, retry or handle
        break;
    }
}
```

**Prevention:**
- Use larger ID types
- Implement retry logic
- Monitor collision rates

### Error Handling Patterns

#### Pattern 1: Early Return

```cpp
Expected<void, std::string> process_request() {
    auto id = gen.generate();
    if (!id) {
        return make_unexpected("Failed to allocate ID");
    }
    
    // Process with *id...
    
    return {};
}
```

#### Pattern 2: Retry Logic

```cpp
std::optional<uint64_t> generate_with_retry(int max_attempts = 3) {
    for (int i = 0; i < max_attempts; ++i) {
        auto id = rand_gen.generate();
        if (id) return *id;
        
        if (id.error() == IdError::AlreadyInUse) {
            continue;  // Retry on collision
        } else {
            return std::nullopt;  // Other errors are fatal
        }
    }
    return std::nullopt;
}
```

#### Pattern 3: RAII (Recommended)

```cpp
void safe_operation() {
    auto guard = gen.scoped_id();
    if (!guard) {
        // Handle error
        return;
    }
    
    // Use **guard
    // Automatic release on all exit paths
}
```

---

## Performance Considerations

### Benchmarks

Tested on Intel Core i7-8850H @ 2.60GHz:

| Operation | Time | Notes |
|-----------|------|-------|
| Sequential generate (fresh) | ~20 ns | No recycling |
| Sequential generate (recycled) | ~25 ns | From recycled pool |
| Random generate | ~80 ns | Includes RNG overhead |
| Release | ~15 ns | Add to recycled pool |
| is_active() | ~10 ns | Set lookup |
| Thread-safe overhead | +75% | Mutex locking cost |

### Memory Usage

- Base overhead: ~48 bytes (vtable, mutex, base_id)
- Per active ID: ~32 bytes (std::set node)
- Per recycled ID: ~16 bytes (std::deque element)

**Example:**
- 1000 active IDs: ~32 KB
- 1000 recycled IDs: ~16 KB
- Total: ~48 KB

### Optimization Tips

#### 1. Choose Appropriate ID Type

```cpp
// Overkill for small applications
SimpleIdGenerator<uint64_t> gen;  // Supports 18 quintillion IDs

// Better for limited scope
SimpleIdGenerator<uint16_t> gen;  // Supports 65,535 IDs
```

#### 2. Batch Generation

```cpp
// Inefficient: generate one at a time
for (int i = 0; i < 1000; ++i) {
    auto id = gen.generate();
    // Use id
    gen.release(*id);
}

// Better: generate batch, process, release batch
std::vector<uint64_t> ids;
for (int i = 0; i < 1000; ++i) {
    if (auto id = gen.generate()) {
        ids.push_back(*id);
    }
}

// Process all IDs...

for (auto id : ids) {
    gen.release(id);
}
```

#### 3. Avoid Unnecessary Thread Safety

```cpp
// If guaranteed single-threaded access
SimpleIdGenerator<uint64_t> gen;  // Faster

// Only if actually needed
ThreadSafeIdGenerator<uint64_t> gen;  // Slower
```

#### 4. Reserve Capacity (if implementing custom policies)

```cpp
template <typename IdType = uint64_t>
class PreallocatedRecyclingPolicy {
    std::vector<IdType> recycled_;
    
public:
    PreallocatedRecyclingPolicy() {
        recycled_.reserve(1000);  // Avoid reallocations
    }
    // ...
};
```

#### 5. Profile Before Optimizing

```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 100000; ++i) {
    auto id = gen.generate();
    gen.release(*id);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
std::cout << "Average: " << duration.count() / 100000.0 << " ns/op\n";
```

---

## Best Practices

### ✅ DO

**1. Use Type-Safe IDs with StrongId**
```cpp
using UserId = StrongId<uint64_t, struct UserTag>;
using SessionId = StrongId<uint64_t, struct SessionTag>;

IdGenerator<UserId> user_gen;
IdGenerator<SessionId> session_gen;

// Compile-time safety prevents mixing
```

**2. Use RAII with IdGuard**
```cpp
auto guard = gen.scoped_id();
if (guard) {
    // Use **guard
    // Automatic cleanup
}
```

**3. Check Error Returns**
```cpp
auto id = gen.generate();
if (!id) {
    // Handle error appropriately
    return;
}
// Use *id
```

**4. Monitor Active Count**
```cpp
if (gen.active_count() > threshold) {
    std::cerr << "Warning: High ID usage\n";
}
```

**5. Choose Appropriate Base ID**
```cpp
// Reserve 0 for special meaning
SimpleIdGenerator<uint64_t> gen(1);

// Or use meaningful ranges
SimpleIdGenerator<uint64_t> user_gen(10000);
SimpleIdGenerator<uint64_t> session_gen(50000);
```

**6. Use Recycling Wisely**
```cpp
// For limited ID space
ImmediateRecyclingPolicy  // ✅ Reuse IDs

// For audit trails or logging
NoRecyclingPolicy  // ✅ Never reuse
```

### ❌ DON'T

**1. Don't Mix Raw IDs with Type-Safe IDs**
```cpp
// ❌ Bad
IdGenerator<UserId> gen1;
IdGenerator<uint64_t> gen2;
uint64_t raw_id = gen1.generate()->get();  // Lost type safety
```

**2. Don't Forget to Release IDs**
```cpp
// ❌ Memory leak equivalent
while (true) {
    auto id = gen.generate();
    // Never released - will eventually overflow
}

// ✅ Good
while (true) {
    auto guard = gen.scoped_id();
    // Automatic release
}
```

**3. Don't Use Thread-Unsafe Generator Across Threads**
```cpp
// ❌ Undefined behavior
SimpleIdGenerator<uint64_t> gen;
std::thread t1([&]() { gen.generate(); });
std::thread t2([&]() { gen.generate(); });  // Race condition!

// ✅ Good
ThreadSafeIdGenerator<uint64_t> gen;
```

**4. Don't Ignore Overflow**
```cpp
// ❌ Will crash eventually
auto id = gen.generate();
use(*id);  // Undefined behavior if id has error

// ✅ Good
auto id = gen.generate();
if (id) {
    use(*id);
} else {
    handle_error(id.error());
}
```

**5. Don't Release IDs Multiple Times**
```cpp
// ❌ Logic error
auto id = gen.generate();
gen.release(*id);
gen.release(*id);  // Returns InvalidRelease error

// ✅ Use IdGuard to prevent this
```

**6. Don't Use Random Generator Without Collision Handling**
```cpp
// ❌ Will fail with small ID space
RandomIdGenerator<uint8_t> rand_gen;
for (int i = 0; i < 300; ++i) {
    auto id = rand_gen.generate();  // Collisions likely
}

// ✅ Use larger type or handle collisions
RandomIdGenerator<uint64_t> rand_gen;  // Collisions extremely rare
```

---

## Integration with Other Components

### Integration with StrongId

Perfect combination for type-safe ID management:

```cpp
#include "IdGenerator.h"
#include "StrongId.h"

using UserId = StrongId<uint64_t, struct UserTag>;
using OrderId = StrongId<uint64_t, struct OrderTag>;

class BusinessLogic {
    IdGenerator<UserId> user_gen_{1};
    IdGenerator<OrderId> order_gen_{1000};
    
public:
    Expected<UserId, IdError> create_user() {
        return user_gen_.generate();
    }
    
    Expected<OrderId, IdError> create_order(UserId user_id) {
        // Type system prevents mixing IDs
        return order_gen_.generate();
    }
};
```

### Integration with Expected

Seamless error propagation:

```cpp
#include "IdGenerator.h"
#include "Expected.h"

Expected<std::string, std::string> process_request() {
    SimpleIdGenerator<uint64_t> gen(1);
    
    auto id = gen.generate();
    if (!id) {
        return make_unexpected("Failed to generate ID");
    }
    
    // Chain operations
    return process_with_id(*id)
        .and_then([&](auto result) { return gen.release(*id); })
        .map([]() { return std::string("Success"); });
}
```

### Integration with ConcurrencyPolicies

Flexible synchronization options:

```cpp
#include "IdGenerator.h"
#include "ConcurrencyPolicies.h"

// Single-threaded
using FastGen = IdGenerator<uint64_t,
    SequentialAllocationPolicy<uint64_t>,
    ImmediateRecyclingPolicy<uint64_t>,
    ExpectedErrorPolicy<uint64_t, IdError>,
    SingleThreadedPolicy>;

// Thread-safe
using SafeGen = IdGenerator<uint64_t,
    SequentialAllocationPolicy<uint64_t>,
    ImmediateRecyclingPolicy<uint64_t>,
    ExpectedErrorPolicy<uint64_t, IdError>,
    MutexSynchronizationPolicy>;
```

### Integration with Smart Pointers

```cpp
#include "IdGenerator.h"
#include <memory>

template <typename T>
class IdManagedResource {
    SimpleIdGenerator<uint64_t> id_gen_{1};
    std::unordered_map<uint64_t, std::shared_ptr<T>> resources_;
    
public:
    std::shared_ptr<T> create() {
        auto id = id_gen_.generate();
        if (!id) return nullptr;
        
        auto resource = std::make_shared<T>();
        auto [it, inserted] = resources_.emplace(*id, resource);
        
        // Use custom deleter to release ID
        return std::shared_ptr<T>(resource.get(), 
            [this, id = *id](T*) {
                resources_.erase(id);
                id_gen_.release(id);
            });
    }
};
```

---

## Appendix

### Complete Example: HTTP Session Manager

```cpp
#include "IdGenerator.h"
#include "StrongId.h"
#include "Expected.h"
#include <string>
#include <unordered_map>
#include <chrono>
#include <memory>

using SessionId = StrongId<uint64_t, struct SessionTag>;

struct SessionData {
    std::string user_name;
    std::string ip_address;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
};

class HttpSessionManager {
    ThreadSafeIdGenerator<SessionId> id_gen_{10000};
    std::unordered_map<SessionId, SessionData> sessions_;
    mutable std::mutex sessions_mutex_;
    std::chrono::seconds session_timeout_{1800};  // 30 minutes
    
public:
    Expected<SessionId, IdError> create_session(
        const std::string& user_name,
        const std::string& ip_address) {
        
        auto session_id = id_gen_.generate();
        if (!session_id) {
            return session_id;
        }
        
        auto now = std::chrono::system_clock::now();
        SessionData data{user_name, ip_address, now, now};
        
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            sessions_[*session_id] = std::move(data);
        }
        
        return session_id;
    }
    
    Expected<void, IdError> destroy_session(SessionId session_id) {
        {
            std::lock_guard<std::mutex> lock(sessions_mutex_);
            if (sessions_.erase(session_id) == 0) {
                return make_unexpected(IdError::InvalidRelease);
            }
        }
        
        return id_gen_.release(session_id);
    }
    
    bool validate_session(SessionId session_id) {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return false;
        }
        
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.last_accessed);
        
        if (elapsed > session_timeout_) {
            return false;
        }
        
        it->second.last_accessed = now;
        return true;
    }
    
    void cleanup_expired_sessions() {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto it = sessions_.begin();
        
        while (it != sessions_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.last_accessed);
            
            if (elapsed > session_timeout_) {
                auto session_id = it->first;
                it = sessions_.erase(it);
                id_gen_.release(session_id);
            } else {
                ++it;
            }
        }
    }
    
    size_t active_session_count() const {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        return sessions_.size();
    }
};

// Usage
int main() {
    HttpSessionManager session_mgr;
    
    // Create sessions
    auto session1 = session_mgr.create_session("alice", "192.168.1.100");
    auto session2 = session_mgr.create_session("bob", "192.168.1.101");
    
    if (session1 && session2) {
        std::cout << "Created sessions: " 
                  << session1->get() << " and " 
                  << session2->get() << "\n";
        
        // Validate session
        if (session_mgr.validate_session(*session1)) {
            std::cout << "Session " << session1->get() << " is valid\n";
        }
        
        // Cleanup
        session_mgr.cleanup_expired_sessions();
        
        // Destroy sessions
        session_mgr.destroy_session(*session1);
        session_mgr.destroy_session(*session2);
    }
    
    std::cout << "Active sessions: " 
              << session_mgr.active_session_count() << "\n";
    
    return 0;
}
```

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 2.0 | 2025 | - Improved sequential allocation logic<br>- Fixed `max_id` handling on first call<br>- RandomAllocationPolicy now includes 0<br>- Enhanced documentation |
| 1.0 | 2024 | Initial release |

---

## License

Part of the C++ Utilities Library (fat_p) - Header-only, zero dependencies.

---

## Support

For issues, questions, or contributions, please refer to the project repository.

---

**End of IdGenerator User Manual**
