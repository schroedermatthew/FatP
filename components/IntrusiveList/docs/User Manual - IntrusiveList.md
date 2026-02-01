---
doc_id: UM-INTRUSIVELIST-001
doc_type: "User Manual"
title: "IntrusiveList"
fatp_components: ["IntrusiveList", "IntrusiveListNode", "IntrusiveListIterator", "IntrusiveListFast", "IntrusiveListSafe"]
topics: ["intrusive containers", "zero allocation", "linked list", "free list", "CRTP inheritance", "object pools", "embedded systems", "real-time systems", "ownership policy"]
constraints: ["heap allocation in hot loops", "memory fragmentation", "allocation latency", "single list membership", "node ownership", "iterator invalidation"]
cxx_standard: "C++17"
std_equivalent: null
boost_equivalent: "Boost.Intrusive list"
build_modes: ["Debug", "Release"]
last_verified: "2026-01-21"
audience: ["C++ developers", "embedded systems developers", "game developers", "performance engineers", "AI assistants"]
status: "reviewed"
---

# User Manual - IntrusiveList

*Updated January 2026*

---

## Scope

This manual covers practical usage of IntrusiveList: node setup, list operations, policy selection, iteration patterns, and migration from std::list. It provides recipes for common use cases and troubleshooting guidance.

## Not covered

- Internal implementation details of the sentinel-based design
- Comparison with other intrusive container libraries (see Overview - IntrusiveList)
- Design rationale and tradeoff analysis (see Companion Guide when available)
- Multi-list membership patterns (each node can only be in one list)

## Prerequisites

- Familiarity with C++ templates and CRTP pattern
- Understanding of pointer semantics and object lifetime
- Basic knowledge of doubly-linked list concepts

---

## User Manual Card

**Component:** IntrusiveList  
**Primary use case:** Zero-allocation list operations in performance-critical code  
**Integration pattern:** Inherit from IntrusiveListNode<T>, create IntrusiveList<T>, add/remove via references  
**Key API:** push_back(), remove(), isLinked(), iteratorTo(), splice()  
**std equivalent:** None. No standard intrusive container exists.  
**Migration from std:** Replace std::list<T*> with IntrusiveList<T>, change pointer semantics to reference semantics  
**Common mistakes:** Destroying objects while linked, double insertion, wrong-list removal (Fast policy)  
**Performance notes:** All operations O(1) except Safe policy splice O(N); zero heap allocation

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Choosing a Policy: Fast vs Safe](#choosing-a-policy-fast-vs-safe)
3. [The Node Contract](#the-node-contract)
4. [List Operations](#list-operations)
5. [Membership Testing: isLinked()](#membership-testing-islinked)
6. [Converting Nodes to Iterators: iteratorTo()](#converting-nodes-to-iterators-iteratorto)
7. [Iteration: Bidirectional and Reverse](#iteration-bidirectional-and-reverse)
8. [Splice Operations](#splice-operations)
9. [Correctness Contract](#correctness-contract)
10. [Common Patterns](#common-patterns)
11. [Thread Safety](#thread-safety)
12. [Error Handling Model](#error-handling-model)
13. [Performance Rules of Thumb](#performance-rules-of-thumb)
14. [Debug Mode](#debug-mode)
15. [Migration from std::list](#migration-from-stdlist)
16. [Troubleshooting](#troubleshooting)
17. [API Reference](#api-reference)

---

## Getting Started

IntrusiveList is a zero-allocation doubly-linked list where link pointers are embedded directly in your objects. Unlike `std::list`, no heap allocation occurs during list operations—just pointer manipulation.

### Minimal Example

```cpp
#include <fat_p/IntrusiveList.h>
#include <iostream>

// Step 1: Your type inherits from IntrusiveListNode
struct Task : fat_p::IntrusiveListNode<Task> {
    int priority;
    std::string name;
    
    Task(int p, std::string n) : priority(p), name(std::move(n)) {}
};

int main() {
    // Step 2: Create a list
    fat_p::IntrusiveList<Task> ready_queue;
    
    // Step 3: Create objects (you own their lifetime)
    Task render{5, "render"};
    Task physics{10, "physics"};
    Task audio{15, "audio"};
    
    // Step 4: Add to list (no allocation!)
    ready_queue.push_back(render);
    ready_queue.push_back(physics);
    ready_queue.push_back(audio);
    
    // Step 5: Iterate
    for (const Task& t : ready_queue) {
        std::cout << t.name << " (priority " << t.priority << ")\n";
    }
    
    // Step 6: Remove (no deallocation!)
    ready_queue.remove(physics);
    
    // Step 7: Check membership
    std::cout << "physics linked? " << physics.isLinked() << "\n";  // false
    std::cout << "render linked? " << render.isLinked() << "\n";    // true
    
    return 0;
}
```

**Output:**
```
render (priority 5)
physics (priority 10)
audio (priority 15)
physics linked? 0
render linked? 1
```

---

## Choosing a Policy: Fast vs Safe

IntrusiveList provides two ownership policies through template parameters.

### Fast Policy (Default)

```cpp
// These are equivalent:
struct Task : fat_p::IntrusiveListNode<Task> { };
struct Task : fat_p::IntrusiveListNode<Task, fat_p::intrusive_list::FastOwnerPolicy> { };

// List types:
fat_p::IntrusiveList<Task> queue;
fat_p::IntrusiveListFast<Task> queue;  // Explicit alias
```

**Properties:**
- 16 bytes per node (prev + next pointers only)
- O(1) splice, move, and all operations
- Wrong-list removal is **undefined behavior** in Release
- Debug mode asserts if node not in list

**Use when:** Performance is critical, you control which list each node belongs to, or you're doing frequent splice operations.

### Safe Policy

```cpp
struct SafeTask : fat_p::IntrusiveListNode<SafeTask, fat_p::intrusive_list::SafeOwnerPolicy> { };

// List type:
fat_p::IntrusiveListSafe<SafeTask> queue;
```

**Properties:**
- 24 bytes per node (prev + next + owner pointers)
- O(N) splice and move (must update owner pointers)
- Wrong-list removal is a **safe no-op**
- O(1) iterator provenance checking

**Use when:** APIs might accidentally remove from wrong list, or debugging complex list interactions.

### Policy Decision Tree

```cpp
// Default: Fast policy
fat_p::IntrusiveList<MyType> fastList;

// Explicit fast:
fat_p::IntrusiveListFast<MyType> alsoFast;

// Safe policy:
fat_p::IntrusiveListSafe<MySafeType> safeList;
```

---

## The Node Contract

### Inheritance Requirement

Your type must inherit from `IntrusiveListNode` with itself as the template parameter (CRTP):

```cpp
// Correct: T inherits from IntrusiveListNode<T>
struct Task : fat_p::IntrusiveListNode<Task> {
    int data;
};

// Also correct: with explicit policy
struct SafeTask : fat_p::IntrusiveListNode<SafeTask, fat_p::intrusive_list::SafeOwnerPolicy> {
    int data;
};
```

### Lifetime Rules

1. **Objects must outlive list membership.** The list stores pointers into your objects. Destroying an object while it's linked causes undefined behavior.

2. **Objects must be unlinked before destruction.** In debug mode, the destructor asserts if the node is still linked:

```cpp
{
    Task task{42};
    list.push_back(task);
    // BUG: task destroyed while still linked!
}  // Debug: assertion failed
```

**Correct pattern:**

```cpp
{
    Task task{42};
    list.push_back(task);
    list.remove(task);  // Unlink before destruction
}  // OK
```

Or clear the list:

```cpp
{
    Task task{42};
    list.push_back(task);
    list.clear();  // Unlinks all nodes
}  // OK
```

### Single List Membership

Each node can be in **at most one list** at a time:

```cpp
Task task;
listA.push_back(task);
listB.push_back(task);  // BUG: Debug assertion fails!
```

To move between lists:

```cpp
// Remove then add
listA.remove(task);
listB.push_back(task);
```

---

## List Operations

All list operations are O(1) and allocation-free (except splice with Safe policy which is O(N)).

### Adding Elements

```cpp
Task a, b, c;

list.push_front(a);     // Add to front
list.push_back(b);      // Add to back
list.insert(list.begin(), c);  // Insert before position
```

### Removing Elements

```cpp
list.pop_front();       // Remove first element
list.pop_back();        // Remove last element
list.remove(task);      // Remove specific node (O(1) if in this list)
list.erase(it);         // Remove at iterator, returns next iterator
list.clear();           // Remove all elements
```

### Element Access

```cpp
Task& first = list.front();  // First element (assert if empty)
Task& last = list.back();    // Last element (assert if empty)
```

### Size Queries

```cpp
bool empty = list.empty();   // O(1)
size_t n = list.size();      // O(1)
```

---

## Membership Testing: isLinked()

Every node has an `isLinked()` method to check if it's currently in any list:

```cpp
Task task;
std::cout << task.isLinked();  // false

list.push_back(task);
std::cout << task.isLinked();  // true

list.remove(task);
std::cout << task.isLinked();  // false
```

**How it works:** A node is linked if `prev != nullptr`. The sentinel-based design ensures this is always accurate:
- Unlinked nodes have `prev = nullptr`
- Linked nodes always have valid `prev` pointer (possibly to sentinel)

### Common Pattern: Check Before Remove

```cpp
void unlink_if_needed(Task& task, IntrusiveList<Task>& list) {
    if (task.isLinked()) {
        list.remove(task);
    }
}
```

### Common Pattern: Move Between Lists

```cpp
void move_to_list(Task& task, IntrusiveList<Task>& from, IntrusiveList<Task>& to) {
    if (task.isLinked()) {
        from.remove(task);
    }
    to.push_back(task);
}
```

---

## Converting Nodes to Iterators: iteratorTo()

The `iteratorTo()` method converts a node reference to an iterator pointing to that node. This is useful when you have a reference to a node but need an iterator for operations like `erase()` or positional `insert()`.

### Basic Usage

```cpp
Task task{42, "example"};
list.push_back(task);

// Get iterator to the node
auto it = list.iteratorTo(task);

// Now you can use iterator operations
auto next = list.erase(it);  // Remove via iterator
```

### Behavior by Policy

The behavior differs between Fast and Safe policies:

| Scenario | Fast Policy | Safe Policy |
|----------|-------------|-------------|
| Node is linked to this list | Returns valid iterator | Returns valid iterator |
| Node is not linked | Returns `end()` | Returns `end()` |
| Node is linked to different list | **Undefined behavior** | Returns `end()` (safe) |

### Fast Policy Example

```cpp
struct Task : fat_p::IntrusiveListNode<Task> { int id; };
fat_p::IntrusiveList<Task> listA, listB;

Task task{1};
listA.push_back(task);

auto it = listA.iteratorTo(task);  // Valid iterator
// listB.iteratorTo(task);  // UB! Task is in listA, not listB
```

### Safe Policy Example

```cpp
struct SafeTask : fat_p::IntrusiveListNode<SafeTask, fat_p::intrusive_list::SafeOwnerPolicy> { int id; };
fat_p::IntrusiveListSafe<SafeTask> listA, listB;

SafeTask task{1};
listA.push_back(task);

auto itA = listA.iteratorTo(task);  // Valid iterator
auto itB = listB.iteratorTo(task);  // Returns listB.end() (safe)

if (itB == listB.end()) {
    // Task is not in listB
}
```

### Const Correctness

Both mutable and const versions are available:

```cpp
void process(const fat_p::IntrusiveList<Task>& list, const Task& task) {
    auto cit = list.iteratorTo(task);  // Returns const_iterator
    if (cit != list.end()) {
        std::cout << cit->id << "\n";
    }
}
```

---

## Iteration: Bidirectional and Reverse

IntrusiveList provides full bidirectional iterator support.

### Forward Iteration

```cpp
// Range-based for
for (Task& t : list) {
    process(t);
}

// Iterator-based
for (auto it = list.begin(); it != list.end(); ++it) {
    process(*it);
}

// Const iteration
for (const Task& t : list) {
    inspect(t);
}
```

### Reverse Iteration

```cpp
// Reverse range (C++20)
for (Task& t : list | std::views::reverse) {
    process(t);
}

// Explicit reverse iterators
for (auto it = list.rbegin(); it != list.rend(); ++it) {
    process(*it);
}
```

### Decrementing end()

Unlike some intrusive list implementations, `--end()` works correctly:

```cpp
if (!list.empty()) {
    auto it = list.end();
    --it;  // Now points to last element
    Task& last = *it;
}
```

This works because of the sentinel-based design: `end()` points to the sentinel, and `--end()` moves to `sentinel.prev`, which is the last real element.

### Iterator Invalidation

Iterators remain valid unless the pointed-to node is removed:

```cpp
auto it = list.begin();
list.push_back(newTask);  // it still valid
list.remove(otherTask);   // it still valid (unless it pointed to otherTask)
++it;                     // OK
```

### Iterator Invalidation Table

| Operation | Invalidated Iterators |
|-----------|----------------------|
| `push_front(node)` | None |
| `push_back(node)` | None |
| `insert(pos, node)` | None |
| `pop_front()` | Iterator to removed element only |
| `pop_back()` | Iterator to removed element only |
| `remove(node)` | Iterator to removed node only |
| `erase(it)` | `it` only (returns next valid iterator) |
| `clear()` | All iterators except `end()` |
| `splice(pos, other)` | All iterators into `other` invalidated |
| Move construction | Source iterators invalidated; destination iterators valid |
| Move assignment | All destination iterators; source iterators invalidated |

**Key guarantee:** Iterators to nodes that remain in the list are never invalidated by operations on other nodes.

**Critical: Iterator List Binding**

Iterators store a reference to their originating list's sentinel node. This has important implications:

1. **Cross-list comparison is undefined:** Comparing iterators from different lists may produce incorrect results.

2. **Splice invalidates source iterators:** After `splice(pos, other)`, all iterators into `other` are invalidated—including iterators to transferred nodes. Use `iteratorTo(node)` or `begin()`/`end()` on the destination list.

3. **Move invalidates source iterators:** After move construction or assignment, all iterators into the moved-from list are invalidated.

```cpp
// WRONG: iterator still references source list's sentinel
auto it = listA.begin();
listB.splice(listB.end(), listA);
++it;  // UB: sentinel mismatch

// CORRECT: obtain fresh iterator from destination
listB.splice(listB.end(), listA);
for (auto& node : listB) { /* safe */ }
// Or: auto it = listB.iteratorTo(node);
```

**Safe removal during iteration:**

```cpp
for (auto it = list.begin(); it != list.end(); ) {
    if (should_remove(*it)) {
        it = list.erase(it);  // erase returns next iterator
    } else {
        ++it;
    }
}
```

---

## Splice Operations

Splice transfers all elements from one list to another without copying or allocation.

### Splice All Elements

```cpp
fat_p::IntrusiveList<Task> source, dest;
// ... populate source ...

dest.splice(dest.end(), source);  // Transfer all from source to end of dest
// source is now empty
```

### Splice at Beginning

```cpp
dest.splice(dest.begin(), source);  // Insert source contents at front of dest
```

### Complexity by Policy

| Operation | Fast Policy | Safe Policy |
|-----------|-------------|-------------|
| splice(all) | O(1) | O(N) |

**Why Safe is O(N):** Safe policy must update the owner pointer for every transferred node.

### Self-Splice Guard

Splicing a list into itself is detected and handled as a no-op:

```cpp
list.splice(list.end(), list);  // Does nothing (same list)
```

---

## Correctness Contract

IntrusiveList enforces several invariants to prevent common bugs.

### Debug Assertions

In debug builds (without `NDEBUG`), the following are asserted:

1. **Double insertion prevention:** `insert()` asserts if node is already linked
2. **Destroyed-while-linked detection:** Node destructor asserts if still linked
3. **Iterator provenance:** Operations assert if iterator doesn't belong to this list

### Release Behavior

| Scenario | Fast Policy (Release) | Safe Policy (Release) |
|----------|----------------------|----------------------|
| Double insertion | Undefined behavior | Undefined behavior |
| Wrong-list remove | Undefined behavior | Safe no-op |
| Destroy while linked | Undefined behavior | Undefined behavior |

### Best Practices

```cpp
// 1. Always check isLinked() before operations that assume state
void add_to_ready(Task& t) {
    assert(!t.isLinked());  // Document your expectations
    ready_queue_.push_back(t);
}

// 2. Always unlink before destroy
void destroy_task(Task& t) {
    if (t.isLinked()) {
        current_list_.remove(t);
    }
    // Now safe to destroy
}

// 3. Use Safe policy when ownership is unclear
void external_api_remove(SafeTask& t) {
    // Caller might pass node from wrong list - safe no-op
    my_list_.remove(t);
}
```

---

## Common Patterns

### Free List / Object Pool

```cpp
template <typename T, size_t N>
class SimplePool {
    std::array<T, N> storage_;
    fat_p::IntrusiveList<T> free_list_;
    
public:
    SimplePool() {
        for (auto& obj : storage_) {
            free_list_.push_back(obj);
        }
    }
    
    T* acquire() {
        if (free_list_.empty()) return nullptr;
        T& obj = free_list_.front();
        free_list_.pop_front();
        return &obj;
    }
    
    void release(T& obj) {
        free_list_.push_back(obj);
    }
};
```

### Priority Queues (Multiple Lists)

```cpp
class PriorityScheduler {
    fat_p::IntrusiveList<Task> high_, medium_, low_;
    
public:
    void schedule(Task& t) {
        if (t.priority > 10) high_.push_back(t);
        else if (t.priority > 5) medium_.push_back(t);
        else low_.push_back(t);
    }
    
    Task* next() {
        if (!high_.empty()) {
            Task& t = high_.front();
            high_.pop_front();
            return &t;
        }
        // ... similar for medium, low ...
        return nullptr;
    }
};
```

### LRU Cache

```cpp
template <typename Key, typename Value>
class LRUCache {
    struct Entry : fat_p::IntrusiveListNode<Entry> {
        Key key;
        Value value;
    };
    
    std::unordered_map<Key, Entry*> map_;
    fat_p::IntrusiveList<Entry> order_;  // Most recent at back
    std::deque<Entry> storage_;
    size_t capacity_;
    
public:
    void access(const Key& key) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            Entry& e = *it->second;
            order_.remove(e);
            order_.push_back(e);  // Move to most recent
        }
    }
    
    void evict_oldest() {
        if (!order_.empty()) {
            Entry& oldest = order_.front();
            order_.pop_front();
            map_.erase(oldest.key);
        }
    }
};
```

---

## Thread Safety

IntrusiveList provides **no internal synchronization**. All concurrent access must be externally synchronized.

### Single-Threaded Use

No synchronization needed:

```cpp
fat_p::IntrusiveList<Task> queue;
queue.push_back(task);   // OK
queue.remove(task);      // OK
```

### Multi-Threaded Use

Protect with mutex:

```cpp
class ThreadSafeQueue {
    mutable std::mutex mutex_;
    fat_p::IntrusiveList<Task> queue_;
    
public:
    void push(Task& t) {
        std::lock_guard lock(mutex_);
        queue_.push_back(t);

---

## Error Handling Model

IntrusiveList uses **assertions for contract violations** rather than exceptions. This design reflects the zero-overhead philosophy: correct code pays no runtime cost for error checking.

### Debug Mode (NDEBUG not defined)

| Violation | Behavior |
|-----------|----------|
| Insert already-linked node | Assertion failure |
| Destroy node while linked | Assertion failure |
| Dereference end() iterator | Assertion failure |
| Iterator from wrong list (Safe policy) | Assertion failure |

### Release Mode (NDEBUG defined)

| Violation | Fast Policy | Safe Policy |
|-----------|-------------|-------------|
| Insert already-linked node | Undefined behavior | Undefined behavior |
| Destroy node while linked | Undefined behavior | Undefined behavior |
| Remove from wrong list | Undefined behavior | No-op (safe) |
| Dereference end() iterator | Undefined behavior | Undefined behavior |

### No Exceptions

IntrusiveList operations are `noexcept` where possible. The library never throws exceptions. User code in callbacks (e.g., during iteration) may throw, but IntrusiveList itself provides strong exception neutrality—if user code throws, list state remains consistent.

### Defensive Programming Pattern

```cpp
void safe_add(Task& t, fat_p::IntrusiveList<Task>& list) {
    // Precondition check
    assert(!t.isLinked() && "Task must not already be in a list");
    list.push_back(t);
}

void safe_remove(Task& t, fat_p::IntrusiveList<Task>& list) {
    // Only remove if actually linked
    if (t.isLinked()) {
        list.remove(t);
    }
}
```

---

## Performance Rules of Thumb

### Operation Costs

| Operation | Time | Allocations | Cache Behavior |
|-----------|------|-------------|----------------|
| `push_back/front` | ~2 ns | 0 | 2 cache lines touched |
| `remove` | ~2 ns | 0 | 3 cache lines touched |
| `isLinked()` | <1 ns | 0 | 1 cache line (node itself) |
| `iteratorTo()` | <1 ns | 0 | 1 cache line (node itself) |
| `splice` (Fast) | O(1) | 0 | 4 cache lines |
| `splice` (Safe) | O(N) | 0 | N+4 cache lines |
| `size()` | O(1) | 0 | 1 cache line (list header) |
| Iteration per node | ~2 ns | 0 | 1 cache line per node |

### Memory Layout Considerations

IntrusiveList nodes are allocated wherever you put them—stack, heap, array, pool. This gives you control over memory layout:

```cpp
// Contiguous allocation = cache-friendly iteration
std::vector<Task> tasks(1000);
fat_p::IntrusiveList<Task> list;
for (auto& t : tasks) {
    list.push_back(t);
}
// Iteration touches consecutive memory
```

### When IntrusiveList Wins

1. **Frequent add/remove in hot paths:** Zero allocation beats any allocator
2. **Object pool free lists:** O(1) acquire/release with no overhead
3. **Known-node removal:** O(1) removal vs O(N) for find-then-erase
4. **Memory-constrained systems:** No per-node allocation overhead

### When IntrusiveList Loses

1. **Iteration-dominated workloads:** std::vector is faster for sequential access
2. **Multi-list membership needed:** Each node can only be in one list
3. **Polymorphic storage:** Cannot store different derived types in same list
4. **Frequent splice with Safe policy:** O(N) owner updates add up

---
    
    bool try_pop(Task*& out) {
        std::lock_guard lock(mutex_);
        if (queue_.empty()) return false;
        out = &queue_.front();
        queue_.pop_front();
        return true;
    }
};
```

### Per-Thread Queues

Often better than shared queue:

```cpp
thread_local fat_p::IntrusiveList<Task> local_queue;

void worker_thread() {
    while (running) {
        for (Task& t : local_queue) {
            t.execute();
        }
        local_queue.clear();
        // Steal work from other threads...
    }
}
```

---

## Debug Mode

In debug builds (when `NDEBUG` is not defined), IntrusiveList provides additional safety checks.

### Enabled Assertions

1. **Node destructor:** Asserts if destroyed while linked
2. **insert():** Asserts if node already linked
3. **Iterator operations:** Assert on dereference of end iterator
4. **Iterator provenance:** Assert if iterator doesn't belong to this list

### Example Debug Output

```
Assertion failed: !this->isLinked() && "IntrusiveListNode destroyed while still linked"
File: fat_p/IntrusiveList.h, Line: 175
```

### Debug-Only Overhead

Debug mode performs additional validation. To get release performance in debug builds:

```cpp
#define NDEBUG
#include <fat_p/IntrusiveList.h>
```

---

## Migration from std::list

### Step 1: Add CRTP Inheritance

```cpp
// Before
struct Task {
    int priority;
};
std::list<Task*> taskList;

// After
struct Task : fat_p::IntrusiveListNode<Task> {
    int priority;
};
fat_p::IntrusiveList<Task> taskList;
```

### Step 2: Change Add Operations

```cpp
// Before: passing pointer
taskList.push_back(&task);

// After: passing reference
taskList.push_back(task);
```

### Step 3: Change Remove Operations

```cpp
// Before: find then erase
auto it = std::find(taskList.begin(), taskList.end(), &task);
if (it != taskList.end()) taskList.erase(it);

// After: direct remove
if (task.isLinked()) taskList.remove(task);
```

### Step 4: Change Iteration

```cpp
// Before: dereferencing pointers
for (Task* t : taskList) {
    process(*t);
}

// After: references directly
for (Task& t : taskList) {
    process(t);
}
```

### Migration Checklist

- [ ] Add CRTP inheritance to your type
- [ ] Change `std::list<T*>` to `fat_p::IntrusiveList<T>`
- [ ] Change `push_back(&obj)` to `push_back(obj)`
- [ ] Replace find+erase with `isLinked()` + `remove()`
- [ ] Update iteration to use references instead of pointers
- [ ] Ensure objects are removed before destruction
- [ ] Add external synchronization if multi-threaded
- [ ] Choose policy (Fast default, Safe if needed)

---

## Troubleshooting

### "Assertion failed: IntrusiveListNode destroyed while still linked"

**Cause:** Destroying an object that's still in a list.

**Fix:** Remove from list before destruction:
```cpp
if (task.isLinked()) {
    list.remove(task);
}
// Now safe to destroy
```

### "Assertion failed: insert() called with a node that is already linked"

**Cause:** Adding a node that's already in a list (same or different).

**Fix:** Check membership first:
```cpp
if (!task.isLinked()) {
    list.push_back(task);
}
```

### "Segmentation fault during iteration"

**Cause:** Likely destroying objects during iteration without using `erase()`.

**Fix:** Use `erase()` pattern:
```cpp
for (auto it = list.begin(); it != list.end(); ) {
    if (should_destroy(*it)) {
        it = list.erase(it);  // Returns next iterator
        // Now destroy the object if needed
    } else {
        ++it;
    }
}
```

### "isLinked() returns false but node should be linked"

**Cause:** Node was never added, or was removed by a different code path.

**Fix:** Add logging to track list operations in debug builds:
```cpp
void add_to_list(Task& t) {
    std::cout << "Adding task " << &t << " to list\n";
    list_.push_back(t);
}
```

---

## API Reference

### IntrusiveListNode<T, OwnerPolicy>

```cpp
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveListNode {
public:
    IntrusiveListNode() noexcept = default;
    ~IntrusiveListNode();  // Asserts if linked (debug mode)
    
    IntrusiveListNode(const IntrusiveListNode&) = delete;
    IntrusiveListNode& operator=(const IntrusiveListNode&) = delete;
    
    [[nodiscard]] bool isLinked() const noexcept;
};
```

### IntrusiveList<T, OwnerPolicy>

```cpp
template <typename T, typename OwnerPolicy = intrusive_list::FastOwnerPolicy>
class IntrusiveList {
public:
    // Types
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using iterator = IntrusiveListIterator<T, OwnerPolicy>;
    using const_iterator = IntrusiveListConstIterator<T, OwnerPolicy>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using size_type = std::size_t;
    
    // Construction/destruction
    IntrusiveList() noexcept;
    ~IntrusiveList();
    IntrusiveList(IntrusiveList&& other) noexcept;
    IntrusiveList& operator=(IntrusiveList&& other) noexcept;
    
    // Non-copyable
    IntrusiveList(const IntrusiveList&) = delete;
    IntrusiveList& operator=(const IntrusiveList&) = delete;
    
    // Capacity
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] size_type size() const noexcept;
    
    // Element access
    [[nodiscard]] reference front();
    [[nodiscard]] const_reference front() const;
    [[nodiscard]] reference back();
    [[nodiscard]] const_reference back() const;
    
    // Iterators
    [[nodiscard]] iterator begin() noexcept;
    [[nodiscard]] const_iterator begin() const noexcept;
    [[nodiscard]] const_iterator cbegin() const noexcept;
    [[nodiscard]] iterator end() noexcept;
    [[nodiscard]] const_iterator end() const noexcept;
    [[nodiscard]] const_iterator cend() const noexcept;
    [[nodiscard]] reverse_iterator rbegin() noexcept;
    [[nodiscard]] const_reverse_iterator rbegin() const noexcept;
    [[nodiscard]] reverse_iterator rend() noexcept;
    [[nodiscard]] const_reverse_iterator rend() const noexcept;
    [[nodiscard]] const_reverse_iterator crbegin() const noexcept;
    [[nodiscard]] const_reverse_iterator crend() const noexcept;
    
    // Modifiers
    void push_front(T& node);
    void push_back(T& node);
    void pop_front();
    void pop_back();
    iterator insert(iterator pos, T& node);
    iterator erase(iterator pos);
    void remove(T& node);
    void clear();
    
    // Splice (transfers all elements from other)
    void splice(iterator pos, IntrusiveList& other);
    
    // Node-to-iterator conversion
    [[nodiscard]] iterator iteratorTo(T& node);
    [[nodiscard]] const_iterator iteratorTo(const T& node) const;
};
```

### Type Aliases

```cpp
namespace fat_p {
    // List aliases
    template <typename T>
    using IntrusiveListFast = IntrusiveList<T, intrusive_list::FastOwnerPolicy>;
    
    template <typename T>
    using IntrusiveListSafe = IntrusiveList<T, intrusive_list::SafeOwnerPolicy>;
}
```

### Policy Types

```cpp
namespace fat_p::intrusive_list {
    // Fast policy (default): 16 bytes/node, O(1) splice, wrong-list remove is UB
    struct FastOwnerPolicy {
        static constexpr bool kHasOwner = false;
    };
    
    // Safe policy: 24 bytes/node, O(N) splice, wrong-list remove is safe no-op  
    struct SafeOwnerPolicy {
        static constexpr bool kHasOwner = true;
    };
}
```

---

## Summary

IntrusiveList is a specialized tool for zero-allocation list operations.

**The contract:**
1. Types inherit from `IntrusiveListNode<T>` (or `IntrusiveListNode<T, Policy>`)
2. Objects can be in at most one list at a time
3. Objects must be unlinked before destruction
4. List operations never allocate

**In return:**
1. O(1) insert, remove, membership test
2. Zero allocation in hot paths  
3. Correct bidirectional iteration (including `--end()`)
4. Policy choice for performance vs safety tradeoff

**Choose your policy:**
- **Fast (default):** 16 bytes/node, O(1) splice, wrong-list remove is UB
- **Safe:** 24 bytes/node, O(N) splice, wrong-list remove is safe no-op

---

*IntrusiveList.h — Fat-P Library v3.3*
