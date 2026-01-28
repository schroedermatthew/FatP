# Handbook: Phantom Types

## Zero-Cost Type Distinctions Through Unused Template Parameters

**Estimated time:** 30–40 minutes  
**Prerequisites:** Templates, Session 1 (Strong Typedefs), Session 4 (Type-State)  
**Guarantee:** ✅ Compile-time type safety with zero runtime overhead

---

## The Pattern

A **phantom type** is a template parameter that appears in the type but is never used at runtime. It exists solely to create distinct types that the compiler treats as incompatible.

```cpp
template<typename Tag>
class StrongInt {
    int value_;  // Tag is NOT stored!
public:
    explicit StrongInt(int v) : value_(v) {}
    int get() const { return value_; }
};

struct UserIdTag {};
struct DocumentIdTag {};

using UserId = StrongInt<UserIdTag>;
using DocumentId = StrongInt<DocumentIdTag>;

// Same runtime representation, different types
static_assert(sizeof(UserId) == sizeof(int));
static_assert(sizeof(DocumentId) == sizeof(int));

UserId user{42};
DocumentId doc{42};
user = doc;  // Compile error: different types!
```

The `Tag` parameter is "phantom" — it affects the type but not the value.

---

## StrongId Deconstructed

FAT-P's `StrongId` is a phantom type pattern:

```cpp
template<typename Tag, typename T = int>
class StrongId {
    T value_;  // Only T is stored, Tag is phantom
public:
    explicit StrongId(T v) : value_(v) {}
    T value() const { return value_; }
    
    bool operator==(StrongId other) const { return value_ == other.value_; }
    bool operator<(StrongId other) const { return value_ < other.value_; }
};
```

The tag creates a family of incompatible types:

```cpp
using UserId = StrongId<struct UserTag>;
using OrderId = StrongId<struct OrderTag>;
using ProductId = StrongId<struct ProductTag>;

void process_order(UserId user, OrderId order, ProductId product);

process_order(user_id, order_id, product_id);  // OK
process_order(order_id, user_id, product_id);  // Compile error!
```

---

## Beyond IDs: Units as Phantom Types

Physical units can be encoded as phantom types:

```cpp
template<typename Unit>
class Quantity {
    double value_;
public:
    explicit Quantity(double v) : value_(v) {}
    double get() const { return value_; }
    
    Quantity operator+(Quantity other) const {
        return Quantity{value_ + other.value_};
    }
    
    Quantity operator-(Quantity other) const {
        return Quantity{value_ - other.value_};
    }
    
    Quantity operator*(double scalar) const {
        return Quantity{value_ * scalar};
    }
};

struct Meters {};
struct Feet {};
struct Seconds {};

using Distance = Quantity<Meters>;
using Duration = Quantity<Seconds>;

Distance d1{100};
Distance d2{50};
Distance d3 = d1 + d2;  // OK: same unit

Duration t{10};
auto bad = d1 + t;  // Compile error: Quantity<Meters> + Quantity<Seconds>
```

For full dimensional analysis (velocity = distance/time), see Session 11: Physical Units.

---

## States as Phantom Types

The Type-State pattern uses phantom types to encode object state:

```cpp
template<typename State>
class File {
    int fd_;
    
    template<typename> friend class File;  // Allow state transitions
    
    explicit File(int fd) : fd_(fd) {}
    
public:
    // Only Closed files can be opened
    static File<struct Open> open(const char* path)
        requires std::same_as<State, struct Closed>
    {
        int fd = ::open(path, O_RDWR);
        if (fd < 0) throw std::runtime_error("open failed");
        return File<Open>{fd};
    }
    
    // Only Open files can be read/written
    void write(const char* data)
        requires std::same_as<State, struct Open>
    {
        ::write(fd_, data, strlen(data));
    }
    
    std::string read(size_t n)
        requires std::same_as<State, struct Open>
    {
        std::string buf(n, '\0');
        ::read(fd_, buf.data(), n);
        return buf;
    }
    
    // Only Open files can be closed
    File<struct Closed> close() &&
        requires std::same_as<State, struct Open>
    {
        ::close(fd_);
        return File<Closed>{-1};
    }
};

using ClosedFile = File<struct Closed>;
using OpenFile = File<struct Open>;

// Usage
ClosedFile cf{};
OpenFile of = cf.open("data.txt");
of.write("hello");
auto data = of.read(100);
ClosedFile cf2 = std::move(of).close();

// These don't compile:
// cf.write("x");      // Error: Closed file can't write
// cf.read(10);        // Error: Closed file can't read
// of.open("other");   // Error: Open file can't open
```

---

## Permissions as Phantom Types

Encode access permissions in types:

```cpp
template<typename Permission>
class DatabaseQuery {
    std::string sql_;
public:
    explicit DatabaseQuery(std::string sql) : sql_(std::move(sql)) {}
    const std::string& sql() const { return sql_; }
};

struct ReadOnly {};
struct ReadWrite {};

using ReadQuery = DatabaseQuery<ReadOnly>;
using WriteQuery = DatabaseQuery<ReadWrite>;

class Database {
public:
    // Read queries can run on any replica
    ResultSet execute(ReadQuery q) {
        return run_on_any_replica(q.sql());
    }
    
    // Write queries must run on primary
    ResultSet execute(WriteQuery q) {
        return run_on_primary(q.sql());
    }
};

ReadQuery select{"SELECT * FROM users"};
WriteQuery update{"UPDATE users SET active = true"};

db.execute(select);  // Runs on replica
db.execute(update);  // Runs on primary

// Type system prevents running write query on replica
```

---

## Validation State as Phantom Types

Track whether data has been validated:

```cpp
template<typename ValidationState>
class UserInput {
    std::string data_;
public:
    explicit UserInput(std::string s) : data_(std::move(s)) {}
    const std::string& get() const { return data_; }
};

struct Raw {};
struct Sanitized {};

using RawInput = UserInput<Raw>;
using SafeInput = UserInput<Sanitized>;

// Only sanitized input can be rendered
void render_html(SafeInput input) {
    std::cout << input.get();  // Safe: already sanitized
}

// Sanitization converts Raw to Sanitized
SafeInput sanitize(RawInput input) {
    std::string clean = escape_html(input.get());
    return SafeInput{std::move(clean)};
}

// Usage
RawInput user_data{get_from_request()};
render_html(user_data);  // Compile error: Raw != Sanitized
render_html(sanitize(user_data));  // OK
```

---

## Ownership as Phantom Types

Distinguish owned vs borrowed data:

```cpp
template<typename Ownership>
class StringHandle {
    const char* data_;
    size_t size_;
public:
    StringHandle(const char* d, size_t s) : data_(d), size_(s) {}
    
    const char* data() const { return data_; }
    size_t size() const { return size_; }
};

struct Owned {};
struct Borrowed {};

using OwnedString = StringHandle<Owned>;
using BorrowedString = StringHandle<Borrowed>;

class Storage {
public:
    // Takes ownership—will store the string
    void store(OwnedString s);
    
    // Just reads—won't store
    void process(BorrowedString s);
};

// Caller must be explicit about ownership transfer
storage.store(OwnedString{strdup(data), len});
storage.process(BorrowedString{data, len});
```

---

## Zero Overhead Proof

Phantom types have no runtime cost:

```cpp
template<typename Tag>
class Tagged {
    int value_;
public:
    explicit Tagged(int v) : value_(v) {}
    int get() const { return value_; }
};

using A = Tagged<struct TagA>;
using B = Tagged<struct TagB>;

int use_phantom(A a, B b) {
    return a.get() + b.get();
}

int use_raw(int a, int b) {
    return a + b;
}
```

Both functions compile to identical assembly:

```asm
use_phantom:
    lea eax, [rdi + rsi]
    ret

use_raw:
    lea eax, [rdi + rsi]
    ret
```

The phantom tag exists only at compile time.

---

## Combining Phantom Types

Multiple phantom parameters can encode multiple properties:

```cpp
template<typename Unit, typename Precision, typename Validated>
class Measurement {
    double value_;
public:
    // ...
};

struct Meters {};
struct HighPrecision {};
struct LowPrecision {};
struct Calibrated {};
struct Uncalibrated {};

using CalibratedPreciseMeasurement = 
    Measurement<Meters, HighPrecision, Calibrated>;

// Only calibrated measurements can be used in calculations
template<typename U, typename P>
auto calculate(Measurement<U, P, Calibrated> m);
```

---

## Design Guidelines

### When to Use Phantom Types

| Use Case | Benefit |
|----------|---------|
| IDs and handles | Prevent argument swapping |
| Units and dimensions | Prevent unit confusion |
| State encoding | Invalid operations don't compile |
| Permission levels | Access control at compile time |
| Validation tracking | Unvalidated data can't reach sensitive code |

### When NOT to Use

| Situation | Alternative |
|-----------|-------------|
| State changes at runtime | Runtime state machine |
| Need to store mixed types | std::variant or inheritance |
| Simple internal code | Plain types, good naming |
| Prototyping | Add types later |

### Implementation Tips

1. **Use incomplete types for tags** — `struct MyTag;` (no definition needed)

2. **Friend templates for state transitions** — allow moving data between states

3. **`requires` clauses for conditional methods** — methods exist only in certain states

4. **Type aliases for users** — hide the template parameters

5. **Static assert size equality** — verify no runtime overhead

---

## Summary

| Application | Phantom Type Encodes |
|-------------|---------------------|
| StrongId | Identity domain |
| Quantity | Physical unit |
| File | Open/closed state |
| Query | Read/write permission |
| Input | Raw/sanitized status |
| Handle | Owned/borrowed |

### Key Principles

1. **Phantom parameters affect type, not value** — zero runtime storage

2. **Different phantom types are incompatible** — compiler enforces distinctions

3. **Use for compile-time properties** — things known statically

4. **Combine with requires/enable_if** — conditional method availability

5. **Zero overhead guaranteed** — same assembly as untyped code

### The Guideline in One Sentence

> Use phantom types to create zero-cost type distinctions that the compiler enforces.

---

## Further Reading

- Session 1: Strong Typedefs — primary use case
- Session 4: Type-State Pattern — state as phantom types
- Session 10: Builder Type Accumulation — phantom types for builder state
- "Phantom Types and Subtyping" — academic paper on the concept
