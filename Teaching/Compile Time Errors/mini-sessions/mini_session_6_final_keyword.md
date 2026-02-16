# Mini-Session 6: The final Keyword

## Preventing Inheritance and Override

**Estimated time:** 10–15 minutes  
**Prerequisites:** Basic inheritance and virtual functions  
**Guarantee:** ✅ Compile-time

---

## The One-Minute Summary

`final` prevents inheritance or method override:

```cpp
class Singleton final { };           // Cannot inherit from Singleton
class Base {
    virtual void process() final;    // Cannot override process()
};
```

---

## final on Classes

Prevent inheritance entirely:

```cpp
class HttpClient final {
public:
    void get(const std::string& url);
    void post(const std::string& url, const std::string& body);
    // Not designed for inheritance—behavior is complete
};

class MyClient : public HttpClient { };  // Compile error: HttpClient is final
```

**Use when:**
- Class is not designed for inheritance
- Singleton or utility class
- Performance: enables devirtualization
- Security: prevent behavior modification

---

## final on Virtual Methods

Prevent further override in derived classes:

```cpp
class Shape {
public:
    virtual double area() const = 0;
    virtual void draw() const;
};

class Circle : public Shape {
public:
    double area() const override final {  // No further overrides allowed
        return 3.14159 * radius_ * radius_;
    }
    void draw() const override;  // Can still be overridden
};

class FancyCircle : public Circle {
    double area() const override;  // Compile error: area() is final
    void draw() const override;    // OK
};
```

**Use when:**
- Method behavior must not change in further derivation
- Non-virtual interface pattern (NVI)
- Performance: enables devirtualization

---

## Devirtualization

`final` enables the compiler to optimize virtual calls:

```cpp
class Base {
public:
    virtual void process();
};

class Derived final : public Base {
public:
    void process() override;
};

void use(Derived& d) {
    d.process();  // Compiler knows exact type → direct call, no vtable lookup
}

void use(Base& b) {
    b.process();  // Must use vtable—type unknown at compile time
}
```

With `final`, the compiler can:
- Skip vtable lookup
- Inline the function
- Eliminate dead code

---

## When to Use final

### Use final on Classes When:

| Scenario | Example |
|----------|---------|
| Not designed for inheritance | Utility classes, singletons |
| Complete implementation | `std::vector`, `std::string` |
| Security-sensitive | Prevent behavior modification |
| Performance-critical | Enable devirtualization |

### Use final on Methods When:

| Scenario | Example |
|----------|---------|
| Behavior must not change | Core algorithm steps |
| Template method pattern | Fixed parts of algorithm |
| Non-virtual interface | Public methods calling private virtuals |

---

## Relationship with override

Both `final` and `override` are contextual keywords for virtual functions:

```cpp
class Derived : public Base {
    void foo() override;        // Must override something
    void bar() final;           // Cannot be overridden further
    void baz() override final;  // Both: overrides Base::baz, cannot be overridden
};
```

Always use `override` when overriding. Add `final` when that override should be the last.

---

## Summary

| Keyword | On Class | On Method |
|---------|----------|-----------|
| `final` | No inheritance allowed | No further override allowed |
| `override` | N/A | Must override base method |

```cpp
class Complete final {           // Cannot inherit
    virtual void f() final;      // Cannot override (even in hypothetical subclass)
};

class Partial : public Base {
    void g() override final;     // Overrides Base::g, no further override
};
```

---

## Exercise

Identify classes in your codebase that:
1. Have no virtual methods and shouldn't be inherited
2. Have virtual methods that should not be overridden further

Add `final` appropriately.

---

## Further Reading

- [C++ Core Guidelines C.139](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c139-use-final-on-classes-sparingly)
- [C++ Core Guidelines C.128](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c128-virtual-functions-should-specify-exactly-one-of-virtual-override-or-final)
