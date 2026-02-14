---
doc_id: OV-REFLECTION-001
doc_type: "Overview"
title: "Reflection"
fatp_components: ["Reflection", "Reflectable", "FieldAccessor"]
topics: ["compile-time reflection", "struct field iteration", "NTTP field names", "macro-based registration", "visitor pattern", "generic programming", "member pointers", "type introspection"]
constraints: ["no native C++ reflection before C++26", "struct boilerplate for serialization/logging/comparison", "macro-based registration required", "32-field maximum"]
cxx_standard: "C++20"
std_equivalent: "P2996 (proposed for C++26, not yet available)"
boost_equivalent: "Boost.PFR (aggregate types only, no names in C++17)"
build_modes: ["Debug", "Release"]
last_verified: "2026-02-10"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Overview - Reflection

*Fat-P Library — February 2026*

---

## Executive Summary

Reflection is a compile-time struct introspection system that bridges the gap between what the C++ compiler knows about your types and what your code can access. A single macro call registers a type's fields, after which they can be iterated, accessed by index or name, converted to tuples, and formatted for debugging—all resolved at compile time with zero runtime overhead. The implementation exploits C++20 non-type template parameters to store field names as compile-time strings, achieving what was impossible in earlier standards without runtime storage. The system is designed to become obsolete gracefully when C++26's P2996 reflection lands, but until then it fills a gap the language has never filled.

---

## Overview Card

**Component:** Reflection
**Problem solved:** C++ has no built-in mechanism to iterate struct fields, retrieve field names, or generically process arbitrary struct types
**When to use:** Serialization, logging, debug output, generic comparison, configuration binding, ORM mapping—any code that needs to process struct fields without knowing the type at compile time
**When NOT to use:** Types with private fields (requires public members); types with more than 32 fields; projects that can use C++26 native reflection
**Key guarantee:** Zero runtime overhead for compile-time indexed access; all field metadata resolved at compile time
**std equivalent:** P2996 (proposed for C++26, not yet standardized or implemented)
**Boost equivalent:** Boost.PFR (aggregate-only, no field names in C++17)
**Other alternatives:** refl-cpp, magic_enum (enums only), manual boilerplate
**Read next:** User Manual - Reflection

---

## The Problem Domain

### What Reflection Is: A Cross-Language Perspective

The question "what are the fields of this object?" has a different answer in every programming language, and those answers reveal deep design choices about what a language considers important.

LISP, dating to 1958, was the first language where programs could inspect their own structure. Code and data shared the same representation—the S-expression—so a program could traverse its own source tree as easily as any other list. This wasn't called "reflection" yet (that term came later, from Brian Smith's 1982 doctoral thesis at MIT), but the fundamental capability was there: a program that could ask questions about its own composition.

Smalltalk, developed at Xerox PARC in the 1970s, made introspection a first-class design principle. Every object knew its class. Every class knew its methods and instance variables. The `instVarNames` message returned a collection of field names. The debugger, the inspector, the serializer—all were built on this self-knowledge. Alan Kay's insight was that if objects can describe themselves, then generic tools become possible: a single inspector can display any object, a single serializer can persist any object, a single debugger can examine any object.

Java (1995) brought reflection to mainstream systems programming. The JVM's bytecode format preserves class structure—field names, types, method signatures, annotations—in metadata tables shipped with every compiled class. `Class.getDeclaredFields()` returns an array of `Field` objects. The cost is real: metadata occupies memory in every loaded class, and reflection calls are 10–100× slower than direct access because they involve name lookups, type checks, and security verification.

C# (2000) extended the model with attributes—custom metadata annotations that the programmer attaches to fields, methods, and classes. `[JsonProperty("user_name")]` on a field tells the serializer to use a different JSON key. The reflection API reads these annotations at runtime. This moved beyond "what fields exist?" to "what should I do with them?"—a richer model, but one that deepens the commitment to runtime metadata.

Python takes the most radical approach. Every object is fundamentally a dictionary. `obj.__dict__` returns the attribute map. `getattr(obj, name)` looks up an attribute by runtime string. There is essentially no compilation—the interpreter preserves everything because it needs everything.

Each of these languages made the same basic choice: preserve type metadata alongside the type, and provide an API to query it. The tradeoff is binary size (strings in the compiled output), runtime speed (lookup overhead), and sometimes security (reflection can bypass access controls). For their target domains—application development, enterprise software, scripting—the tradeoff is acceptable.

C++ made the opposite choice.

### What the C++ Compiler Knows and Won't Tell You

When the C++ compiler processes a struct definition, it builds a complete internal model: field names, types, sizes, offsets, alignment, constructors, destructors, conversion operators. It uses every piece of this model during compilation—resolving member access expressions, computing layout for aggregate initialization, generating parameter passing code.

Then it discards the names.

The compiled binary contains `mov` instructions that use computed offsets. `sensor.temperature` becomes "load 8 bytes at base address plus 32." The string `"temperature"` served its purpose during name resolution and does not appear in the object file. There is no metadata table. There is no query API.

This is a foundational design decision, not an oversight. C++ targets domains where every byte of binary size matters—embedded firmware in 256 KB of flash, kernel modules where metadata would be loaded into precious non-paged memory, real-time control loops where the nanoseconds spent on name lookups would violate timing guarantees. The principle is: you don't pay for what you don't use. If your program never asks "what are the fields of Sensor?", the information required to answer that question should not exist in your binary.

The consequence is that every operation that processes all fields of a struct—serialization, deserialization, comparison, hashing, logging, validation, database mapping, configuration binding—must enumerate those fields by hand. Each enumeration is a maintenance liability: add a field to the struct, and every consumer must be updated independently.

### The Maintenance Surface

The bug pattern is always the same. A struct gains a new field during a feature request. The serializer and deserializer are updated because their tests broke. The logger compiles fine without the new field—it just silently omits it. The comparator compiles fine too—it just considers two objects equal when they differ on the new field.

A modest codebase with 30 struct types and 4 field-enumerating operations each has 120 hand-written field lists. A large codebase might have 200 types and 6 operations each—1,200 sites. Every struct modification is a potential silent bug at every site. The compiler cannot warn about a field that was never mentioned.

This is the problem reflection solves. Register a type's fields once, in one place, and write every consumer as a generic visitor over "all fields." Add a field, add it to the registration, and every consumer picks it up automatically.

---

## Architecture: Three Techniques Combined

Reflection combines three C++ mechanisms that, individually, are well-known but insufficient. Together, they simulate what other languages provide natively.

### Member Pointers for Field Access

`&Sensor::temperature` produces a pointer-to-data-member—on most ABIs, simply the byte offset of the field within the struct. When used as a template parameter, the offset is resolved at compile time. `Field::get(obj)` compiles to the same machine code as `obj.temperature`: a single memory access at a known offset.

This gives C++ the same access capability as Java's `Field.get(object)`, but resolved entirely at compile time with no runtime cost.

### Template Specialization as a Type Database

The primary template `Reflectable<T>` is undefined. Each registration macro creates a specialization that stores field metadata as a `std::tuple` of `Field` objects. SFINAE detects whether a specialization exists, providing `is_reflectable<T>()`.

Each `Field` object is an empty type—its member pointer, field type, and field name are template parameters, generating zero runtime storage. The entire metadata structure exists only in the compiler's type system.

### NTTP Strings for Compile-Time Names

C++20 non-type template parameters for literal class types allow `fixed_string<N>` to carry field names as template arguments. `Field<Sensor, double, &Sensor::temperature, "temperature">` is a unique type that encodes the name without runtime storage. Before C++20, this required either runtime strings (defeating zero-overhead) or external code generation (defeating single-macro registration).

### The Macro Front End

`FATP_REFLECT_REGISTER` uses preprocessor stringification (`#field`) to capture names, variadic counting macros to determine arity, and `MAP_N` expansion macros to apply a transformation to each field. The 500 lines of macro boilerplate—`MAP_1` through `MAP_32`—are the irreducible cost of iteration in a preprocessor with no control flow. Every macro-based reflection library has this code.

---

## Feature Inventory

### 1. Indexed Field Access

`get_field<I>(obj)` returns a reference to the I-th field. Resolved to a direct member access at compile time. Out-of-bounds indices fail at compile time via `static_assert`.

### 2. Field Names

`get_field_name<I, T>()` returns a `constexpr std::string_view`. `get_field_names<T>()` returns all names as a compile-time `std::array`. Names are NTTP strings—no runtime memory allocation.

### 3. Field Visitation

`visit_fields(obj, visitor)` calls a generic visitor for each field, passing the name and a typed reference. The fold expression over an index sequence generates one inlined call per field with no loop overhead.

### 4. Named Field Lookup

`FieldAccessor<T>::has_field(name)` and `FieldAccessor<T>::visit_field(obj, name, func)` provide runtime string-based access. Linear O(N) lookup, optimal for the 2–32 field counts typical of struct reflection.

### 5. Tuple Conversion and Debug Output

`to_tuple(obj)` returns a `std::tuple` of references for STL interop. `to_debug_string(obj)` generates a formatted representation with type name, field names, and values.

### 6. Type Introspection

`is_reflectable<T>()` detects registration via SFINAE. `type_name<T>()` extracts the compiler-generated type name from `__PRETTY_FUNCTION__` (GCC/Clang) or `__FUNCSIG__` (MSVC).

---

## Why Not Alternatives?

### Boost.PFR

Boost.PFR deduces field count and access for aggregate types automatically, with no registration. For types that qualify—simple aggregates with no constructors, no private members, no base classes—it just works. The constraint is those qualifications, and the inability to portably retrieve field names across compilers.

**When to use Boost.PFR:** Simple aggregates, no field names needed, Boost dependency acceptable.

**When to use Fat-P Reflection:** You need field names, your types have constructors, you want zero external dependencies.

### refl-cpp

A mature library with a richer metadata model: attributes, annotations, function reflection. Registration macros are more verbose because they support more metadata.

**When to use refl-cpp:** Function reflection, custom annotations, or the full metadata model.

**When to use Fat-P Reflection:** Struct field reflection with single-line registration, or you already use Fat-P.

### Code Generation

Python scripts, Jinja2 templates, or clang-based tools can generate reflection code. The generator handles anything—private members, inheritance, virtual functions. The cost is build system integration and generator maintenance.

**When to use code generation:** Complex types, private members, or an existing pipeline.

**When to use Fat-P Reflection:** You want reflection logic in the source, verified by the compiler, with no external tools.

---

## The "Forever Stuck" Reality

C++ reflection has been under discussion since SG7 was formed in 2013. The proposals have been redesigned from scratch three times: P0194, P1240, and P2996. The current proposal, P2996, introduces `std::meta::members_of(^T)` and a splice operator `[: :]`. It is the most complete and elegant design yet.

Even if adopted into C++26, compiler implementations will follow 1–2 years later. And the vast majority of production C++ runs on compilers older than the latest standard. The gap between "what the compiler knows" and "what the programmer can access" has existed for the entire 40-year history of C++.

Fat-P Reflection is designed to be replaced. The `FATP_HAS_CPP26_REFLECTION` flag is already stubbed. The consumer-facing API is generic enough that a P2996 backend could implement it without changing call sites. When native reflection ships for your toolchain, the migration is a deletion—remove the `FATP_REFLECT_REGISTER` lines.

---

## Integration Points

```
Reflection.h
    → uses: ConstexprUtilities.h (constexpr_hash64 for field name hashing)
    → uses: CppFeatureDetection.h (FATP_HAS_REFLECTION feature flag)
    → consumers: none currently (leaf component, end-user utility)
```

Reflection has no internal consumers within Fat-P. Its natural integration targets—the serialization headers and the equality framework—currently use per-type traits. A single `FATP_REFLECT_REGISTER` call could replace manual `JsonTraits<T>` and `EqualityTraits<T>` specializations. That bridge is a future opportunity.

---

## Final Assessment

Reflection delivers on the Fat-P promise:

**Permanence.** C++ has lacked type introspection for its entire history. Native reflection will not arrive before C++26, with compiler support likely in 2027–2028. Codebases targeting C++20 or earlier will never get it.

**Zero overhead.** Indexed field access compiles to a single member access instruction. Field names are template parameters. `Reflectable<T>` generates no runtime storage.

**Graceful obsolescence.** The registration macros are scaffolding. The consumer-facing API survives the P2996 transition. When native reflection ships, the migration is a deletion, not a rewrite.

For any C++20 project that needs to generically process struct fields, Reflection eliminates the maintenance surface of manual field enumeration at the cost of a single registration line per type.

---

*Reflection.h — Fat-P Library*
