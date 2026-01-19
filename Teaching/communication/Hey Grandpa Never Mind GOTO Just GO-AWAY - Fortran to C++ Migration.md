# Hey Grandpa, Never Mind GOTO, Just GO-AWAY
## A Fortran → C++ Migration Guide for the Battle-Weary

*"We put men on the moon with this code."*

---

## Know Your Adversary

You've inherited a codebase. It simulates fluid dynamics, or neutron transport, or atmospheric modeling. It's been running since 1987. It works. And it's written in Fortran—not modern Fortran 2018, but FORTRAN 77 with "extensions" added over decades by people who are now retired or dead.

The last surviving original author—let's call him Dr. Punchcard—is 67 years old. He wrote the core algorithms during the Reagan administration. He has mass: 40 years of Fortran, mass: distrust of anything invented after 1995. He's not hostile, exactly. He's just *tired*—tired of young programmers who don't understand that the code works, has always worked, and will continue to work as long as nobody touches it.

Dr. Punchcard exhibits the following clinical symptoms:

**Nostalgic Immunity.** Every criticism of the code is met with historical context. "This COMMON block has been stable for 30 years." "This GOTO structure matched the original paper." "We validated this against experimental data in 1991." The implication: if it worked then, it works now, and your objections are ignorance disguised as progress.

**Modification Anxiety.** Dr. Punchcard has seen rewrites before. All of them failed. The 1994 C rewrite: abandoned. The 2003 Java port: a disaster. The 2011 Python prototype: too slow by a factor of 1000. He's not being stubborn; he's being *empirical*. Rewrites fail. This is observed fact.

**Implicit Intuition.** He knows that variables starting with I through N are integers. He doesn't just know it—it's wired into his brain at a level below conscious thought. When he sees `DELTA`, he knows it's real. When he sees `INDEX`, he knows it's integer. The idea of *declaring* types feels redundant, like labeling your shoes "left" and "right."

**Column Consciousness.** His mind still formats code in 72-column cards. Columns 1-5 are for labels. Column 6 is for continuation. Columns 7-72 are for code. Columns 73-80 are for sequence numbers (ignored). Free-form code looks *wrong* to him, like poetry without meter.

**The GOTO Gambit.** When you mention GOTO, he sighs. He's heard this before. "GOTO is fine if you use it correctly." He will then explain structured programming, Dijkstra's letter, and why the criticism is overblown. This is a trap. GOTO is not the real problem. GOTO is the problem you can see. The real problems are invisible.

**Heroic Self-Image.** This code ran on machines with 64KB of memory. It was optimized for punchcard readers and magnetic tape. It survived the transition from mainframes to minicomputers to workstations to PCs to clusters. The code isn't legacy—it's a *survivor*. Treating it as technical debt is disrespectful.

---

## The Horrors That Await You

### Horror #1: IMPLICIT Typing

**The Atrocity**

Fortran doesn't require variable declarations. By default, variables starting with I, J, K, L, M, N are integers. Everything else is REAL. This is called IMPLICIT typing, and it has caused more silent bugs than any other feature in programming history.

```fortran
C     FORTRAN 77: IMPLICIT typing in action
      IMPLICIT REAL*8 (A-H, O-Z)
      
      SUBROUTINE CALC(X, Y, RESULT)
      DELTA = X - Y
      INDEX = 1
      TOTAL = 0.0D0
      DO 10 I = 1, 100
         TOTAL = TOTAL + DELTA * I
   10 CONTINUE
      RESULT = TOTAL
      END
```

Notice: no declarations. `DELTA`, `TOTAL`, `RESULT`, `X`, `Y` are all REAL*8 because they don't start with I-N. `INDEX`, `I` are integers because they do. This is "obvious" to Dr. Punchcard. It's a landmine for everyone else.

The bug that will kill you:

```fortran
C     Spot the bug
      D0 10 I = 1, 100
         TOTAL = TOTAL + X(I)
   10 CONTINUE
```

There is no bug, you say? Look closer. That's `D0`, not `DO`. The number zero, not the letter O. This creates a variable named `D010I` and assigns it the value `1.100`. The loop never executes. This bug crashed a NASA rocket. (Mariner 1, 1962. Look it up.)

**Arguments That Don't Work**

- "IMPLICIT NONE should be standard." (It's extra typing for no benefit.)
- "This allows typos to become variables." (I don't make typos.)
- "Modern Fortran requires declarations." (This isn't modern Fortran, and it works.)

**Arguments That Do Work**

- "I found 7 variables that are used but never initialized. With explicit declarations, the compiler would have caught these. Here's the list." (Show the bugs.)
- "When I add IMPLICIT NONE, the code won't compile until I fix 23 undeclared variables—some of which are misspelled versions of other variables. This isn't new bugs; it's old bugs the compiler can finally see." (Reveal the existing rot.)
- "The Department of Defense banned IMPLICIT typing in 1995 for safety-critical systems. Our sponsor follows DoD standards." (External authority.)

**The Correct Migration**

```cpp
// C++: every variable declared, no ambiguity
void calc(double x, double y, double& result) {
    const double delta = x - y;
    double total = 0.0;
    
    for (int i = 0; i < 100; ++i) {
        total += delta * (i + 1);  // Note: Fortran was 1-indexed
    }
    
    result = total;
}
```

The C++ version:
- Declares every variable with its type
- Fails to compile if you misspell a variable name
- Makes the integer/float distinction visible
- Uses `const` to indicate values that don't change

Dr. Punchcard will complain about "verbosity." The correct response: "Verbosity is insurance. The compiler reads declarations; humans read them too. The declarations are documentation that can't go stale."

---

### Horror #2: COMMON Blocks

**The Atrocity**

Fortran has no modules, no namespaces, no objects. So how do subroutines share data? COMMON blocks—named regions of memory that any subroutine can access if it declares the same COMMON block.

```fortran
C     File: physics.f
      SUBROUTINE PHYSICS
      COMMON /PARAMS/ DT, DX, GRAVITY, VISCOSITY
      COMMON /STATE/ X(10000), V(10000), A(10000)
C     ... use the variables ...
      END

C     File: output.f
      SUBROUTINE OUTPUT
      COMMON /PARAMS/ TIMESTEP, GRIDSIZE, G, NU
      COMMON /STATE/ POSITION(10000), VELOCITY(10000), ACCEL(10000)
C     ... same memory, different names ...
      END
```

Did you notice? The COMMON block `/PARAMS/` has different variable names in different files. That's legal. The memory layout is the same; the names are local. `DT` and `TIMESTEP` are the same variable. `GRAVITY` and `G` are the same variable. This is called "sequence association," and it's how Fortran achieves global state without global variables.

The bug that will kill you:

```fortran
C     File: physics.f - the original
      COMMON /PARAMS/ DT, DX, GRAVITY, VISCOSITY

C     File: newmodule.f - added by a grad student in 2019
      COMMON /PARAMS/ DT, DX, GRAVITY
C     Oops—only 3 variables, not 4. VISCOSITY is now garbage.
```

Every subroutine that uses `/PARAMS/` must declare it *identically*. If one file is wrong, you get silent memory corruption. The compiler can't check this—COMMON blocks are resolved at link time.

Some codebases have dozens of COMMON blocks, each used in dozens of files. Changing any of them requires updating every file. Missing one corrupts memory silently.

**Arguments That Don't Work**

- "COMMON blocks are global state, which is bad." (Global state is fine if you're careful.)
- "This is hard to maintain." (I maintain it fine.)
- "Modules would be better." (We tried modules in 2008. It broke everything.)

**Arguments That Do Work**

- "I found 3 files where COMMON block /STATE/ has different sizes. The code is silently corrupting memory. With C++ structs, the compiler would catch this." (Show the corruption.)
- "COMMON blocks prevent multithreading—every thread would share the same global state. The new cluster requires thread-level parallelism." (Modernization requirement.)
- "I'll write a script that extracts all COMMON blocks into a single header file. Each file includes the header. Changes happen in one place. This is safer than the current system." (Incremental improvement.)

**The Correct Migration**

```cpp
// C++: explicit shared state, type-safe, thread-aware

// params.h - single source of truth
struct SimulationParams {
    double dt       = 0.001;
    double dx       = 0.1;
    double gravity  = 9.81;
    double viscosity = 1.0e-6;
};

struct SimulationState {
    std::vector<double> x;
    std::vector<double> v;
    std::vector<double> a;
    
    explicit SimulationState(size_t n) 
        : x(n), v(n), a(n) {}
};

// physics.cpp
void physics(const SimulationParams& params, SimulationState& state) {
    // params and state explicitly passed
    // No hidden global dependencies
}

// output.cpp  
void output(const SimulationParams& params, const SimulationState& state) {
    // Same structs, same names, compiler-verified
}
```

The C++ version:
- Single definition of shared data structures
- Compiler verifies every access
- Explicit parameter passing shows data flow
- Thread-safe: each thread can have its own state
- Default values documented in one place

---

### Horror #3: The GOTO Labyrinth

**The Atrocity**

Yes, GOTO. You knew it was coming. But the reality is worse than you imagine, because Fortran GOTO comes in flavors:

```fortran
C     Unconditional GOTO - the one everyone hates
      GOTO 100

C     Computed GOTO - runtime switch based on integer value  
      GOTO (100, 200, 300), INDEX
C     If INDEX=1, go to 100. If INDEX=2, go to 200. Etc.

C     Assigned GOTO - the label is a variable
      ASSIGN 100 TO LABEL
      GOTO LABEL

C     Arithmetic IF - three-way branch based on sign
      IF (X) 100, 200, 300
C     If X<0, go to 100. If X=0, go to 200. If X>0, go to 300.
```

Combine these with deeply nested loops, and you get code that reads like a choose-your-own-adventure book written by a sadist:

```fortran
      SUBROUTINE SOLVER(N, A, B, X, IERR)
      DIMENSION A(N,N), B(N), X(N)
      IERR = 0
      DO 100 I = 1, N
         IF (A(I,I)) 10, 20, 30
   10    IF (ABS(A(I,I)) .LT. 1.0D-10) GOTO 200
         GOTO 40
   20    DO 25 J = I+1, N
            IF (A(J,I) .NE. 0.0D0) GOTO 26
   25    CONTINUE
         GOTO 200
   26    DO 27 K = 1, N
            TEMP = A(I,K)
            A(I,K) = A(J,K)
            A(J,K) = TEMP
   27    CONTINUE
         TEMP = B(I)
         B(I) = B(J)
         B(J) = TEMP
   30    CONTINUE
   40    DO 50 J = I+1, N
C        ... 200 more lines of this ...
  100 CONTINUE
      RETURN
  200 IERR = 1
      RETURN
      END
```

The control flow is not visible from the code structure. The only way to understand it is to trace every GOTO by hand. This is why Dijkstra wrote "Go To Statement Considered Harmful"—not because GOTO is inherently evil, but because it destroys local reasoning.

**Arguments That Don't Work**

- "Dijkstra said GOTO is harmful." (Dijkstra never wrote production code.)
- "This is spaghetti code." (It follows the algorithm in the paper exactly.)
- "No one can understand this." (I understand it. The problem is you.)

**Arguments That Do Work**

- "I need to add error handling for the new boundary conditions. With the current GOTO structure, I can't determine which cleanup code runs on which error path. Can you walk me through it?" (Make him confront the complexity.)
- "The original 1987 version was 400 lines. It's now 2,300 lines. Each modification added more GOTOs because that was easier than restructuring. The growth is unsustainable." (Historical trajectory argument.)
- "I've restructured one subroutine as an experiment. Same algorithm, same results, but now I can add the new feature in 20 lines instead of 200. Can we review it together?" (Demonstrate the benefit.)

**The Correct Migration**

```cpp
// C++: structured control flow, same algorithm

enum class SolverError {
    None,
    SingularMatrix,
    NumericalInstability
};

SolverError solve(Matrix& A, Vector& b, Vector& x) {
    const size_t n = A.rows();
    
    for (size_t i = 0; i < n; ++i) {
        // Handle zero pivot
        if (std::abs(A(i,i)) < 1.0e-10) {
            // Find non-zero pivot in column
            auto pivot_row = find_nonzero_pivot(A, i);
            if (!pivot_row) {
                return SolverError::SingularMatrix;
            }
            swap_rows(A, b, i, *pivot_row);
        }
        
        // Eliminate column
        for (size_t j = i + 1; j < n; ++j) {
            eliminate_row(A, b, i, j);
        }
    }
    
    // Back substitution
    back_substitute(A, b, x);
    
    return SolverError::None;
}

// Helper functions make the algorithm visible
std::optional<size_t> find_nonzero_pivot(const Matrix& A, size_t col) {
    for (size_t j = col + 1; j < A.rows(); ++j) {
        if (A(j, col) != 0.0) {
            return j;
        }
    }
    return std::nullopt;
}
```

The transformation rules:
- `GOTO error_label` → `return ErrorCode`
- Computed GOTO → `switch` statement
- Arithmetic IF → `if`/`else if`/`else`
- Loop with GOTO exit → `break` or early `return`
- "GOTO into a loop" → This is evil. Refactor completely.

---

### Horror #4: EQUIVALENCE and Memory Aliasing

**The Atrocity**

EQUIVALENCE tells the compiler to store two variables in the same memory location. It's type punning before type punning had a name.

```fortran
C     EQUIVALENCE: two names, one memory location
      REAL*8 X(1000)
      INTEGER*4 IX(2000)
      EQUIVALENCE (X, IX)
C     X and IX overlap in memory. X(1) shares bytes with IX(1) and IX(2).
      
C     Used for "efficient" memory reuse:
      REAL*8 TEMP1(10000), TEMP2(10000)
      EQUIVALENCE (TEMP1, TEMP2)
C     Only allocate memory once, reuse for different phases
```

Why would anyone do this? Because in 1977, memory was precious. If subroutine A needs a 10,000-element scratch array, and subroutine B needs a 10,000-element scratch array, and they never run at the same time, why allocate 20,000 elements? EQUIVALENCE them.

The bug that will kill you:

```fortran
C     Legacy code: aliased scratch arrays
      REAL*8 SCRATCH1(10000), SCRATCH2(10000)  
      EQUIVALENCE (SCRATCH1, SCRATCH2)
      
C     Phase 1: uses SCRATCH1
      CALL PHASE1(SCRATCH1)  
      
C     Phase 2: uses SCRATCH2 (same memory!)
      CALL PHASE2(SCRATCH2, SCRATCH1)  
C     Oops—PHASE2 reads SCRATCH1, but PHASE1's data is gone
```

EQUIVALENCE also enables this horror:

```fortran
C     Type punning: access REAL*8 as INTEGER*4 array
      REAL*8 X
      INTEGER*4 IX(2)
      EQUIVALENCE (X, IX)
      
      X = 3.14159D0
      PRINT *, IX(1), IX(2)
C     Prints the raw bit representation of the double
```

This was used for "efficient" serialization, bit manipulation, and manual IEEE-754 inspection. It violates every aliasing rule in modern compilers.

**Arguments That Don't Work**

- "This is undefined behavior." (It works. It's always worked.)
- "Modern compilers assume no aliasing." (So use the right compiler flags.)
- "This is unmaintainable." (I maintain it fine.)

**Arguments That Do Work**

- "The new compiler's optimizer assumes strict aliasing. With EQUIVALENCE, it generates wrong code—I have a test case. We either disable optimization (40% slower) or remove EQUIVALENCE." (Compiler forces the issue.)
- "I need to add OpenMP parallelization. Aliased arrays break the parallel analysis—threads would corrupt each other's data. The EQUIVALENCE must go." (Modernization requirement.)
- "Memory is cheap now. The entire scratch space is 800KB. My laptop has 32GB of RAM. The 'optimization' from 1985 is not worth the bugs it causes." (Economics have changed.)

**The Correct Migration**

```cpp
// C++: explicit memory management, no aliasing

class Solver {
private:
    std::vector<double> scratch1_;
    std::vector<double> scratch2_;
    // Separate allocations. Memory is cheap. Correctness is priceless.
    
public:
    explicit Solver(size_t n) 
        : scratch1_(n)
        , scratch2_(n)
    {}
    
    void solve() {
        phase1(scratch1_);
        phase2(scratch2_, scratch1_);  // No aliasing, no bugs
    }
};

// For genuine type punning needs (rare), use std::bit_cast (C++20)
double x = 3.14159;
auto bits = std::bit_cast<uint64_t>(x);  // Explicit, safe, documented
```

---

### Horror #5: Fixed-Form Formatting

**The Atrocity**

Fortran 77 uses "fixed-form" source format, a relic of punched cards:

```
Columns 1-5:   Statement label (optional)
Column 6:      Continuation character (any non-blank character)
Columns 7-72:  Statement text
Columns 73-80: Sequence number (ignored by compiler)
```

This means:

```fortran
C23456789012345678901234567890123456789012345678901234567890123456789012
C     ^ Column 7 starts here                                    ^ Column 72
      SUBROUTINE CALCULATE_FLUID_DYNAMICS_PROPERTIES(DENSITY, VI
     +SCOSITY, PRESSURE, TEMPERATURE, VELOCITY_X, VELOCITY_Y, VEL
     +OCITY_Z, RESULT_ARRAY, ERROR_CODE)
```

The `+` in column 6 means "this line continues the previous line." The statement is split across three lines because it's too long for 66 characters.

Worse, column 6 can be *any* non-blank, non-zero character:

```fortran
      X = A + B + C + D + E +
     1    F + G + H + I + J +
     2    K + L + M + N + O +
     $    P + Q + R + S + T
C     All valid continuation characters: 1, 2, $
```

And comments must have C, c, or * in column 1:

```fortran
C     This is a comment
c     So is this
*     And this
      X = 1.0   This is NOT a comment - it's a syntax error!
```

**The Horrors This Causes**

1. **Invisible bugs from tabs:** A tab character advances to the next tab stop, which might put code in the wrong column. `<tab>X = 1.0` might put the X in column 9 or column 1 depending on tab settings, completely changing meaning.

2. **Invisible bugs from trailing spaces:** Fortran ignores spaces within statements. `DO 10 I = 1, 10` and `DO10I=1,10` are identical. `GO TO 100` and `GOTO100` are identical.

3. **Line length bugs:** If code extends past column 72, it's silently ignored. Your carefully written statement just... stops.

```fortran
C     Can you spot the bug?
      IMPORTANT_RESULT = COEFFICIENT_A * VARIABLE_X + COEFFICIENT_B * VAR
      IABLE_Y
C     Columns 73-80 are ignored. This assigns COEFFICIENT_A * VARIABLE_X + COEFFICIENT_B * VAR.
C     The next line creates a new variable IABLE_Y with no assignment.
```

**The Migration**

C++ has none of these problems. But you need to be careful when translating:

```cpp
// Watch for truncated lines in the original
// This Fortran:
//       RESULT = A_VERY_LONG_EXPRESSION + ANOTHER_LONG_TERM + YET_ANOTH
//       ER_TERM + FINAL_TERM
// 
// Is actually:
//       RESULT = A_VERY_LONG_EXPRESSION + ANOTHER_LONG_TERM + YET_ANOTH
// Plus a fragment on the next line that doesn't compile.
// 
// When migrating, VERIFY the Fortran compiles. Then translate.

double result = a_very_long_expression 
              + another_long_term 
              + yet_another_term 
              + final_term;  // C++ doesn't care about line length
```

---

### Horror #6: Assumed-Size and Assumed-Shape Arrays

**The Atrocity**

Fortran arrays can be passed without their size:

```fortran
C     Assumed-size array: last dimension is *
      SUBROUTINE PROCESS(A, N)
      REAL*8 A(*)
      INTEGER N
C     A is an array of unknown size. N is the actual size. We hope.
      DO 10 I = 1, N
         A(I) = A(I) * 2.0D0
   10 CONTINUE
      END
```

The subroutine trusts the caller to provide the correct size. There's no bounds checking. There's no way to query the actual size. If N is wrong, you corrupt memory silently.

Multi-dimensional arrays are worse:

```fortran
C     2D array with assumed first dimension
      SUBROUTINE MATMUL(A, B, C, M, N, K)
      REAL*8 A(M, *), B(N, *), C(M, *)
C     The second dimension is unknown for all three matrices.
C     We're trusting M, N, K are correct. They probably aren't.
```

And then there's the dimension mismatch bug:

```fortran
C     Main program
      REAL*8 X(100, 50)
      CALL PROCESS_MATRIX(X, 100, 50)
      
C     Subroutine
      SUBROUTINE PROCESS_MATRIX(A, N, M)
      REAL*8 A(N, M)
C     Wait—is it (100, 50) or (50, 100)?
C     If the caller and callee disagree, silent corruption.
```

**The Correct Migration**

```cpp
// C++: std::span or templates for safe array passing

// Modern approach: span carries size with data
void process(std::span<double> a) {
    for (auto& val : a) {
        val *= 2.0;
    }
}

// Template approach: size known at compile time for small fixed arrays
template<size_t N>
void process(std::array<double, N>& a) {
    for (auto& val : a) {
        val *= 2.0;
    }
}

// Matrix class: dimensions embedded in type or stored with data
class Matrix {
public:
    size_t rows() const { return rows_; }
    size_t cols() const { return cols_; }
    
    double& operator()(size_t i, size_t j) {
        assert(i < rows_ && j < cols_);  // Bounds checking in debug
        return data_[i * cols_ + j];
    }
    
private:
    size_t rows_, cols_;
    std::vector<double> data_;
};

// Impossible to pass wrong dimensions—the matrix knows its own size
void matmul(const Matrix& A, const Matrix& B, Matrix& C) {
    assert(A.cols() == B.rows());  // Dimension check
    assert(C.rows() == A.rows() && C.cols() == B.cols());
    // ...
}
```

---

### Horror #7: No Recursion (FORTRAN 77)

**The Atrocity**

FORTRAN 77 does not support recursion. Subroutines cannot call themselves. This isn't a guideline—it's a fundamental limitation. The compiler allocates local variables statically; there's no stack frame, so recursion is impossible.

```fortran
C     This is ILLEGAL in FORTRAN 77
      INTEGER FUNCTION FACTORIAL(N)
      IF (N .LE. 1) THEN
         FACTORIAL = 1
      ELSE
         FACTORIAL = N * FACTORIAL(N-1)  ! COMPILER ERROR
      ENDIF
      END
```

The workaround? Manual stack management:

```fortran
C     "Recursive" factorial via manual stack
      INTEGER FUNCTION FACTORIAL(N)
      INTEGER STACK(100), SP, CURRENT, RESULT
      SP = 0
      CURRENT = N
      
C     Push phase
   10 IF (CURRENT .LE. 1) GOTO 20
      SP = SP + 1
      STACK(SP) = CURRENT
      CURRENT = CURRENT - 1
      GOTO 10
      
C     Pop phase
   20 RESULT = 1
   30 IF (SP .EQ. 0) GOTO 40
      RESULT = RESULT * STACK(SP)
      SP = SP - 1
      GOTO 30
      
   40 FACTORIAL = RESULT
      END
```

Dr. Punchcard has been writing iterative versions of recursive algorithms for 40 years. He doesn't see this as a limitation—he sees it as discipline.

**Arguments That Don't Work**

- "Recursion is more natural for this algorithm." (Everything can be written iteratively.)
- "The recursive version is clearer." (The iterative version is faster. We profiled it in 1989.)
- "Modern Fortran supports recursion." (We're not using modern Fortran.)

**Arguments That Do Work**

- "The tree traversal we need for the new adaptive mesh has depth proportional to log(N). Your manual stack version has a hard-coded limit of 100 levels. With N > 2^100 nodes (more atoms than in the universe), your stack overflows." (Show the limitation matters.)
- "I timed both versions. The recursive C++ with tail-call optimization is identical to the iterative Fortran. Modern compilers eliminate the 'overhead' you're avoiding." (Benchmark.)
- "For genuinely deep recursion, I can use explicit std::stack in C++ too. But the compiler checks the implementation. Your manual stack is GOTO spaghetti that nobody can verify." (Same technique, safer implementation.)

**The Correct Migration**

```cpp
// C++: natural recursion with tail-call optimization

// Simple recursive version (compiler optimizes)
int factorial(int n) {
    return (n <= 1) ? 1 : n * factorial(n - 1);
}

// Explicit tail-recursive version if you care
int factorial_impl(int n, int acc) {
    return (n <= 1) ? acc : factorial_impl(n - 1, n * acc);
}
int factorial(int n) {
    return factorial_impl(n, 1);
}

// For algorithms that actually need deep recursion, 
// std::stack is explicit and bounds-checked
int factorial_iterative(int n) {
    std::stack<int> frames;
    while (n > 1) {
        frames.push(n--);
    }
    int result = 1;
    while (!frames.empty()) {
        result *= frames.top();
        frames.pop();
    }
    return result;
}
```

---

### Horror #8: Hollerith Constants and CHARACTER Misery

**The Atrocity**

Before Fortran 77 introduced CHARACTER types, strings were stored as Hollerith constants—counted sequences of characters stuffed into numeric variables:

```fortran
C     Hollerith constant: 5 characters stored in an integer
      DATA LABEL /5HHELLO/
C     The integer LABEL contains the bytes 'H', 'E', 'L', 'L', 'O'
      
C     Passing Hollerith to subroutines
      CALL PRINTMSG(12HWARNING: BAD)
```

The number before the H is the character count. Miscount it, and you read garbage or corrupt memory.

Fortran 77's CHARACTER type is better, but still painful:

```fortran
C     Fixed-length strings
      CHARACTER*20 NAME
      CHARACTER*80 LINE
      CHARACTER*(*) FLEXIBLE
      
      NAME = 'SHORT'
C     NAME is now 'SHORT               ' (padded with spaces)
      
      IF (NAME .EQ. 'SHORT') THEN
C         This is FALSE! NAME contains trailing spaces.
      ENDIF
      
C     String operations require knowing lengths
      NAME = FIRSTNAME(1:10) // ' ' // LASTNAME(1:9)
C     Manual substring and concatenation
```

Comparisons are haunted by trailing spaces. Concatenation requires manual length tracking. Null terminators don't exist. Dynamic strings don't exist (in FORTRAN 77).

**The Correct Migration**

```cpp
// C++: std::string handles everything

std::string name = "SHORT";  // No trailing spaces

if (name == "SHORT") {  // True, as expected
    // ...
}

// Concatenation just works
std::string full_name = first_name + " " + last_name;

// Dynamic sizing
name += " EXTENDED";  // Grows automatically

// For interfacing with Fortran code that expects fixed-length CHARACTER:
void call_fortran_with_string(const std::string& s, size_t fortran_len) {
    std::string padded = s;
    padded.resize(fortran_len, ' ');  // Pad or truncate to exact length
    fortran_subroutine(padded.data(), fortran_len);
}
```

---

## Technical Reference: Fortran → C++ Patterns

### Type Mappings

| Fortran | C++ | Notes |
|---------|-----|-------|
| `INTEGER` | `int32_t` | Often 4 bytes, but verify |
| `INTEGER*2` | `int16_t` | |
| `INTEGER*4` | `int32_t` | |
| `INTEGER*8` | `int64_t` | |
| `REAL` | `float` | Usually 4 bytes |
| `REAL*4` | `float` | |
| `REAL*8` | `double` | |
| `REAL*16` | `long double` | Platform-dependent |
| `DOUBLE PRECISION` | `double` | Synonym for REAL*8 |
| `COMPLEX` | `std::complex<float>` | |
| `COMPLEX*8` | `std::complex<float>` | 4+4 bytes |
| `COMPLEX*16` | `std::complex<double>` | 8+8 bytes |
| `LOGICAL` | `bool` | Or `int32_t` for ABI compat |
| `CHARACTER*N` | `std::string` | Or `std::array<char, N>` |
| Arrays | `std::vector<T>` | Or `Eigen::VectorXd` |

### Array Storage Order

**Critical:** Fortran is column-major. C++ (C-style arrays) is row-major.

```fortran
C     Fortran: column-major
C     A(1,1), A(2,1), A(3,1), A(1,2), A(2,2), A(3,2), ...
      REAL*8 A(3, 3)
```

```cpp
// C-style: row-major
// A[0][0], A[0][1], A[0][2], A[1][0], A[1][1], A[1][2], ...
double A[3][3];

// Eigen default: column-major (matches Fortran!)
Eigen::MatrixXd A(3, 3);  // Column-major by default

// Eigen row-major (matches C):
Eigen::Matrix<double, 3, 3, Eigen::RowMajor> A;
```

When reading Fortran binary files or calling Fortran libraries, use column-major storage or transpose.

### Control Flow Mappings

```cpp
// Fortran: DO 10 I = 1, N, 2
for (int i = 1; i <= N; i += 2)  // Note: 1-based, inclusive

// Fortran: DO WHILE (condition)
while (condition)

// Fortran: IF (X) 10, 20, 30  (arithmetic IF)
if (x < 0) { goto label_10; }
else if (x == 0) { goto label_20; }
else { goto label_30; }
// Better: refactor to eliminate GOTOs

// Fortran: GOTO (10, 20, 30), INDEX  (computed GOTO)
switch (index) {
    case 1: /* 10 */ break;
    case 2: /* 20 */ break;
    case 3: /* 30 */ break;
}

// Fortran: CYCLE
continue;

// Fortran: EXIT
break;

// Fortran: STOP
std::exit(0);  // Or return from main

// Fortran: RETURN
return;

// Fortran: CALL SUBROUTINE(args)
subroutine(args);

// Fortran: RESULT = FUNCTION(args)
result = function(args);
```

### Common Intrinsics Mapping

```cpp
// Fortran: ABS(x)
std::abs(x)

// Fortran: SQRT(x)
std::sqrt(x)

// Fortran: EXP(x), LOG(x)
std::exp(x), std::log(x)

// Fortran: SIN(x), COS(x), TAN(x)
std::sin(x), std::cos(x), std::tan(x)

// Fortran: ASIN(x), ACOS(x), ATAN(x)
std::asin(x), std::acos(x), std::atan(x)

// Fortran: ATAN2(y, x)
std::atan2(y, x)

// Fortran: MOD(a, b)
a % b  // for integers
std::fmod(a, b)  // for floats

// Fortran: MIN(a, b, c, ...), MAX(a, b, c, ...)
std::min({a, b, c})
std::max({a, b, c})

// Fortran: SIGN(a, b)  -- |a| with sign of b
std::copysign(a, b)

// Fortran: INT(x), REAL(x), DBLE(x)
static_cast<int>(x)
static_cast<float>(x)
static_cast<double>(x)

// Fortran: CMPLX(re, im)
std::complex<double>(re, im)

// Fortran: CONJG(z)
std::conj(z)

// Fortran: SIZE(array), SIZE(array, dim)
array.size()  // std::vector
array.rows(), array.cols()  // Eigen

// Fortran: MAXVAL(array), MINVAL(array)
*std::max_element(array.begin(), array.end())
array.maxCoeff()  // Eigen

// Fortran: SUM(array)
std::accumulate(array.begin(), array.end(), 0.0)
array.sum()  // Eigen

// Fortran: MATMUL(A, B)
A * B  // Eigen

// Fortran: TRANSPOSE(A)
A.transpose()  // Eigen

// Fortran: DOT_PRODUCT(a, b)
a.dot(b)  // Eigen
```

---

## Psychological Survival Strategies

### Strategy #1: The Respect Gambit

Dr. Punchcard has forgotten more about numerical methods than you'll ever learn. His algorithm implementations are correct—they've been validated against experiments, benchmarked against theory, cited in papers. The code structure is the problem, not the mathematics.

Lead with respect:

```
❌ "This code is a mess. We need to rewrite it."
   → Dismisses 40 years of work. Dr. Punchcard will fight you forever.

✅ "The numerical methods here are solid—that's why we're migrating 
    instead of replacing. I want to preserve the algorithms while 
    improving the infrastructure around them."
   → Acknowledges his contribution. He becomes an ally, not an adversary.
```

### Strategy #2: The Validation Framework

Dr. Punchcard has been burned by failed rewrites. The way to earn trust is to prove correctness at every step.

Build a validation test suite *before* you migrate anything:

1. **Capture inputs and outputs** from the existing Fortran code
2. **Create regression tests** that compare C++ outputs to Fortran outputs
3. **Run both versions** on every test case, every commit
4. **Show Dr. Punchcard the green checkmarks** after each migration

When the tests pass, he can't argue the migration is wrong. When tests fail, you catch bugs before he sees them.

### Strategy #3: The Parallel Path

Don't demand a complete switchover. Instead, build the C++ version alongside the Fortran:

```
Phase 1: C++ wrapper calls Fortran libraries
Phase 2: Critical subroutines migrated to C++
Phase 3: Most code in C++, some Fortran remains
Phase 4: Pure C++ (optional—maybe never)
```

Dr. Punchcard can verify each phase. The Fortran code remains available as a reference. If the C++ version has problems, you can fall back.

This approach takes longer but succeeds where "big bang" rewrites fail.

### Strategy #4: The Performance Card

Fortran programmers care about performance. It's in the name: Formula Translation was designed to generate code as fast as hand-written assembly.

Show that modern C++ is just as fast:

```
"The C++ version with Eigen uses AVX-512 vectorization. The Fortran 
version uses SSE2 because the compiler is from 2004. The new version 
is 3x faster on the same algorithm."
```

Performance is the one argument that can overcome nostalgia.

### Strategy #5: The Succession Plan

Dr. Punchcard is 67. He will retire. The students he mentored are now professors themselves; they don't maintain code, they publish papers. When he leaves, who understands the Fortran?

Frame the migration as preservation:

```
"I want to learn how this code works. The best way I know is to 
translate it into a language I understand deeply. Can we work through 
the algorithm together while I migrate it?"
```

This flatters his expertise, positions you as a student, and produces a codebase the next generation can maintain.

---

## The Exit Strategy

Sometimes the migration isn't worth it. Before you commit, consider:

**Is the code actually valuable?** Some Fortran codebases are genuine scientific assets—validated algorithms encoding decades of domain knowledge. Others are grad student code that nobody dares touch because nobody understands it. If you can't identify the valuable parts, maybe they don't exist.

**Is maintenance the real problem?** If the Fortran code is stable and rarely changes, maybe wrapping it is enough. Fortran interoperability exists in every serious language. Call it as a library. Don't rewrite what doesn't need rewriting.

**Is Dr. Punchcard the actual blocker?** If his resistance is the only obstacle, wait. Seven years is not that long. Document everything so his successor can help.

**Is the organization ready?** If leadership demanded this migration without understanding the cost, they'll abandon it when it takes longer than expected. Get written commitment before you start.

---

## Appendix: Quick Reference Card

### The Mindset Shift

| Fortran Thinking | C++ Thinking |
|------------------|--------------|
| Variables are implicitly typed | Every variable is declared |
| COMMON blocks share state | Explicit parameter passing |
| Memory is manually overlapped | Memory is managed by RAII |
| Fixed-form columns | Free-form modern syntax |
| GOTOs structure control flow | Structured loops and functions |
| Recursion is forbidden | Recursion is natural |
| Arrays trust the caller | Arrays know their size |
| Strings are fixed-length | Strings are dynamic |
| Validation is for the weak | Tests prove correctness |
| If it compiles, it works | If tests pass, it works |

### The Conversion Checklist

- [ ] IMPLICIT NONE added to original Fortran (find latent bugs)
- [ ] Test suite captures all inputs/outputs
- [ ] Validation framework compares old vs. new
- [ ] COMMON blocks → explicit parameters or classes
- [ ] EQUIVALENCE → separate variables
- [ ] GOTO spaghetti → structured control flow
- [ ] Assumed-size arrays → sized containers
- [ ] Fixed-length CHARACTER → std::string
- [ ] Column-major arrays handled correctly
- [ ] SAVE variables → explicit state management
- [ ] Error codes → exceptions or Expected<T>
- [ ] Performance validated (not just correctness)
- [ ] Dr. Punchcard has reviewed and approved

### The Diplomat's Phrasebook

| What You're Thinking | What You Say |
|---------------------|--------------|
| "This code is ancient." | "This code has an impressive history." |
| "GOTO is evil." | "Modern control flow might simplify this section." |
| "COMMON blocks are global state." | "We could make the data dependencies more explicit." |
| "Nobody can read this." | "Could you walk me through this algorithm?" |
| "IMPLICIT typing is insane." | "I'd like to add declarations for my own understanding." |
| "This will never parallelize." | "Let me prototype the threading approach." |
| "I found 50 bugs." | "I found some interesting edge cases." |
| "Fortran is obsolete." | Never say this. Ever. |

---

*The code has survived for 40 years. With care, the migration will too.*

---

**Document version:** 1.0  
**Last updated:** January 2026  
**Survival probability with this guide:** 68%  
**Survival probability without:** 8%
