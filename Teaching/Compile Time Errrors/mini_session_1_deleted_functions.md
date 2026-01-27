# Mini-Session 1: Deleted Functions

## Explicitly Prohibiting Operations with `= delete`

**Estimated time:** 15–20 minutes  
**Prerequisites:** Basic C++ classes, function overloading  
**Guarantee:** ✅ Compile-time

---

## The One-Minute Summary

`= delete` explicitly forbids operations that would otherwise compile silently.

```cpp
class UniqueResource {
public:
    UniqueResource(const UniqueResource&) = delete;            // No copying
    UniqueResource& operator=(const UniqueResource&) = delete; // No copy-assign
    UniqueResource(double) = delete;                           // No conversion from double
};

UniqueResource a;
UniqueResource b = a;       // Compile error: use of deleted function
UniqueResource c = 3.14;    // Compile error: use of deleted function
```

---

## Use Cases

### 1. Prevent Copying (Move-Only Types)

```cpp
class FileHandle {
    int fd_;
public:
    explicit FileHandle(const char* path) : fd_(open(path, O_RDONLY)) {}
    ~FileHandle() { if (fd_ >= 0) close(fd_); }
    
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;
    
    FileHandle(FileHandle&& other) noexcept : fd_(other.fd_) { other.fd_ = -1; }
    FileHandle& operator=(FileHandle&& other) noexcept {
        if (this != &other) { if (fd_ >= 0) close(fd_); fd_ = other.fd_; other.fd_ = -1; }
        return *this;
    }
};
```

### 2. Prevent Implicit Conversions

```cpp
class UserId {
    int id_;
public:
    explicit UserId(int id) : id_(id) {}
    UserId(double) = delete;     // No UserId(3.14) → UserId(3)
    UserId(bool) = delete;       // No UserId(true) → UserId(1)
    UserId(char) = delete;       // No UserId('A') → UserId(65)
};

UserId a(42);     // OK
UserId b(3.14);   // Compile error
UserId c(true);   // Compile error
```

### 3. Prevent Dangerous Overloads

```cpp
class StringView {
public:
    StringView(const std::string& s) : data_(s.data()), size_(s.size()) {}
    StringView(std::string&&) = delete;  // Prevent dangling!
private:
    const char* data_;
    size_t size_;
};

std::string s = "hello";
StringView good(s);                    // OK
StringView bad(std::string("temp"));   // Compile error: would dangle
```

### 4. Prevent Heap Allocation

```cpp
class StackOnly {
public:
    void* operator new(size_t) = delete;
    void* operator new[](size_t) = delete;
};

StackOnly s;                  // OK
StackOnly* p = new StackOnly; // Compile error
```

### 5. Prevent Default Construction

```cpp
class MustInit {
public:
    MustInit() = delete;
    explicit MustInit(int v);
};

MustInit a;      // Compile error
MustInit b(42);  // OK
```

---

## Summary Table

| To Prevent | Delete This |
|------------|-------------|
| Copying | Copy constructor + copy assignment |
| Moving | Move constructor + move assignment |
| Conversion from X | Constructor taking X |
| Heap allocation | `operator new` |
| Default construction | Default constructor |

---

## Exercise

Add `= delete` to prevent a `Socket` class from being copied but allow moving.

---

## Further Reading

- [C++ Core Guidelines C.81](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c81-use-delete-when-you-want-to-disable-default-behavior-without-wanting-an-alternative)
- Session 1: Strong Typedefs
