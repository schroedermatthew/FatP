# The Psychopathic MATLAB Programmer
## Surviving the Interaction: A MATLAB → C++ Migration Guide

*"It's just one line in MATLAB."*

---

## Know Your Adversary

You've been assigned to migrate a critical simulation from MATLAB to C++. The original author—let's call him Dr. Matrix—has mass: 15 years of MATLAB, zero years of anything else. He wrote the code. He understands the code. And he will *defend* the code.

Dr. Matrix exhibits the following clinical symptoms:

**Indexing Dysmorphia.** He believes arrays start at 1. Not as a preference—as a *moral position*. He will argue that 0-based indexing is a "computer science fetish" that "real engineers" don't need. When you explain that C++ uses 0-based indexing, he will suggest you "just add a wrapper."

**Matrix Monotheism.** Everything is a matrix. Customer records? 47×12 matrix. State machine? 8×8 matrix. Configuration file? You guessed it—matrix. The concept of `std::map` or `struct` causes him physical discomfort.

**Temporal Denial.** He doesn't understand why code needs to compile. In his world, you type commands, things happen, you see results. The idea of a "build step" feels like bureaucracy. He will ask, repeatedly, "Can't we just run it?"

**Ownership Blindness.** Memory management doesn't exist in MATLAB. Variables appear. Variables disappear. The garbage collector handles everything. When you explain RAII, move semantics, or `std::unique_ptr`, his eyes glaze over. "Why can't it just... figure it out?"

**The One-Liner Obsession.** Dr. Matrix can solve any problem in one line. The line may be 847 characters. It may nest 12 function calls. It may be incomprehensible to anyone including Dr. Matrix three weeks later. But it is *one line*, and that makes it elegant.

```matlab
% "Elegant" MATLAB: one line, zero comprehension
result = arrayfun(@(i) sum(data(max(1,i-N):i,:) .* weights, 'all'), 1:size(data,1), 'UniformOutput', true)';
```

**Variable Name Atheism.** Variables are named `x`, `y`, `data`, `temp`, `result`, `A`, `B`, `M`, `N`, `ii`, `jj`. Comments are for people who don't understand the code. He understands the code. (He wrote it eight years ago and no longer understands it, but admitting this is not possible.)

---

## The Migration Battlefield

### Battle #1: The Indexing War

**The Conflict**

Dr. Matrix's code is riddled with 1-based indexing. Not just array access—loop bounds, offset calculations, boundary conditions. It's baked into the mathematics.

```matlab
% MATLAB: 1-based, inclusive ranges
for i = 1:N
    for j = 1:M
        result(i,j) = data(i,j) * weights(i);
    end
end
```

He will propose this solution for C++:

```cpp
// Dr. Matrix's "solution": just add 1 everywhere!
for (int i = 1; i <= N; ++i) {
    for (int j = 1; j <= M; ++j) {
        result[i][j] = data[i][j] * weights[i];  // BUFFER OVERFLOW
    }
}
```

When you explain the buffer overflow, he will suggest allocating arrays with an extra element "so the indexing works." This is not a joke. He will genuinely propose wasting memory to preserve 1-based indexing.

**Arguments That Don't Work**

- "C++ uses 0-based indexing." (He knows. He doesn't care.)
- "This is how the language works." (The language is wrong.)
- "Every C++ programmer expects 0-based." (They can adapt.)

**Arguments That Do Work**

- "The Eigen library, which is the fastest matrix library available, uses 0-based indexing. If we fight it, we lose the performance benefits." (Appeal to speed.)
- "I found three bugs in the original MATLAB code caused by off-by-one errors at boundaries. Converting to 0-based with < instead of <= eliminates this entire category of bugs." (Appeal to his existing bugs.)
- "I'll create a conversion script that mechanically transforms all indices. You won't have to think about it." (Remove his burden.)

**The Correct Migration**

```cpp
// Clean C++: 0-based, half-open ranges
for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < M; ++j) {
        result(i, j) = data(i, j) * weights[i];
    }
}
```

Or, better yet, eliminate the indices entirely:

```cpp
// Modern C++: no indices, no bugs
std::transform(data.begin(), data.end(), weights.begin(), result.begin(),
    [](const auto& row, double w) { return row * w; });
```

Dr. Matrix will hate this. It doesn't look like math. This is a feature, not a bug—code that looks like math is often wrong; code that expresses intent is verifiable.

---

### Battle #2: The Matrix Monotheism Schism

**The Conflict**

Dr. Matrix has represented a customer database as a 50,000 × 23 matrix. Column 1 is customer ID. Column 7 is account balance. Column 19 is... he thinks it's the signup date? Or maybe the last transaction? The comment says "% temporal field" which clarifies nothing.

```matlab
% MATLAB: everything is a matrix
customers = zeros(50000, 23);
customers(:, 1) = customer_ids;
customers(:, 7) = balances;
customers(:, 19) = dates;  % maybe?

% Access pattern: magic numbers everywhere
high_value = customers(customers(:,7) > 10000, :);
```

He will want to port this directly:

```cpp
// Dr. Matrix's port: Eigen matrix of mystery
Eigen::MatrixXd customers(50000, 23);
// Column 7 is... something
auto high_value = customers.row(customers.col(7).array() > 10000);  // doesn't even compile
```

**Arguments That Don't Work**

- "Structs are cleaner." (Matrices are mathematical. Structs are bureaucracy.)
- "This is hard to maintain." (He maintains it fine. He is lying.)
- "What if the schema changes?" (It won't. It will.)

**Arguments That Do Work**

- "With a struct, the compiler catches typos. `customer.balence` won't compile. `customers(:,8)` instead of `customers(:,7)` silently corrupts data. I found two instances of this in the existing code." (Show him his own bugs.)
- "Named fields let us add validation. We can enforce that balance is never negative, that dates are valid, that IDs are unique. The matrix version can't do this." (Offer capabilities he doesn't have.)
- "The struct version runs faster because we can store the fields in separate arrays (struct of arrays) and get better cache performance." (It's true, and speed wins arguments.)

**The Correct Migration**

```cpp
// Clean C++: named fields, type safety, validation
struct Customer {
    CustomerId   id;
    std::string  name;
    Money        balance;      // not double—actual money type
    Date         signup_date;
    Date         last_transaction;
    
    // Validation built in
    static Expected<Customer> create(/* params */) {
        if (balance < Money::zero()) 
            return Unexpected(Error::NegativeBalance);
        // ...
    }
};

std::vector<Customer> customers;

// Access pattern: self-documenting
auto high_value = customers | std::views::filter([](const Customer& c) {
    return c.balance > Money::from_dollars(10000);
});
```

The type system now prevents:
- Accessing the wrong column (compile error)
- Storing invalid data (validation)
- Confusing balance with ID (different types)
- Date format errors (Date type handles it)

Dr. Matrix will complain that this is "verbose." The correct response: "Yes. It's verbose like a contract is verbose. The verbosity prevents lawsuits."

---

### Battle #3: The Memory Ownership Meltdown

**The Conflict**

MATLAB has no memory management. Variables exist in a workspace. When you're done, they vanish. This is beautiful if you never think about it, and catastrophic when you try to write real systems.

Dr. Matrix's code creates temporary matrices constantly:

```matlab
% MATLAB: memory is infinite and free
function result = process_data(input)
    temp1 = preprocess(input);         % allocation
    temp2 = transform(temp1);          % allocation
    temp3 = filter(temp2);             % allocation
    temp4 = normalize(temp3);          % allocation
    result = postprocess(temp4);       % allocation
end  % garbage collector deals with it later, maybe
```

When you explain that C++ needs to manage memory, he will suggest using `new` everywhere:

```cpp
// Dr. Matrix's "solution": memory leaks as a service
Matrix* process_data(Matrix* input) {
    Matrix* temp1 = new Matrix(preprocess(input));   // leaked
    Matrix* temp2 = new Matrix(transform(temp1));    // leaked
    Matrix* temp3 = new Matrix(filter(temp2));       // leaked
    Matrix* temp4 = new Matrix(normalize(temp3));    // leaked
    return new Matrix(postprocess(temp4));           // caller's problem
}
```

When you show him the memory leak, he will suggest "just calling delete at the end." When you explain that exceptions break this, he will suggest "not using exceptions." This is the negotiation equivalent of "we've tried nothing and we're all out of ideas."

**Arguments That Don't Work**

- "This leaks memory." (MATLAB doesn't leak memory. Checkmate.)
- "We need smart pointers." (Sounds complicated. The current way works.)
- "RAII is a fundamental C++ concept." (I don't need to learn new concepts.)

**Arguments That Do Work**

- "The MATLAB runtime uses approximately 3GB of memory for this simulation. The C++ version using value semantics uses 400MB and runs 15x faster because we're not constantly allocating." (Show the benchmark. Numbers win.)
- "Every major C++ project—Eigen, TensorFlow, PyTorch's C++ backend—uses RAII and value semantics. If we use raw pointers, we can't integrate with any of them." (Ecosystem argument.)
- "I already wrote it the right way. Here's the code. It's shorter than your version." (Present the fait accompli.)

**The Correct Migration**

```cpp
// Clean C++: value semantics, zero leaks, faster
Matrix process_data(const Matrix& input) {
    auto temp1 = preprocess(input);      // value on stack
    auto temp2 = transform(temp1);       // move semantics
    auto temp3 = filter(temp2);          // no heap allocation
    auto temp4 = normalize(temp3);       // compiler optimizes
    return postprocess(temp4);           // NRVO elides copy
}

// Or, if matrices are large and you want explicit control:
Matrix process_data(const Matrix& input) {
    Matrix result = preprocess(input);
    transform_in_place(result);
    filter_in_place(result);
    normalize_in_place(result);
    postprocess_in_place(result);
    return result;  // single allocation, zero copies
}
```

The key insight for Dr. Matrix: **C++ value semantics are not "copying everywhere."** Move semantics and copy elision mean that well-written C++ passes large objects with zero overhead. The compiler is smarter than manual `new`/`delete`.

---

### Battle #4: The Dynamic Typing Delusion

**The Conflict**

MATLAB is dynamically typed. A variable can be a scalar, then a vector, then a matrix, then a struct, then a cell array. This "flexibility" is responsible for approximately 100% of silent bugs in production MATLAB code.

```matlab
% MATLAB: type? what type?
function result = compute(x)
    if isscalar(x)
        result = x * 2;
    elseif isvector(x)
        result = x .* x;
    elseif ismatrix(x)
        result = x * x';
    else
        result = x;  % ¯\_(ツ)_/¯
    end
end
```

Dr. Matrix believes this flexibility is *good*. He calls it "generic programming." (It is not generic programming. It is chaos with a smiley face.)

He will want to replicate this in C++:

```cpp
// Dr. Matrix's "solution": std::any, the type system escape hatch
std::any compute(std::any x) {
    if (x.type() == typeid(double)) {
        return std::any_cast<double>(x) * 2;
    } else if (x.type() == typeid(std::vector<double>)) {
        // this keeps going and it only gets worse
    }
}
```

**Arguments That Don't Work**

- "C++ is statically typed." (This is a limitation we should work around.)
- "std::any is slow." (Premature optimization.)
- "This defeats the point of C++." (The point is to get the code working.)

**Arguments That Do Work**

- "I found four bugs in the MATLAB code where a function received the wrong type and silently produced wrong results. The paper based on this code may have errors. With static typing, these bugs cannot exist." (Academic credibility threat.)
- "Templates give you actual generic programming—one implementation that works for any type, with zero runtime overhead. Your MATLAB approach has runtime overhead for every call." (Offer something better.)
- "The static types serve as documentation. Six months from now, when you need to modify this code, the types tell you what each function expects. The MATLAB version requires reading the entire implementation." (Appeal to future maintainability.)

**The Correct Migration**

```cpp
// Clean C++: templates for real generics
template<typename T>
T compute(const T& x) {
    return x * x;  // works for scalar, vector, matrix—all at compile time
}

// Or with concepts for constrained generics (C++20)
template<typename T>
    requires std::is_arithmetic_v<T> || is_matrix_v<T>
auto compute(const T& x) {
    if constexpr (std::is_arithmetic_v<T>) {
        return x * 2;
    } else {
        return x * x.transpose();
    }
}
```

The key insight: **Templates are compile-time polymorphism.** The compiler generates specialized code for each type. There's no runtime type checking, no branches, no overhead. It's faster than MATLAB's dynamic dispatch *and* catches type errors at compile time.

---

### Battle #5: The Vectorization Veneration

**The Conflict**

MATLAB programmers worship vectorization. Loops are sin. The goal is to express every operation as matrix operations, no matter how convoluted the result.

```matlab
% MATLAB: "elegant" vectorization
% Find indices where consecutive differences exceed threshold
idx = find(abs(diff(data)) > threshold);

% Apply moving average with edge handling
result = conv(data, ones(1,N)/N, 'same');

% Complex nested operation that nobody can read
output = bsxfun(@times, reshape(A, [m, 1, n]), permute(B, [1, 3, 2]));
```

Dr. Matrix believes this style must be preserved. He will attempt to write C++ that looks like MATLAB:

```cpp
// Dr. Matrix's port: MATLAB cosplay
auto idx = find(abs(diff(data)) > threshold);  // these functions don't exist
auto result = conv(data, ones(N)/N, "same");   // this syntax is wrong
```

When you explain these functions don't exist, he will ask "why not?" and suggest you write them.

**Arguments That Don't Work**

- "C++ doesn't have these functions." (Then add them.)
- "Loops are fine in C++." (Loops are slow. Everyone knows this.)
- "This is more readable." (Vectorized code is mathematical. Math is readable.)

**Arguments That Do Work**

- "MATLAB vectorization avoids loops because MATLAB's interpreter is slow. C++ has no interpreter—the loop compiles to the same machine code as any 'vectorized' operation. I benchmarked it." (Destroy the foundational premise.)
- "The vectorized MATLAB allocates 14 temporary arrays. The explicit C++ loop allocates zero and runs 3x faster because of cache locality." (Show the benchmark.)
- "Eigen and other libraries give you the vectorization syntax when it helps, but also let you write clear loops when that's cleaner. Best of both worlds." (Offer a compromise.)

**The Correct Migration**

```cpp
// For simple cases: Eigen gives you MATLAB-like syntax
Eigen::VectorXd result = (data.tail(N-1) - data.head(N-1)).cwiseAbs();
auto threshold_mask = (result.array() > threshold);

// For complex cases: explicit loops are fine and often faster
std::vector<size_t> indices;
for (size_t i = 1; i < data.size(); ++i) {
    if (std::abs(data[i] - data[i-1]) > threshold) {
        indices.push_back(i);
    }
}

// For moving average: standard library algorithms
std::vector<double> moving_average(const std::vector<double>& data, size_t window) {
    std::vector<double> result(data.size());
    double sum = std::accumulate(data.begin(), data.begin() + window, 0.0);
    result[window/2] = sum / window;
    
    for (size_t i = window; i < data.size(); ++i) {
        sum += data[i] - data[i - window];
        result[i - window/2] = sum / window;
    }
    return result;
}
```

The insight for Dr. Matrix: **The goal is correct, fast, maintainable code—not code that looks like MATLAB.** If MATLAB syntax helps, use Eigen. If explicit loops are clearer, use loops. The compiler doesn't care what it looks like.

---

### Battle #6: The Build System Bewilderment

**The Conflict**

MATLAB has no build system. You type commands. Things happen. The concept of compiling, linking, and managing dependencies is foreign.

Dr. Matrix will ask questions like:
- "Why do I need to compile? Can't it just run?"
- "What's a header file? Why can't I just put everything in one file?"
- "Why is there a CMakeLists.txt AND a .cpp file AND a .h file? This is too complicated."
- "I changed the code but the program is the same. Why?" (He forgot to rebuild.)

He will also resist dependencies:
- "We need Eigen for matrices." → "Can't you just write the matrix code?"
- "We need Catch2 for testing." → "I'll just test it manually."
- "We need fmt for string formatting." → "printf works fine."

**Arguments That Don't Work**

- "This is how C++ works." (MATLAB works better.)
- "Compilation catches errors." (I don't make errors.)
- "Dependencies save us months of work." (I can write it myself in a weekend.)

**Arguments That Do Work**

- "Compilation lets us catch errors before running. The MATLAB code failed at runtime after 3 hours of computation because of a typo. The C++ version won't start if there's a typo." (Painful personal experience.)
- "Eigen is written by professionals who've optimized it for 15 years. We get AVX2 vectorization, cache-aware blocking, and parallel execution for free. Writing this ourselves would take two years and be 10x slower." (Respect for expertise.)
- "I'll set up the build system and IDE. You'll press one button to build and run, just like MATLAB. You won't need to understand CMake." (Remove the obstacle.)

**The Setup You'll Actually Use**

```cmake
# CMakeLists.txt: hide this from Dr. Matrix
cmake_minimum_required(VERSION 3.16)
project(simulation CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Eigen3 REQUIRED)
find_package(Catch2 3 REQUIRED)

add_executable(simulation
    main.cpp
    physics.cpp
    solver.cpp
)

target_link_libraries(simulation PRIVATE Eigen3::Eigen)
```

Configure the IDE so that F5 means "build and run." Dr. Matrix never needs to know what happens underneath.

---

## Technical Reference: MATLAB → C++ Patterns

### Data Type Mappings

| MATLAB | C++ | Notes |
|--------|-----|-------|
| `double` scalar | `double` | Identical |
| `double` array | `Eigen::VectorXd` | Or `std::vector<double>` |
| `double` matrix | `Eigen::MatrixXd` | Column-major like MATLAB |
| `single` | `float` | Identical |
| `int32` | `int32_t` | Use fixed-width types |
| `logical` | `bool` | Or `Eigen::Array<bool>` |
| `char` array | `std::string` | Or `std::string_view` |
| `cell` array | `std::vector<std::variant<...>>` | Avoid if possible |
| `struct` | `struct` / `class` | Use proper C++ structs |
| `struct` array | `std::vector<Struct>` | Much cleaner |
| `containers.Map` | `std::unordered_map` | Faster than MATLAB |
| `function_handle` | `std::function` | Or template parameter |

### Common Function Mappings

```cpp
// MATLAB: zeros(m, n)
Eigen::MatrixXd::Zero(m, n);

// MATLAB: ones(m, n)
Eigen::MatrixXd::Ones(m, n);

// MATLAB: eye(n)
Eigen::MatrixXd::Identity(n, n);

// MATLAB: rand(m, n)
Eigen::MatrixXd::Random(m, n);  // range [-1, 1], not [0, 1]!

// MATLAB: size(A)
A.rows(), A.cols()

// MATLAB: length(v)
v.size()

// MATLAB: A'
A.transpose()

// MATLAB: A.'
A.transpose()  // Eigen doesn't distinguish conjugate transpose for real matrices

// MATLAB: A * B
A * B  // matrix multiplication

// MATLAB: A .* B
A.cwiseProduct(B)  // or A.array() * B.array()

// MATLAB: A ./ B
A.cwiseQuotient(B)

// MATLAB: sum(A)
A.sum()

// MATLAB: mean(A)
A.mean()

// MATLAB: max(A)
A.maxCoeff()

// MATLAB: [A; B]  (vertical concatenation)
Eigen::MatrixXd C(A.rows() + B.rows(), A.cols());
C << A, B;

// MATLAB: [A, B]  (horizontal concatenation)
Eigen::MatrixXd C(A.rows(), A.cols() + B.cols());
C << A, B;

// MATLAB: A(1:10, :)
A.topRows(10)

// MATLAB: A(:, 1:10)
A.leftCols(10)

// MATLAB: A(2:end, :)
A.bottomRows(A.rows() - 1)

// MATLAB: diag(v)
v.asDiagonal()

// MATLAB: inv(A)
A.inverse()  // prefer A.lu().solve(B) for A\B

// MATLAB: A \ B
A.lu().solve(B)  // or other decompositions

// MATLAB: eig(A)
Eigen::EigenSolver<Eigen::MatrixXd> es(A);
es.eigenvalues();
es.eigenvectors();

// MATLAB: svd(A)
Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeFullU | Eigen::ComputeFullV);
svd.singularValues();
svd.matrixU();
svd.matrixV();

// MATLAB: fft(x)
// Use Eigen's unsupported FFT module or FFTW

// MATLAB: interp1(x, y, xi)
// No direct equivalent; use a spline library or write your own

// MATLAB: find(condition)
std::vector<size_t> indices;
for (size_t i = 0; i < data.size(); ++i) {
    if (condition(data[i])) indices.push_back(i);
}

// MATLAB: arrayfun(@f, A)
std::transform(A.begin(), A.end(), result.begin(), f);
// or with Eigen:
A.unaryExpr([](double x) { return f(x); });
```

### Control Flow Mappings

```cpp
// MATLAB: for i = 1:N
for (size_t i = 0; i < N; ++i)  // Note: 0-based, exclusive upper bound

// MATLAB: for i = N:-1:1
for (size_t i = N; i-- > 0; )  // Careful with unsigned reverse loops

// MATLAB: while condition
while (condition)

// MATLAB: if/elseif/else
if (cond1) {
    // ...
} else if (cond2) {
    // ...
} else {
    // ...
}

// MATLAB: switch/case
switch (value) {
    case 1: /* ... */ break;
    case 2: /* ... */ break;
    default: /* ... */ break;
}

// MATLAB: try/catch
try {
    // ...
} catch (const std::exception& e) {
    // ...
}

// MATLAB: return (early return)
return result;  // Identical

// MATLAB: break/continue
break; continue;  // Identical
```

### Storage Order: The Hidden Bug

MATLAB stores matrices in **column-major** order. Eigen defaults to **column-major** to match. But C-style 2D arrays are **row-major**.

```cpp
// Column-major (MATLAB, Eigen default, Fortran)
// Elements stored: A(0,0), A(1,0), A(2,0), A(0,1), A(1,1), A(2,1), ...
Eigen::MatrixXd A(3, 3);  // column-major

// Row-major (C arrays, numpy default)
// Elements stored: A[0][0], A[0][1], A[0][2], A[1][0], A[1][1], A[1][2], ...
Eigen::Matrix<double, 3, 3, Eigen::RowMajor> B;  // row-major

// This matters when:
// 1. Interfacing with C code
// 2. Reading binary files
// 3. Optimizing cache access patterns
```

If Dr. Matrix wrote binary files from MATLAB, you need to read them column-major in C++ or transpose after loading.

---

## Psychological Survival Strategies

### Strategy #1: The Fait Accompli

Don't ask permission. Don't propose designs. Just write the code correctly, then show it working. Dr. Matrix can't argue with running code that produces the same results faster.

```
❌ "I think we should use structs instead of matrices for the customer data."
   → Opens negotiation. You will lose.

✅ "Here's the new customer module. It's 3x faster and caught two bugs in the 
    original MATLAB. The tests pass. Can you verify the outputs match?"
   → Presents completed work. Verification is easier than design debate.
```

### Strategy #2: The Benchmark Bludgeon

Dr. Matrix respects speed. MATLAB programmers profile obsessively. Use this.

Every time you make a design decision he questions, produce a benchmark. "Your way takes 340ms. My way takes 12ms. Here's the code. You can run it yourself."

He may still argue. But he'll argue less.

### Strategy #3: The Bug Archaeology

Before you migrate anything, run the MATLAB code under every edge case you can imagine. Document the bugs. Screenshot the wrong outputs.

Then, when Dr. Matrix resists a change: "This change prevents the bug on slide 17. The bug that caused the paper revision. The bug that cost us three months."

Past pain is a powerful argument.

### Strategy #4: The Incremental Envelopment

Don't try to convert everything at once. Start with one module—preferably the slowest one. Make it work. Make it fast. Get Dr. Matrix to admit, grudgingly, that it's better.

Then convert the next module. And the next. Each success makes the next conversion easier.

By the time he realizes the whole codebase is C++, it's too late to object.

### Strategy #5: The Alibi of Authority

Dr. Matrix dismisses your opinions. You're "just" a software engineer. But he respects other academics, famous libraries, and Google.

- "Eigen does it this way."
- "The Google C++ style guide recommends..."
- "Stroustrup's book says..."
- "This paper from MIT shows..."

These arguments shouldn't be more valid than your own expertise. But they are. Use them.

---

## The Exit Strategy

Sometimes, despite your best efforts, the migration fails. Dr. Matrix refuses to cooperate. Management won't force the issue. The MATLAB code shambles on, accumulating technical debt.

In this case, your options are:

1. **Document Everything.** Write a migration guide. Note all the bugs you found. Record the performance comparisons. When the code eventually fails catastrophically—and it will—the documentation proves you tried.

2. **Isolate the Cancer.** If you can't convert the MATLAB code, at least wrap it. Create a C++ interface that calls MATLAB via the Engine API or compiled MATLAB libraries. The wrapper is ugly. It's slow. But it contains the damage.

3. **Wait for Retirement.** Dr. Matrix is 58. The MATLAB code has no other maintainers. In seven years, you inherit it by default. Plan your rewrite now.

4. **Leave.** Sometimes the correct career move is to work somewhere that values software engineering. Your skills have market value. Use them.

---

## Appendix: Quick Reference Card

### The Mindset Shift

| MATLAB Thinking | C++ Thinking |
|-----------------|--------------|
| Everything is a matrix | Use the right data structure |
| Memory is infinite | Ownership is explicit |
| Variables are dynamic | Types are static |
| Loops are slow | Loops compile to fast code |
| One-liners are elegant | Readable code is elegant |
| Runtime errors are normal | Compile-time errors are better |
| Workspace is state | Objects encapsulate state |
| Scripts are programs | Programs are structured |

### The Conversion Checklist

- [ ] Indices: 1-based → 0-based
- [ ] Ranges: inclusive → half-open (< not <=)
- [ ] Matrices → structs where appropriate
- [ ] Magic numbers → named constants
- [ ] Comments added to uncommented code
- [ ] Variable names → meaningful names
- [ ] Global state → encapsulated objects
- [ ] Manual memory → RAII / value semantics
- [ ] Runtime type checks → compile-time types
- [ ] tests written before conversion
- [ ] Benchmarks to prove correctness and speed

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "This code is garbage." | "There are some opportunities for improvement." |
| "This will never work." | "Let me prototype an alternative approach." |
| "You don't understand C++." | "C++ has different idioms than MATLAB." |
| "This is obviously wrong." | "I'd like to understand the reasoning here." |
| "I found 47 bugs." | "I found some edge cases we should discuss." |
| "Please just let me do my job." | "I'll handle the implementation details." |

---

*Remember: the goal is shipping working software, not winning arguments. Pick your battles. Win the war.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 73%  
**Survival probability without:** 12%
