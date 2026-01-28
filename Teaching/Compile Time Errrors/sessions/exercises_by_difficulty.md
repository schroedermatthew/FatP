# Exercises: Compile-Time Safety

## Graded Exercises from Beginner to Advanced

---

## How to Use This Document

Exercises are organized by difficulty:
- **Beginner (B):** Apply single techniques to simple problems
- **Intermediate (I):** Combine techniques, handle edge cases
- **Advanced (A):** Design complex systems, optimize for real-world constraints
- **Projects (P):** Multi-day projects integrating multiple concepts

Each exercise includes:
- Problem statement
- Hints (expandable)
- Techniques involved
- Estimated time

---

## Beginner Exercises

### B1: Add [[nodiscard]] Audit

**Time:** 30 minutes  
**Techniques:** [[nodiscard]] (Session 7)

**Problem:**  
Take any C++ file from your codebase (or use the code below) and add `[[nodiscard]]` to all functions where ignoring the return value would be a bug.

```cpp
class FileSystem {
public:
    bool create_directory(const std::string& path);
    bool delete_file(const std::string& path);
    bool file_exists(const std::string& path);
    std::string read_file(const std::string& path);
    size_t file_size(const std::string& path);
    void set_permissions(const std::string& path, int mode);
    int get_permissions(const std::string& path);
};
```

**Tasks:**
1. Identify which functions should have `[[nodiscard]]`
2. Add the attribute
3. Write test code that triggers warnings for ignored returns
4. Document why each function does or doesn't need `[[nodiscard]]`

<details>
<summary>Hints</summary>

- `create_directory`, `delete_file`: return success/failure → `[[nodiscard]]`
- `file_exists`: checking existence is the whole point → `[[nodiscard]]`
- `read_file`: content is the result → `[[nodiscard]]`
- `file_size`, `get_permissions`: computed values → `[[nodiscard]]`
- `set_permissions`: void return, side effect → no `[[nodiscard]]`

</details>

---

### B2: Enable -Werror=switch-enum

**Time:** 20 minutes  
**Techniques:** Enum exhaustiveness (Session 2)

**Problem:**  
Given this code:

```cpp
enum class HttpMethod { GET, POST, PUT, DELETE, PATCH };

std::string method_to_string(HttpMethod m) {
    switch (m) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        default: return "UNKNOWN";
    }
}
```

**Tasks:**
1. Compile with `-Werror=switch-enum`
2. Fix all warnings by adding missing cases
3. Remove the `default` case
4. Add a new enum value `HEAD` and verify the compiler catches the missing case

<details>
<summary>Hints</summary>

- Missing: `DELETE`, `PATCH`
- After removing `default`, any new value will cause a compile error
- The `default` was hiding the missing cases

</details>

---

### B3: Replace int IDs with StrongId

**Time:** 45 minutes  
**Techniques:** Strong typedefs (Session 1)

**Problem:**  
This code has a bug where user_id and document_id are swapped:

```cpp
void grant_access(int user_id, int document_id, int permission_level);

void process_request(int doc, int user, int level) {
    grant_access(doc, user, level);  // BUG: swapped!
}
```

**Tasks:**
1. Define `StrongId` types for `UserId`, `DocumentId`, and `PermissionLevel`
2. Update `grant_access` signature
3. Update `process_request` to use the correct types
4. Verify the compiler catches the swapped arguments

<details>
<summary>Hints</summary>

```cpp
template<typename Tag, typename T = int>
class StrongId {
    T value_;
public:
    explicit StrongId(T v) : value_(v) {}
    T value() const { return value_; }
};

using UserId = StrongId<struct UserTag>;
using DocumentId = StrongId<struct DocTag>;
using PermissionLevel = StrongId<struct PermTag>;
```

</details>

---

### B4: Const Correctness Audit

**Time:** 30 minutes  
**Techniques:** const (Session 5)

**Problem:**  
Add `const` everywhere possible:

```cpp
class Rectangle {
    double width;
    double height;
public:
    Rectangle(double w, double h) : width(w), height(h) {}
    
    double area() { return width * height; }
    double perimeter() { return 2 * (width + height); }
    
    void scale(double factor) {
        width *= factor;
        height *= factor;
    }
    
    double get_width() { return width; }
    double get_height() { return height; }
    
    void set_width(double w) { width = w; }
    void set_height(double h) { height = h; }
};

void print_info(Rectangle r) {
    std::cout << "Area: " << r.area() << "\n";
}
```

**Tasks:**
1. Add `const` to all member functions that don't modify state
2. Add `const` to parameters that shouldn't be modified
3. Add `const` to local variables where appropriate
4. Use `const Rectangle&` for pass-by-reference

<details>
<summary>Solution</summary>

```cpp
class Rectangle {
    double width_;
    double height_;
public:
    Rectangle(const double w, const double h) : width_(w), height_(h) {}
    
    double area() const { return width_ * height_; }
    double perimeter() const { return 2 * (width_ + height_); }
    
    void scale(const double factor) {
        width_ *= factor;
        height_ *= factor;
    }
    
    double get_width() const { return width_; }
    double get_height() const { return height_; }
    
    void set_width(const double w) { width_ = w; }
    void set_height(const double h) { height_ = h; }
};

void print_info(const Rectangle& r) {
    std::cout << "Area: " << r.area() << "\n";
}
```

</details>

---

### B5: Convert Pointer to Reference

**Time:** 20 minutes  
**Techniques:** Non-null references (Session 6)

**Problem:**  
Refactor to use references:

```cpp
void print_user(User* user) {
    if (user == nullptr) {
        std::cout << "No user\n";
        return;
    }
    std::cout << "User: " << user->name << "\n";
}

void process_users(std::vector<User*> users) {
    for (User* u : users) {
        print_user(u);
    }
}
```

**Tasks:**
1. Change `print_user` to take `const User&`
2. Update `process_users` to handle the "no user" case differently
3. Decide what the right behavior is when there's no user

<details>
<summary>Hints</summary>

- If `print_user` requires a user, use `const User&`
- If nulls are expected in the vector, filter them out or use `std::optional`
- Consider using `std::vector<User>` instead of `std::vector<User*>`

</details>

---

## Intermediate Exercises

### I1: State Machine for TCP Connection

**Time:** 2 hours  
**Techniques:** StateMachine (Session 3), enum exhaustiveness (Session 2)

**Problem:**  
Implement a simplified TCP state machine with these states:
- CLOSED
- LISTEN
- SYN_SENT
- SYN_RECEIVED
- ESTABLISHED
- FIN_WAIT_1
- FIN_WAIT_2
- CLOSE_WAIT
- CLOSING
- LAST_ACK
- TIME_WAIT

**Tasks:**
1. Define the state enum
2. Define valid transitions (see TCP state diagram)
3. Implement entry/exit actions (logging is fine)
4. Make invalid transitions fail at compile time or immediately at runtime
5. Write tests for valid and invalid transitions

---

### I2: Type-State File Handle

**Time:** 1.5 hours  
**Techniques:** Type-State (Session 4), phantom types (Handbook)

**Problem:**  
Create a file handle where:
- `write()` only available when open for writing
- `read()` only available when open for reading
- `close()` consumes the handle
- Opening returns a new type

```cpp
// Target API
ClosedFile cf;
auto rf = cf.open_read("data.txt");
std::string content = rf.read();
auto cf2 = std::move(rf).close();

auto wf = cf2.open_write("output.txt");
wf.write("hello");
auto cf3 = std::move(wf).close();

// These should not compile:
// cf.read();          // Can't read closed file
// rf.write("x");      // Can't write to read-mode file
// rf.read();          // Can't use after close (moved from)
```

---

### I3: Exhaustive Variant Visitor

**Time:** 1 hour  
**Techniques:** Variant exhaustiveness (Session 9)

**Problem:**  
Create a calculator expression type:

```cpp
using Expr = std::variant<
    double,                          // Literal
    std::tuple<char, Expr*, Expr*>,  // BinaryOp: op, left, right
    std::tuple<char, Expr*>,         // UnaryOp: op, operand
    std::string                      // Variable
>;
```

**Tasks:**
1. Implement `evaluate(const Expr&, const std::map<std::string, double>&)`
2. Implement `to_string(const Expr&)`
3. Use the `overloaded{}` pattern
4. Add a new expression type `FunctionCall` and verify compiler catches missing handlers

---

### I4: Expected Error Propagation

**Time:** 1 hour  
**Techniques:** [[nodiscard]], Expected (Session 7)

**Problem:**  
Implement a config file loader:

```cpp
Expected<Config, ConfigError> load_config(const std::string& path);
```

Where loading can fail due to:
- File not found
- Permission denied
- Parse error (with line number)
- Validation error (with field name)

**Tasks:**
1. Define `ConfigError` enum or class
2. Implement using `EXPECTED_TRY` or monadic operations
3. Write code that handles each error case differently
4. Ensure no error can be silently ignored

---

### I5: Concept-Constrained Container

**Time:** 1.5 hours  
**Techniques:** Concepts (Session 8)

**Problem:**  
Write a `SortedVector<T>` that maintains elements in sorted order.

**Requirements:**
- T must be copyable
- T must be totally ordered (`<` operator)
- Provide `insert()`, `contains()`, `erase()`
- Use binary search for O(log n) lookup

**Tasks:**
1. Define a concept `Sortable` that captures requirements
2. Constrain the template with the concept
3. Test with valid types (int, string)
4. Test with invalid types (custom type without `<`) and verify clear error message

---

## Advanced Exercises

### A1: Type-Accumulating Builder

**Time:** 3 hours  
**Techniques:** Builder type accumulation (Session 10), phantom types

**Problem:**  
Create an HTTP request builder where:
- `method` is required
- `url` is required
- `headers` is optional
- `body` is optional but requires method to be POST/PUT/PATCH
- `timeout` is optional

`build()` should only compile when all requirements are met.

**Tasks:**
1. Design the type state system
2. Implement with C++20 concepts or C++17 SFINAE
3. Ensure `body()` is only available after `method("POST")` etc.
4. Verify all constraints at compile time

---

### A2: Physical Units Library

**Time:** 4 hours  
**Techniques:** Phantom types, dimensional analysis (Session 11)

**Problem:**  
Implement a mini units library supporting:
- Length (meters, feet)
- Time (seconds, minutes)
- Velocity (derived: length/time)
- Acceleration (derived: velocity/time)

**Tasks:**
1. Use template parameters for dimension exponents
2. Implement `+`, `-`, `*`, `/` with correct dimension tracking
3. Ensure adding meters to seconds fails at compile time
4. Implement conversion between compatible units (meters ↔ feet)
5. Verify zero runtime overhead

---

### A3: Policy-Based Allocator

**Time:** 3 hours  
**Techniques:** Policy-based design (Handbook)

**Problem:**  
Create a `Vector<T, AllocPolicy, BoundsPolicy>` where:
- `AllocPolicy`: Heap, Stack<N>, Pool
- `BoundsPolicy`: NoBounds, AssertBounds, ThrowBounds

**Tasks:**
1. Define policy interfaces
2. Implement at least 2 allocation policies
3. Implement all 3 bounds policies
4. Verify zero overhead for disabled policies
5. Use static_assert to enforce policy requirements

---

### A4: Compile-Time State Machine Validator

**Time:** 4 hours  
**Techniques:** Constexpr, static_assert, type lists

**Problem:**  
Create a state machine where invalid transitions are caught at compile time.

```cpp
// Target: this should fail to compile
constexpr StateMachine<
    States<Idle, Running, Stopped>,
    Transitions<
        Transition<Idle, Running>,
        Transition<Running, Stopped>,
        Transition<Stopped, Idle>
    >
> sm;

sm.transition<Idle, Stopped>();  // Compile error: no such transition
```

**Tasks:**
1. Use variadic templates for states and transitions
2. Use constexpr and static_assert for validation
3. Make invalid transitions fail at compile time
4. Generate clear error messages

---

## Projects

### P1: Safe JSON Library (3-5 days)

**Techniques:** Expected, variant exhaustiveness, strong types

**Build a JSON library where:**
- Parse errors return `Expected<Json, ParseError>`
- Type access uses variant visitors (exhaustive)
- Paths are strongly typed
- Null is explicit (`std::optional` or `std::monostate`)

**Deliverables:**
- Parser with good error messages
- Type-safe accessors
- Serializer
- Test suite

---

### P2: Protocol State Machine (3-5 days)

**Techniques:** StateMachine, Type-State, Expected

**Implement an HTTP/1.1 request parser as a state machine:**
- Request line → Headers → Body
- Chunked transfer encoding
- Error states for malformed input

**Deliverables:**
- State machine definition
- Parser implementation
- Comprehensive tests
- Documentation

---

### P3: Compile-Time SQL Query Builder (5-7 days)

**Techniques:** Type accumulation, phantom types, concepts

**Build a query builder where:**
- SELECT/FROM/WHERE/JOIN/ORDER BY
- Type-safe column references
- Invalid queries don't compile
- Generates SQL strings

**Example:**
```cpp
auto query = Query::from<Users>()
    .select<&Users::name, &Users::email>()
    .where<&Users::active>(Eq{true})
    .order_by<&Users::name>(Asc)
    .build();
// "SELECT name, email FROM users WHERE active = true ORDER BY name ASC"
```

**Deliverables:**
- Query builder with compile-time checks
- Support for common SQL operations
- Type-safe joins
- Test suite

---

## Exercise Checklist

### Before Attempting

- [ ] Read the relevant session(s)
- [ ] Understand the technique being practiced
- [ ] Have a working C++ environment
- [ ] Know how to enable relevant compiler warnings

### After Completing

- [ ] Code compiles without warnings (with strict flags)
- [ ] Invalid operations fail at compile time
- [ ] Runtime checks exist for unavoidable runtime conditions
- [ ] Tests cover valid and invalid inputs
- [ ] Code is documented

---

## Difficulty Progression

```
B1-B5 → I1-I2 → I3-I5 → A1-A2 → A3-A4 → P1-P3
 │        │       │       │       │       │
 └────────┴───────┴───────┴───────┴───────┘
           Increasing complexity
```

**Recommended path:**
1. Complete all Beginner exercises
2. Do I1 (State Machine) and I2 (Type-State)
3. Do I3 (Variants) and I4 (Expected)
4. Choose one Advanced exercise based on interest
5. Attempt a Project

---

## Further Practice

- Audit your own codebase using these techniques
- Review open-source projects for compile-time safety patterns
- Read FAT-P source code for implementation examples
- Contribute improvements to FAT-P library
