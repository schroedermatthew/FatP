---
doc_id: HB-MIGRATION-ANALYSIS-001
doc_type: "Handbook"
title: "C Codebase Migration Analysis"
fatp_components: ["ServiceLocator"]
topics: ["global state", "static analysis", "code comprehension", "dependency analysis", "thread safety", "migration planning"]
constraints: ["legacy C code", "unknown architecture", "limited documentation", "threading correctness"]
cxx_standard: "C++20"
last_verified: "2025-01-09"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# Handbook - C Codebase Migration Analysis

### *Systematic methods for understanding C codebases before migrating to modern C++*

*FAT-P Library — January 2025*

---

## Handbook Card

**Domain:** Pre-migration analysis of C codebases  
**Core principle:** You cannot safely migrate what you do not understand  
**Key discipline:** Systematic inventory of global state, dependencies, and thread safety before writing any migration code  
**Common failure:** Starting migration before understanding what globals are process-inherent vs. migratable  
**Hard rules:** Never migrate mutexes, memory allocators, or OS-coordination globals  
**Applies to:** Any C-to-C++ migration involving global state  
**Build-mode notes:** Thread analysis requires both static tools and runtime sanitizers  
**Guarantees:** Complete inventory of migration candidates and blockers  
**Non-guarantees:** Does not guarantee the migration itself will succeed  

---

## Table of Contents

1. [Scope](#scope)
2. [Prerequisites](#prerequisites)
3. [The Analysis Problem](#the-analysis-problem)
4. [Tool Categories](#tool-categories)
5. [Tool Survey](#tool-survey)
6. [Tool Comparison Matrix](#tool-comparison-matrix)
7. [Recommended Workflows](#recommended-workflows)
8. [Minimal Tooling Fallback](#minimal-tooling-fallback)
9. [Documenting Findings](#documenting-findings)
10. [Analysis Checklist](#analysis-checklist)

---

## Scope

This handbook covers:

- Tools for analyzing C codebase structure
- Methods for finding global state
- Techniques for understanding dependencies
- Thread safety analysis approaches
- Workflows for different budget/time constraints

## Not Covered

- The migration implementation itself (see Case Study - SQLite VFS Migration)
- C++ modernization patterns (see Pattern Guide - ServiceLocator)
- Specific tool installation on all platforms (see tool documentation)

## Prerequisites

- Familiarity with C compilation model (translation units, linkage)
- Basic understanding of static vs. dynamic analysis
- Access to the target codebase source and build system

---

## The Analysis Problem

Before migrating a C codebase to use patterns like ServiceLocator, you must answer:

| Question | Why It Matters |
|----------|----------------|
| What global variables exist? | These are migration candidates |
| Who reads/writes each global? | Determines coupling and migration scope |
| What mutexes protect what data? | Must preserve thread safety |
| What is the initialization order? | ServiceLocator has its own init requirements |
| What is the public API? | Must preserve ABI compatibility |
| What cannot be migrated? | OS-level coordination, bootstrap dependencies |

Answering these questions manually through code reading does not scale. Tools exist.

---

## Tool Categories

| Category | Question Answered | When to Use |
|----------|-------------------|-------------|
| **Symbol indexing** | "Where is X defined? Who uses X?" | Initial exploration, cross-referencing |
| **Call/dependency graphs** | "What depends on what?" | Understanding architecture, finding coupling |
| **Static analysis** | "What could go wrong?" | Finding bugs, validating thread safety |
| **AST querying** | "Find all code matching pattern X" | Precise structural queries |
| **Runtime analysis** | "What actually happens?" | Thread races, coverage, profiling |
| **Visualization** | "Show me the architecture" | Communication, understanding large systems |

---

## Tool Survey

### Doxygen

**Type:** Documentation generator with analysis side effects  
**Cost:** Free (open source)  
**Website:** https://www.doxygen.nl/

**What it produces:**
- Call graphs and caller graphs per function (via Graphviz)
- Include dependency graphs (which files include which)
- Collaboration diagrams (data structure relationships)
- Cross-referenced, browsable HTML documentation
- Symbol index (all functions, variables, types)

**Configuration for analysis:**

```
# Doxyfile settings for migration analysis
EXTRACT_ALL            = YES    # Include undocumented entities
EXTRACT_STATIC         = YES    # Include static functions/variables
EXTRACT_PRIVATE        = YES    # Include private members
CALL_GRAPH             = YES    # Generate call graphs
CALLER_GRAPH           = YES    # Generate caller graphs
HAVE_DOT               = YES    # Use Graphviz for graphs
COLLABORATION_GRAPH    = YES    # Show data relationships
INCLUDE_GRAPH          = YES    # Show #include dependencies
INCLUDED_BY_GRAPH      = YES    # Show reverse includes
SOURCE_BROWSER         = YES    # Hyperlinked source code
REFERENCED_BY_RELATION = YES    # Show what references each entity
REFERENCES_RELATION    = YES    # Show what each entity references
```

**Strengths:**
- Low barrier to entry — most C developers have used it
- Visual call graphs immediately show coupling
- Cross-reference lets you click from use to definition
- Handles large codebases reasonably well
- Integrates with existing documentation workflows

**Weaknesses:**
- No direct "list all globals" report — must navigate to find them
- Call graphs become unreadable for highly-connected functions (>20 callers)
- Doesn't understand mutex semantics or thread safety
- Analysis is incidental to documentation purpose
- Graphviz can be slow on large graphs

**Best for:** Initial orientation. Understanding call hierarchies. Finding all callers of accessor functions.

**Migration-specific use:** Generate caller graph for functions that access globals (e.g., `sqlite3_vfs_find`). The graph shows all entry points into that subsystem.

---

### cscope

**Type:** Symbol cross-reference database  
**Cost:** Free (open source)  
**Website:** http://cscope.sourceforge.net/

**What it does:** Indexes all symbols in C source files and provides interactive queries.

**Query types:**
```
0: Find this C symbol              (any occurrence)
1: Find this global definition     (where it's defined)
2: Find functions called by this   (callees)
3: Find functions calling this     (callers)
4: Find this text string
6: Find this egrep pattern
7: Find this file
8: Find files #including this file
```

**Setup:**
```bash
# Create file list
find src/ -name "*.[ch]" > cscope.files

# Build database (-b = build, -q = quick lookup, -k = kernel mode: ignore /usr/include)
cscope -b -q -k

# Interactive use
cscope -d

# Non-interactive query (for scripting)
cscope -d -L1 vfsList    # Find global definition of vfsList
cscope -d -L3 vfsList    # Find all callers of vfsList
```

**Strengths:**
- Very fast, even on huge codebases (Linux kernel)
- Query type 1 directly answers "where are globals defined"
- Terminal-based — works over SSH
- Integrates with vim (`:cs find`), emacs
- Scriptable for batch analysis

**Weaknesses:**
- Pattern-based, not semantic — can have false positives
- No visualization whatsoever
- Requires you to know what to look for
- No threading awareness
- Doesn't understand C preprocessor deeply

**Best for:** Fast interactive exploration. "Who calls this function?" answered in milliseconds.

**Migration-specific use:** Query type 1 on suspected global names. Query type 3 to trace all usage. Script to enumerate all globals and their call sites.

---

### Universal Ctags

**Type:** Symbol indexer (tags file generator)  
**Cost:** Free (open source)  
**Website:** https://ctags.io/

**What it does:** Creates a tags file mapping symbol names to file:line locations.

**Output fields available:**
```bash
# Generate tags with extra fields
ctags --fields=+niazS --extras=+q -R src/

# Fields: n=line number, i=inheritance, a=access, z=kind, S=signature
```

**Tag kinds for C:**
```
d  macro definitions
e  enumerators
f  function definitions
g  enumeration names
h  included header files
m  struct/union members
s  structure names
t  typedefs
u  union names
v  variable definitions
```

**Strengths:**
- Universal format — works with vim, emacs, VS Code, etc.
- Very fast generation
- Actively maintained (unlike Exuberant Ctags)
- Field filters let you extract just what you need

**Weaknesses:**
- Definition locations only — no call graph
- No cross-referencing (who calls what)
- Single-direction lookup

**Best for:** Editor integration. Quick "jump to definition."

**Migration-specific use:** Extract all `v` (variable) tags with file scope to get globals list:
```bash
ctags -x --c-kinds=v src/*.c | grep -v "static"  # External globals
ctags -x --c-kinds=v src/*.c | grep "static"     # File-scope statics
```

---

### Understand (SciTools)

**Type:** Commercial code comprehension IDE  
**Cost:** ~$995/year (individual), volume discounts  
**Website:** https://scitools.com/

**What it produces:**
- Global Objects report — directly lists all globals
- Dependency graphs at file, function, variable, and type level
- "Variables Modified By" and "Variables Used By" reports
- Metrics: coupling, complexity, fan-in/fan-out
- Architecture diagrams auto-generated from code
- Custom queries via Perl or Python API

**Built-in reports relevant to migration:**
```
Reports > Metrics > Global Objects
Reports > Graphical > Dependency Graph
Reports > References > Variable References (filter to globals)
```

**Strengths:**
- Built specifically for code comprehension
- "Global Objects" report is exactly what migration needs
- Visual dependency graphs show coupling at a glance
- Can query "show all functions that write to X"
- Handles very large codebases (millions of lines)
- API for scripting custom analyses

**Weaknesses:**
- Expensive for individual developers
- Learning curve to access full power
- Thread analysis is basic — finds pthread calls, doesn't prove safety
- IDE workflow may not fit all teams

**Best for:** Organizations doing repeated large-scale migrations. The investment pays off if analysis is frequent.

**Migration-specific use:** 
1. Run Global Objects report — immediate inventory
2. For each global, run "Referenced By" to see all access sites
3. Export dependency graph to visualize subsystem boundaries

---

### Sourcetrail

**Type:** Code exploration with visualization  
**Cost:** Free (open source, but discontinued)  
**Website:** https://github.com/CoatiSoftware/Sourcetrail (archived)

**What it produces:**
- Searchable index of all symbols
- Interactive graph showing relationships
- Click-to-navigate between definition and uses
- Aggregated view of dependencies

**Strengths:**
- Beautiful visualization of code structure
- Interactive — explore by clicking
- Cross-platform GUI
- Indexes C, C++, Java, Python

**Weaknesses:**
- **Project discontinued in 2021** — no updates, eventual bit rot
- Struggles with very large codebases (>500K lines)
- No specific "globals" filter
- No threading analysis
- Indexing can be slow

**Best for:** Visual exploration of medium-sized codebases. Understanding unfamiliar code through graphs.

**Migration-specific use:** Search for suspected globals, visually trace what touches them. Good for ad-hoc exploration, not systematic analysis.

---

### cflow

**Type:** Call graph generator  
**Cost:** Free (GNU)  
**Website:** https://www.gnu.org/software/cflow/

**What it produces:**
```
main() <int main (int argc, char *argv[]) at main.c:10>:
    sqlite3_open() <int sqlite3_open(...) at sqlite3.c:2451>:
        sqlite3_vfs_find() <sqlite3_vfs *sqlite3_vfs_find(...) at os.c:42>:
            sqlite3MutexAlloc()
```

**Options:**
```bash
cflow --main=sqlite3_open src/*.c    # Start from specific function
cflow -r src/*.c                      # Reverse graph (callers)
cflow --format=posix src/*.c          # Machine-readable output
cflow -d 3 src/*.c                    # Limit depth to 3
```

**Strengths:**
- Pure text output — scriptable
- Can pipe to Graphviz for visualization
- Fast on large codebases
- Reverse mode shows callers

**Weaknesses:**
- Call graphs only — no variable tracking
- Includes dead code paths (static analysis, not runtime)
- Limited semantic understanding
- Text trees hard to read for deep hierarchies

**Best for:** Quick "what does this function call" queries. Supplement to other tools.

**Migration-specific use:** Generate reverse call graph from global accessor functions to find all entry points.

---

### clang-query

**Type:** AST pattern matching  
**Cost:** Free (LLVM)  
**Website:** https://clang.llvm.org/docs/LibASTMatchersReference.html

**What it does:** Query Clang's Abstract Syntax Tree using matchers. Find any syntactic pattern with semantic precision.

**Requires:** Compilation database (`compile_commands.json`)
```bash
# Generate with Bear
bear -- make

# Or with CMake
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
```

**Example queries:**
```
# Find all file-scope static variables
match varDecl(hasGlobalStorage(), isStaticStorageClass())

# Find all calls to functions containing "mutex"
match callExpr(callee(functionDecl(matchesName(".*mutex.*"))))

# Find all variables of type sqlite3_vfs*
match varDecl(hasType(pointerType(pointee(recordDecl(hasName("sqlite3_vfs"))))))
```

**Strengths:**
- Precise: queries based on actual language semantics
- "Find all static variables" is one query
- Can match complex patterns (mutex lock around global access)
- Scriptable with clang-query or C++ LibTooling

**Weaknesses:**
- Steep learning curve (AST matcher DSL)
- Requires compilable code with compile_commands.json
- No visualization
- Queries are exact — you must know what to ask for
- Documentation is reference, not tutorial

**Best for:** Precise structural queries. Automation of analysis patterns you'll reuse.

**Migration-specific use:**
```
# List all globals
clang-query -p build/ -c "match varDecl(hasGlobalStorage())" src/*.c

# Find mutex patterns
clang-query -p build/ -c "match callExpr(callee(functionDecl(hasName(\"pthread_mutex_lock\"))))" src/*.c
```

---

### Coccinelle

**Type:** Semantic patching tool  
**Cost:** Free (open source)  
**Website:** https://coccinelle.gitlabpages.inria.fr/website/

**What it does:** Match and transform C code using semantic patterns. Designed for Linux kernel maintenance.

**Example script (finding globals accessed under mutex):**
```
@@
identifier mutex, global;
@@

  pthread_mutex_lock(&mutex);
  ...
* global
  ...
  pthread_mutex_unlock(&mutex);
```

**Running:**
```bash
spatch --sp-file find_locked_globals.cocci --dir src/
```

**Strengths:**
- Semantic understanding — not grep
- Pattern language designed for C idioms
- Can generate transformations, not just reports
- Used for large-scale kernel refactoring

**Weaknesses:**
- Domain-specific language to learn
- Analysis is secondary to transformation
- No visualization
- Overkill for one-time analysis

**Best for:** Reusable analysis patterns. Finding specific idioms across large codebases.

**Migration-specific use:** Write patterns for "global accessed inside lock" to find which globals are thread-protected.

---

### Clang Static Analyzer (scan-build)

**Type:** Bug finder via symbolic execution  
**Cost:** Free (LLVM)  
**Website:** https://clang-analyzer.llvm.org/

**What it finds:**
- Null pointer dereferences
- Use-after-free
- Memory leaks
- Dead code
- Logic errors

**Usage:**
```bash
scan-build make
scan-build -enable-checker alpha.security.ArrayBoundV2 make
scan-view /tmp/scan-build-*
```

**Strengths:**
- Deep semantic analysis
- HTML reports with annotated source paths
- Catches real bugs

**Weaknesses:**
- Focused on bugs, not architecture
- Doesn't produce dependency graphs
- No "list globals" capability
- Race detection is limited

**Best for:** Finding bugs before/after migration. Validating migration correctness.

**Migration-specific use:** Run before migration to establish baseline. Run after to ensure no regressions.

---

### Infer (Facebook/Meta)

**Type:** Static analyzer for memory and concurrency  
**Cost:** Free (open source)  
**Website:** https://fbinfer.com/

**Key checkers:**
- `THREAD_SAFETY_VIOLATION`: finds potential data races statically
- `NULL_DEREFERENCE`, `USE_AFTER_FREE`: memory bugs
- `DEADLOCK`: lock ordering violations

**Usage:**
```bash
infer run -- make
infer explore  # Interactive results viewer
```

**Annotations for better results:**
```c
#include <infer_annotations.h>

THREAD_SAFE int counter;  // Infer knows this must be synchronized
```

**Strengths:**
- Static race detection without running code
- Compositional — scales to large codebases
- Actively maintained by Meta

**Weaknesses:**
- Best results require annotations
- False positives need tuning
- Focused on bugs, not architecture

**Best for:** Static thread safety analysis. Complements runtime tools.

**Migration-specific use:** Identify which globals have potential race conditions before deciding migration strategy.

---

### ThreadSanitizer (TSan)

**Type:** Runtime race detector  
**Cost:** Free (LLVM/GCC)  
**Website:** https://clang.llvm.org/docs/ThreadSanitizer.html

**Usage:**
```bash
# Compile with TSan
gcc -fsanitize=thread -g -o test test.c -lpthread
# or
clang -fsanitize=thread -g -o test test.c -lpthread

# Run — races reported at runtime
./test
```

**Output:**
```
WARNING: ThreadSanitizer: data race
  Write of size 4 at 0x... by thread T1:
    #0 increment_counter counter.c:15
    #1 thread_func counter.c:22
  Previous read of size 4 at 0x... by thread T2:
    #0 read_counter counter.c:28
```

**Strengths:**
- Precise: shows exact stacks of racing accesses
- No false positives (if race detected, it's real)
- Low annotation burden

**Weaknesses:**
- Runtime only — needs tests that trigger races
- Cannot prove absence of races
- ~5-15x slowdown
- Misses races that don't occur during test run

**Best for:** Validating thread safety. Run test suite under TSan before and after migration.

**Migration-specific use:** Run existing tests with TSan. Any races involving globals you're migrating must be understood and preserved.

---

### Helgrind (Valgrind)

**Type:** Runtime race and lock-order detector  
**Cost:** Free (open source)  
**Website:** https://valgrind.org/docs/manual/hg-manual.html

**Usage:**
```bash
valgrind --tool=helgrind ./test
```

**Additional checks vs TSan:**
- Lock ordering violations (potential deadlocks)
- Misuse of POSIX threading API
- Destruction of locked mutexes

**Strengths:**
- No recompilation needed
- Lock order analysis finds deadlock potential
- Works on unmodified binaries

**Weaknesses:**
- Much slower than TSan (~20-100x)
- Higher false positive rate
- Linux only (mostly)

**Best for:** Lock ordering analysis. Complement to TSan.

**Migration-specific use:** Check for lock ordering issues if migration introduces new mutexes.

---

### CppDepend

**Type:** Architecture and dependency analysis  
**Cost:** ~$399/year  
**Website:** https://www.cppdepend.com/

**What it produces:**
- Dependency structure matrix (DSM)
- Dependency graphs
- Code metrics (coupling, cohesion, complexity)
- Custom queries via CQLinq (LINQ-like query language)
- Trend analysis over time

**CQLinq examples:**
```csharp
// Find all global variables
from v in Variables where v.IsGlobal select v

// Find functions with high coupling
from f in Functions where f.NbTypesUsed > 20 select f
```

**Strengths:**
- Built for architecture analysis
- Query language for custom reports
- Visual DSM shows coupling patterns
- Tracks trends across versions

**Weaknesses:**
- Commercial cost
- Windows-focused (Linux support exists)
- C++ emphasis — C support is secondary

**Best for:** Ongoing architecture governance. Teams with budget for tooling.

**Migration-specific use:** DSM visualization shows which parts of codebase are tightly coupled to globals.

---

## Tool Comparison Matrix

| Tool | Cost | Global Finding | Call Graphs | Thread Analysis | AST Precision | Visualization | Large Codebase | Windows |
|------|------|----------------|-------------|-----------------|---------------|---------------|----------------|---------|
| Doxygen | Free | Indirect | Yes | No | No | Yes | Good | Native |
| cscope | Free | Query-based | No | No | No | No | Excellent | Limited* |
| Universal Ctags | Free | Tag filter | No | No | No | No | Excellent | Native |
| **Understand** | $995/yr | **Direct report** | Yes | Limited | Yes | Yes | Excellent | Native |
| Sourcetrail | Free* | Search | Yes | No | Yes | **Best** | Medium | Native |
| cflow | Free | No | Text only | No | No | Graphviz | Good | Limited |
| clang-query | Free | **Precise** | No | No | **Best** | No | Good | Native |
| Coccinelle | Free | Pattern | No | No | Good | No | Good | WSL |
| scan-build | Free | No | No | Limited | Yes | HTML | Good | Native |
| **Infer** | Free | No | No | **Static races** | Yes | No | Good | WSL |
| **TSan** | Free | No | No | **Runtime races** | N/A | No | N/A | WSL |
| Helgrind | Free | No | No | Runtime+locks | N/A | No | N/A | WSL |
| CppDepend | $399/yr | Query | Yes | No | Yes | DSM | Good | Native |
| Dr. Memory | Free | No | No | Basic | N/A | No | N/A | Native |

\* Sourcetrail is discontinued but still functional.  
\* cscope requires manual build or pre-built binary on Windows.  
**WSL** = Requires Windows Subsystem for Linux.

**Legend:**
- **Bold** = best in category
- "Direct report" = tool has built-in globals listing
- "Query-based" = can find globals but requires knowing to ask
- "Indirect" = information exists but must be navigated to

---

## Recommended Workflows

### Workflow A: Well-Funded Team

**Budget:** Commercial tools available  
**Timeline:** Thorough analysis before migration

| Phase | Tools | Output |
|-------|-------|--------|
| 1. Inventory | Understand: Global Objects report | Complete list of globals |
| 2. Dependencies | Understand: Dependency graphs | Visual architecture |
| 3. Thread safety | Infer + TSan | Race inventory |
| 4. Deep queries | clang-query (if needed) | Specific pattern matches |
| 5. Validation | TSan in CI | Ongoing safety |

**Time estimate:** 2-3 days for medium codebase

---

### Workflow B: Open-Source Stack

**Budget:** Free tools only  
**Timeline:** Systematic but efficient

**Detailed guides:**
- Linux/macOS: [User Manual - C Codebase Analysis with Open Source Tools](User_Manual_-_C_Codebase_Analysis_with_Open_Source_Tools.md)
- Windows: [User Manual - C Codebase Analysis Windows PowerShell](User_Manual_-_C_Codebase_Analysis_Windows_PowerShell.md)

| Phase | Tools | Output |
|-------|-------|--------|
| 1. Orientation | Doxygen (call graphs) | Understanding of structure |
| 2. Inventory | clang-query + ctags | Globals list |
| 3. Cross-reference | cscope | Usage sites for each global |
| 4. Thread safety | TSan (runtime) + Infer (static) | Race inventory |
| 5. Validation | TSan in CI | Ongoing safety |

**Platform notes:**
- Linux/macOS: Full tool support, TSan and Infer native
- Windows: TSan/Infer require WSL; Dr. Memory and Application Verifier as alternatives

**Time estimate:** 3-5 days for medium codebase

---

### Workflow C: Minimal Tooling / Constrained Environment

**Budget:** Only basic Unix tools  
**Timeline:** Pragmatic analysis

**Detailed guide:** See [User Manual - C Codebase Analysis Minimal Tooling](User_Manual_-_C_Codebase_Analysis_Minimal_Tooling.md) for:
- grep/ripgrep patterns and PowerShell equivalents
- Complete automation scripts (bash and PowerShell)
- Editor plugins (Vim, VS Code, Emacs, JetBrains)
- Symbol lookup scripts

| Phase | Tools | Output |
|-------|-------|--------|
| 1. Inventory | grep + nm | Globals list |
| 2. Cross-reference | grep or cscope | Usage sites |
| 3. Thread safety | grep for mutex patterns | Manual review list |
| 4. Validation | Manual code review | Spot checks |

**Time estimate:** 1-2 weeks for medium codebase, higher risk of missing items

See [Minimal Tooling Fallback](#minimal-tooling-fallback) for specific commands.

---

### Workflow D: Unknown Codebase, Time Pressure

**Budget:** Any  
**Timeline:** Fast triage

| Phase | Tools | Output |
|-------|-------|--------|
| 1. Quick visual | Sourcetrail (if available) or Doxygen | Mental model |
| 2. Find obvious globals | grep "^static.*=" | Quick inventory |
| 3. Run tests under TSan | TSan | Find existing races |
| 4. Iterate | cscope interactively | Explore as needed |

**Time estimate:** Hours, but incomplete coverage

---

## Minimal Tooling Fallback

When commercial tools aren't available and clang tooling is impractical (no compilation database, cross-compilation, embedded systems), fall back to basic Unix tools.

### Finding Global Variables

**File-scope static variables:**
```bash
grep -rn "^static\s\+[a-zA-Z_]" --include="*.c" src/ | \
    grep -v "static\s\+inline\|static\s\+void\|static\s\+int\s\+[a-z]*(" 
```

**External linkage globals:**
```bash
# From source
grep -rn "^[a-zA-Z_][a-zA-Z0-9_]*\s\+[a-zA-Z_].*=" --include="*.c" src/ | \
    grep -v "^static\|^\s"

# From object files
nm -g *.o | grep -E "^[0-9a-f]+\s+[BDR]\s+"
```

### Tracing Usage

**All references to a symbol:**
```bash
grep -rn "\bvfsList\b" --include="*.c" --include="*.h" src/
```

**Count uses per file (indicates coupling):**
```bash
grep -rn "\bvfsList\b" --include="*.c" src/ | \
    cut -d: -f1 | sort | uniq -c | sort -rn
```

### Finding Mutex Patterns

**Mutex declarations:**
```bash
grep -rn "pthread_mutex_t\|sqlite3_mutex\s*\*\|mtx_t\b" \
    --include="*.c" --include="*.h" src/
```

**Lock/unlock around globals:**
```bash
grep -B5 -A5 "vfsList" src/os.c | grep -i "mutex\|lock"
```

### Finding Public API

**Exported function declarations:**
```bash
grep -rn "^[A-Z_]*API\|__attribute__.*visibility" --include="*.h" src/
```

**Symbols in shared library:**
```bash
nm -D --defined-only libfoo.so | grep " T "
```

### Automated Discovery Script

```bash
#!/bin/bash
# analyze_globals.sh - Quick global state inventory

SRC_DIR="${1:-.}"
OUTPUT="global_analysis.md"

echo "# Global State Analysis" > "$OUTPUT"
echo "Generated: $(date)" >> "$OUTPUT"
echo "" >> "$OUTPUT"

echo "## Static Variables (File Scope)" >> "$OUTPUT"
echo '```' >> "$OUTPUT"
grep -rn "^static\s\+[a-zA-Z_]" --include="*.c" "$SRC_DIR" | \
    grep -v "static\s\+inline\|static\s\+void\s\|static\s\+int\s\+[a-z]*(" >> "$OUTPUT" 2>/dev/null
echo '```' >> "$OUTPUT"
echo "" >> "$OUTPUT"

echo "## Mutex/Lock Patterns" >> "$OUTPUT"
echo '```' >> "$OUTPUT"
grep -rn "mutex\|_lock\|_unlock\|CRITICAL_SECTION" \
    --include="*.c" --include="*.h" "$SRC_DIR" 2>/dev/null | head -50 >> "$OUTPUT"
echo '```' >> "$OUTPUT"
echo "" >> "$OUTPUT"

echo "## Potential Public API (visibility attributes)" >> "$OUTPUT"
echo '```' >> "$OUTPUT"
grep -rn "^[A-Z_]*API\|visibility\|__declspec.*dllexport" \
    --include="*.h" "$SRC_DIR" 2>/dev/null | head -50 >> "$OUTPUT"
echo '```' >> "$OUTPUT"

echo "Analysis written to $OUTPUT"
```

### Limitations of Minimal Approach

| Aspect | Risk |
|--------|------|
| Completeness | grep misses complex patterns, macro-hidden globals |
| False positives | Pattern matching includes comments, strings |
| Thread safety | No automated race detection |
| Dependencies | Must manually trace each global |
| Maintenance | Scripts need updates as codebase evolves |

Use minimal tooling as starting point. Invest in proper tools for production migrations.

---

## Documenting Findings

Regardless of tools used, produce structured documentation:

### Global State Inventory Template

```markdown
## Global State Inventory

| Variable | File | Type | Linkage | Mutex | Migrateable? | Notes |
|----------|------|------|---------|-------|--------------|-------|
| vfsList | os.c | sqlite3_vfs* | internal | STATIC_MAIN | YES | VFS registry head |
| unixBigLock | os_unix.c | sqlite3_mutex* | internal | self | NO | OS file coordination |
| inodeList | os_unix.c | unixInodeInfo* | internal | unixBigLock | NO | Cross-process state |
| mem0 | malloc.c | Mem0Global | internal | STATIC_MEM | NO | Bootstrap, hot path |
```

### Dependency Documentation

```markdown
## Dependency Map: vfsList

**Readers:**
- sqlite3_vfs_find() — os.c:175
- sqlite3OsInit() — os.c:312

**Writers:**
- sqlite3_vfs_register() — os.c:214
- sqlite3_vfs_unregister() — os.c:248
- vfsUnlink() — os.c:261 (helper)

**Protected by:** SQLITE_MUTEX_STATIC_MAIN

**Coupling score:** Low (3 files, 5 functions)

**Migration verdict:** Good candidate. Well-encapsulated.
```

### Thread Safety Documentation

```markdown
## Thread Safety: VFS Subsystem

**Mutexes involved:**
1. SQLITE_MUTEX_STATIC_MAIN — protects vfsList
2. unixBigLock — protects file operations (not migrated)

**Lock ordering:** STATIC_MAIN > unixBigLock

**Known race conditions:** None found (TSan clean)

**Migration impact:** New VfsRegistry mutex must be at same level as STATIC_MAIN
```

---

## Analysis Checklist

### Before Starting

- [ ] Source code access confirmed
- [ ] Build system understood
- [ ] Can compile code (for clang tooling)
- [ ] Test suite exists and runs
- [ ] Tool selection made based on budget/timeline

### Global Inventory

- [ ] All static file-scope variables identified
- [ ] All extern globals identified
- [ ] Thread-local storage identified
- [ ] Each global's type documented
- [ ] Each global's mutex (if any) documented

### Dependency Analysis

- [ ] Call graph generated for key functions
- [ ] For each global: readers listed
- [ ] For each global: writers listed
- [ ] Coupling assessed (how many files touch it?)
- [ ] Initialization order understood

### Thread Safety

- [ ] Mutex patterns documented
- [ ] Lock ordering rules identified
- [ ] TSan run on test suite
- [ ] Any races documented with stack traces
- [ ] Static analysis (Infer) run if available

### Public API

- [ ] Exported symbols listed
- [ ] ABI contracts documented
- [ ] Header file dependencies mapped

### Migration Decisions

- [ ] Each global classified: migrate / don't migrate / partial
- [ ] Rationale documented for each decision
- [ ] OS-level coordination globals identified
- [ ] Bootstrap dependencies identified
- [ ] Hot-path globals identified

### Validation Plan

- [ ] TSan integrated into CI
- [ ] Benchmark baseline established
- [ ] Rollback plan documented

---

## Glossary

| Term | Definition |
|------|------------|
| **AST** | Abstract Syntax Tree — compiler's structured representation of source code |
| **Compilation database** | JSON file mapping source files to their compile commands (compile_commands.json) |
| **DSM** | Dependency Structure Matrix — visual representation of module dependencies |
| **Internal linkage** | Symbol visible only within its translation unit (static in C) |
| **External linkage** | Symbol visible across translation units (non-static file scope) |
| **TSan** | ThreadSanitizer — runtime race detector |
| **Semantic analysis** | Understanding code meaning, not just text patterns |

---

*FAT-P Library — Handbook HB-MIGRATION-ANALYSIS-001*  
*Last updated: January 2025*
