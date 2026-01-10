---
doc_id: UM-MIGRATION-ANALYSIS-004
doc_type: "User Manual"
title: "Analyzing C Code for Migration (Open Source Tools - Linux)"
fatp_components: ["ServiceLocator"]
topics: ["global state", "C migration", "codebase analysis", "doxygen", "cscope", "clang-query", "TSan", "Infer", "static analysis", "call graphs"]
constraints: ["legacy C code", "Linux environment", "open source tools", "deep analysis"]
cxx_standard: "C++17"
last_verified: "2025-01-09"
audience: ["C developers", "C++ developers", "Linux developers", "AI assistants"]
status: "reviewed"
---

# User Manual - Analyzing C Code for Migration (Open Source Tools - Linux)

### *Deep analysis using Doxygen, cscope, clang-query, Thread Sanitizer, and Infer*

*FAT-P Library — January 2025*

---

## User Manual Card

**What this does:** Deep analysis of C code using specialized open source tools  
**Platform:** Linux (Ubuntu, Debian, RHEL, Fedora, Arch)  
**Time required:** 1-2 days setup, then 1-3 days analysis for medium codebase  
**Tools covered:** Doxygen, cscope, clang-query, Thread Sanitizer (TSan), Infer  
**Skills assumed:** Basic Linux command line, can install packages  
**Skills NOT assumed:** Tool-specific knowledge, LLVM internals, static analysis theory  
**When to use this:** When grep isn't enough — you need call graphs, precise cross-references, or thread safety proof  
**Prerequisite:** Complete the grep-based analysis first (User Manual UM-MIGRATION-ANALYSIS-001)  
**Read next:** Migration Guide - Global State to ServiceLocator

---

## Table of Contents

1. [When Do You Need These Tools?](#chapter-1-when-do-you-need-these-tools)
2. [Tool Overview and Selection](#chapter-2-tool-overview-and-selection)
3. [Setting Up Your Environment](#chapter-3-setting-up-your-environment)
4. [Installing Each Tool](#chapter-4-installing-each-tool)
5. [Doxygen: Call Graphs and Documentation](#chapter-5-doxygen-call-graphs-and-documentation)
6. [cscope: Cross-Reference Database](#chapter-6-cscope-cross-reference-database)
7. [clang-query: AST-Level Analysis](#chapter-7-clang-query-ast-level-analysis)
8. [Thread Sanitizer: Finding Data Races](#chapter-8-thread-sanitizer-finding-data-races)
9. [Infer: Static Analysis](#chapter-9-infer-static-analysis)
10. [Combining Tools for Complete Analysis](#chapter-10-combining-tools-for-complete-analysis)
11. [Worked Example: Full Analysis](#chapter-11-worked-example-full-analysis)
12. [Troubleshooting](#chapter-12-troubleshooting)
13. [FAQ](#faq)
14. [Glossary](#glossary)
15. [Quick Reference](#quick-reference)

---

# Chapter 1: When Do You Need These Tools?

## What grep Can't Do

The grep-based analysis (from User Manual UM-MIGRATION-ANALYSIS-001) finds globals and measures basic coupling. But grep has limitations:

| Task | grep | Specialized Tool |
|------|------|------------------|
| Find static declarations | ✓ Good | ✓ Perfect |
| Count file references | ✓ Good | ✓ Perfect |
| **Generate call graphs** | ✗ Can't | ✓ Doxygen |
| **Find all callers of a function** | ~ Approximate | ✓ cscope |
| **Find through macros** | ✗ Can't | ✓ clang-query |
| **Prove thread safety** | ✗ Can't | ✓ TSan |
| **Find null pointer bugs** | ✗ Can't | ✓ Infer |

## When to Use This Manual

Use these tools when:

1. **You need to understand call chains** — "Which functions ultimately access this global?"
2. **Macros hide the code structure** — grep sees `LOCK()`, not `pthread_mutex_lock(&g_mutex)`
3. **You need proof of thread safety** — not just "it looks protected"
4. **The codebase is large** — 100K+ lines where manual analysis is impractical
5. **You're making a case to management** — visual call graphs are persuasive

## When grep Is Enough

Stick with grep when:
- Small codebase (<20K lines)
- No macros hiding globals
- Single-threaded code
- You just need a quick inventory
- Tools won't install on your system

## Prerequisites

Before starting this manual:

1. **Complete the grep-based analysis first** — You should already have:
   - `migration_analysis/reports/static_globals.txt`
   - `migration_analysis/reports/coupling.txt`
   - `migration_analysis/reports/decisions.md`

2. **Have sudo access** — Most tools require installation

3. **Have internet access** — To download packages

4. **Have disk space** — Doxygen output and Infer can use several GB

---

# Chapter 2: Tool Overview and Selection

## The Tools

| Tool | What It Does | Complexity | Value |
|------|--------------|------------|-------|
| **Doxygen** | Generates call graphs, documentation | Low | High |
| **cscope** | Cross-reference database | Low | High |
| **clang-query** | AST queries (finds through macros) | Medium | Medium |
| **TSan** | Runtime data race detection | Medium | Very High |
| **Infer** | Static analysis (null, memory, threads) | Medium | High |

## Tool Selection Guide

Not every project needs every tool. Use this guide:

```
Do you need call graphs or visual documentation?
  YES → Install Doxygen

Do you need to find all callers/callees of functions?
  YES → Install cscope

Does your code use macros that hide globals or function calls?
  YES → Install clang-query
  
Is your code multi-threaded?
  YES → Install TSan (must recompile code)
  
Do you want static analysis for bugs (null pointers, memory leaks)?
  YES → Install Infer
```

## Minimum Recommended Set

If you can only install some tools:

1. **Doxygen** — Everyone should have this. Visual call graphs are invaluable.
2. **cscope** — Fast cross-referencing, works with any editor.

These two give you 80% of the value with minimal effort.

## If Installation Fails

Each tool section includes:
- What to do if installation fails
- Alternative approaches
- When you can skip the tool entirely

---

# Chapter 3: Setting Up Your Environment

## Verify Your Linux Distribution

```bash
# Check your distribution
cat /etc/os-release

# Check architecture
uname -m
```

Note your distribution name and version. This determines which package manager and package names to use.

## Package Manager Reference

| Distribution | Package Manager | Install Command |
|--------------|-----------------|-----------------|
| Ubuntu, Debian | apt | `sudo apt install <package>` |
| Fedora | dnf | `sudo dnf install <package>` |
| RHEL 8+, CentOS 8+ | dnf | `sudo dnf install <package>` |
| RHEL 7, CentOS 7 | yum | `sudo yum install <package>` |
| Arch | pacman | `sudo pacman -S <package>` |
| openSUSE | zypper | `sudo zypper install <package>` |

## Create Analysis Directory Structure

```bash
cd /path/to/your/project

# Extended structure for these tools
mkdir -p migration_analysis/{reports,scripts,doxygen,cscope,clang,tsan,infer}
```

## Environment Check Script

Save as `migration_analysis/scripts/check_tools.sh`:

```bash
#!/bin/bash
# check_tools.sh - Check which analysis tools are available

echo "========================================"
echo "    Open Source Tools Environment Check"
echo "========================================"
echo ""

check_tool() {
    local name="$1"
    local cmd="$2"
    local install_hint="$3"
    
    printf "%-15s: " "$name"
    if command -v "$cmd" &>/dev/null; then
        version=$("$cmd" --version 2>&1 | head -1)
        echo "OK - $version"
        return 0
    else
        echo "NOT INSTALLED"
        echo "              Install: $install_hint"
        return 1
    fi
}

echo "Build Tools (required for some analyzers):"
echo "-------------------------------------------"
check_tool "gcc" "gcc" "sudo apt install build-essential"
check_tool "clang" "clang" "sudo apt install clang"
check_tool "make" "make" "sudo apt install build-essential"

echo ""
echo "Analysis Tools:"
echo "---------------"
check_tool "Doxygen" "doxygen" "sudo apt install doxygen graphviz"
check_tool "cscope" "cscope" "sudo apt install cscope"
check_tool "clang-query" "clang-query" "sudo apt install clang-tools"
check_tool "Infer" "infer" "See Chapter 4 for installation"

echo ""
echo "Graphviz (for Doxygen graphs):"
echo "------------------------------"
check_tool "dot" "dot" "sudo apt install graphviz"

echo ""
echo "========================================"

# Summary
echo ""
echo "Summary:"
missing=0
for cmd in doxygen cscope dot; do
    command -v "$cmd" &>/dev/null || ((missing++))
done

if [ $missing -eq 0 ]; then
    echo "  Core tools ready (Doxygen, cscope)"
else
    echo "  $missing core tool(s) need installation"
fi
```

Run it:
```bash
chmod +x migration_analysis/scripts/check_tools.sh
./migration_analysis/scripts/check_tools.sh
```

---

# Chapter 4: Installing Each Tool

This chapter provides complete installation instructions for each tool, with troubleshooting for common failures.

## 4.1 Doxygen and Graphviz

Doxygen generates documentation and call graphs. Graphviz renders the graphs.

### Ubuntu/Debian:

```bash
sudo apt update
sudo apt install -y doxygen graphviz
```

### Fedora:

```bash
sudo dnf install -y doxygen graphviz
```

### RHEL/CentOS 8+:

```bash
sudo dnf install -y epel-release
sudo dnf install -y doxygen graphviz
```

### RHEL/CentOS 7:

```bash
sudo yum install -y epel-release
sudo yum install -y doxygen graphviz
```

### Arch:

```bash
sudo pacman -S doxygen graphviz
```

### Verify installation:

```bash
doxygen --version
# Should show: 1.9.x or similar

dot -V
# Should show: dot - graphviz version 2.x.x
```

### If installation fails:

**"Package not found":**
```bash
# Search for the package
apt-cache search doxygen  # Debian/Ubuntu
dnf search doxygen        # Fedora/RHEL

# Package might have a different name
```

**"Unable to locate package" on Ubuntu:**
```bash
# Update package list
sudo apt update

# If still fails, check your sources.list
cat /etc/apt/sources.list
```

**Building from source (last resort):**
```bash
# If packages don't work
wget https://www.doxygen.nl/files/doxygen-1.9.6.src.tar.gz
tar xzf doxygen-1.9.6.src.tar.gz
cd doxygen-1.9.6
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### If you can't install Doxygen:

Skip Chapter 5. You lose:
- Call graphs
- Visual documentation

You can still do coupling analysis with grep and cscope.

---

## 4.2 cscope

cscope builds a cross-reference database of your code.

### Ubuntu/Debian:

```bash
sudo apt install -y cscope
```

### Fedora:

```bash
sudo dnf install -y cscope
```

### RHEL/CentOS:

```bash
# RHEL 8+ may need CodeReady/PowerTools repo
sudo dnf config-manager --set-enabled powertools  # or crb on RHEL 9
sudo dnf install -y cscope

# RHEL 7
sudo yum install -y cscope
```

### Arch:

```bash
sudo pacman -S cscope
```

### Verify:

```bash
cscope --version
# Should show version info
```

### If installation fails:

**RHEL "No match for argument: cscope":**
```bash
# Enable EPEL
sudo dnf install -y epel-release
sudo dnf install -y cscope
```

**Building from source:**
```bash
wget https://sourceforge.net/projects/cscope/files/cscope/v15.9/cscope-15.9.tar.gz
tar xzf cscope-15.9.tar.gz
cd cscope-15.9
./configure
make
sudo make install
```

### If you can't install cscope:

Skip Chapter 6. Use grep for cross-referencing:
```bash
# Instead of cscope, use:
grep -rn "\bfunction_name\b" --include="*.c" src/
```

It's slower and less precise, but works.

---

## 4.3 clang-query

clang-query allows AST-level queries — finding code through macros and complex expressions.

### Ubuntu/Debian:

```bash
# clang-query is in clang-tools
sudo apt install -y clang-tools

# Or for newer versions:
sudo apt install -y clang-tools-14  # Adjust version as available
```

### Fedora:

```bash
sudo dnf install -y clang-tools-extra
```

### RHEL/CentOS 8+:

```bash
sudo dnf install -y clang-tools-extra
```

### Arch:

```bash
sudo pacman -S clang
# clang-query is included
```

### Verify:

```bash
clang-query --version
# Should show LLVM version
```

### If installation fails:

**"Unable to locate package clang-tools":**
```bash
# Try alternative package names
apt-cache search clang | grep -i tools
# Might be: clang-tools-12, clang-tools-14, etc.
```

**Version conflicts:**
```bash
# Install specific version that exists
sudo apt install -y clang-tools-$(apt-cache search clang-tools | grep -oP 'clang-tools-\K[0-9]+' | sort -n | tail -1)
```

### If you can't install clang-query:

Skip Chapter 7. clang-query is mainly useful for:
- Finding globals accessed through macros
- Complex AST patterns

grep handles most cases. You lose precision with macro-heavy code.

---

## 4.4 Thread Sanitizer (TSan)

TSan is part of the compiler, not a separate tool. It requires recompiling your code.

### Check if available:

```bash
# TSan requires gcc 4.8+ or clang 3.2+
gcc --version
clang --version
```

### Ubuntu/Debian:

```bash
# TSan is included with gcc and clang
sudo apt install -y gcc clang

# You might also need:
sudo apt install -y libtsan0  # TSan runtime library
```

### Fedora/RHEL:

```bash
sudo dnf install -y gcc clang libasan libtsan
```

### Verify TSan works:

Create a test file `tsan_test.c`:
```c
#include <pthread.h>

int counter = 0;

void* increment(void* arg) {
    for (int i = 0; i < 1000; i++) {
        counter++;  // Data race!
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment, NULL);
    pthread_create(&t2, NULL, increment, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
```

Compile and run:
```bash
gcc -fsanitize=thread -g -o tsan_test tsan_test.c -lpthread
./tsan_test
```

**What you should see:** TSan warning about data race on `counter`.

### If TSan doesn't work:

**"unrecognized option '-fsanitize=thread'":**
Your compiler is too old. Options:
```bash
# Install newer compiler
sudo apt install gcc-10  # Or newer
gcc-10 -fsanitize=thread ...

# Or use clang
clang -fsanitize=thread ...
```

**Runtime error "FATAL: ThreadSanitizer...":**
```bash
# May need runtime library
sudo apt install libtsan0
```

**Kernel restrictions (containers, old kernels):**
TSan needs kernel support. In Docker, you may need `--privileged` or specific capabilities.

### If TSan won't work:

Skip Chapter 8. Alternatives:
- Helgrind (Valgrind): `valgrind --tool=helgrind ./program`
- Manual code review
- Static analysis with Infer

---

## 4.5 Infer (Facebook's Static Analyzer)

Infer finds bugs through static analysis: null pointers, memory leaks, data races.

### Ubuntu/Debian (x86_64):

```bash
# Download latest release
VERSION=1.1.0  # Check https://github.com/facebook/infer/releases for latest
wget https://github.com/facebook/infer/releases/download/v${VERSION}/infer-linux64-v${VERSION}.tar.xz

# Extract
tar xf infer-linux64-v${VERSION}.tar.xz

# Install to /opt
sudo mv infer-linux64-v${VERSION} /opt/infer

# Add to PATH
echo 'export PATH="/opt/infer/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Fedora/RHEL:

Same as Ubuntu — download the binary release.

### Arch:

```bash
# Available in AUR
yay -S infer
# Or use the binary release method above
```

### Verify:

```bash
infer --version
# Should show: Infer version v1.x.x
```

### If installation fails:

**"Permission denied" extracting:**
```bash
# Extract to home directory instead
tar xf infer-linux64-v${VERSION}.tar.xz -C ~/
echo 'export PATH="$HOME/infer-linux64-v'${VERSION}'/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

**ARM64/aarch64 systems:**
Infer doesn't provide ARM64 binaries. Options:
- Build from source (complex, requires OCaml)
- Skip Infer, use other static analyzers
- Use a different machine for Infer analysis

**"infer: command not found" after installation:**
```bash
# Check if binary exists
ls /opt/infer/bin/infer

# Check PATH
echo $PATH | grep infer

# Reload shell
exec bash
```

### If you can't install Infer:

Skip Chapter 9. Alternatives:
- Cppcheck: `sudo apt install cppcheck && cppcheck --enable=all src/`
- clang static analyzer: `scan-build make`
- PVS-Studio (commercial, free for open source)

---

## Installation Summary Script

After attempting installations, run this to see your status:

```bash
#!/bin/bash
echo "=== Installation Summary ==="
echo ""

check() {
    command -v "$1" &>/dev/null && echo "✓ $1" || echo "✗ $1 (not available)"
}

echo "Core tools:"
check doxygen
check dot
check cscope

echo ""
echo "Advanced tools:"
check clang-query
check infer

echo ""
echo "Compiler sanitizers:"
if gcc -fsanitize=thread -x c -c /dev/null -o /dev/null 2>/dev/null; then
    echo "✓ TSan (gcc)"
else
    echo "✗ TSan (gcc)"
fi

if clang -fsanitize=thread -x c -c /dev/null -o /dev/null 2>/dev/null; then
    echo "✓ TSan (clang)"
else
    echo "✗ TSan (clang)"
fi
```

---

# Chapter 5: Doxygen: Call Graphs and Documentation

Doxygen generates visual call graphs showing which functions call which — invaluable for understanding how globals propagate through code.

## What Doxygen Shows You

- **Call graphs:** Function A calls Function B calls Function C
- **Caller graphs:** Which functions call this function?
- **Include graphs:** Which files include which?
- **Documentation:** Extracted from code comments

## Creating a Doxygen Configuration

Doxygen needs a configuration file. Generate a default one:

```bash
cd /path/to/your/project
doxygen -g migration_analysis/doxygen/Doxyfile
```

This creates a `Doxyfile` with default settings.

## Configuring for Migration Analysis

Edit `migration_analysis/doxygen/Doxyfile` to optimize for our use case:

```bash
# Use sed to modify key settings, or edit manually

# Project name
sed -i 's/^PROJECT_NAME.*=.*/PROJECT_NAME = "Migration Analysis"/' migration_analysis/doxygen/Doxyfile

# Input directory (adjust to your source location)
sed -i 's|^INPUT.*=.*|INPUT = src/ include/|' migration_analysis/doxygen/Doxyfile

# Recursive search
sed -i 's/^RECURSIVE.*=.*/RECURSIVE = YES/' migration_analysis/doxygen/Doxyfile

# Extract all (even undocumented)
sed -i 's/^EXTRACT_ALL.*=.*/EXTRACT_ALL = YES/' migration_analysis/doxygen/Doxyfile
sed -i 's/^EXTRACT_STATIC.*=.*/EXTRACT_STATIC = YES/' migration_analysis/doxygen/Doxyfile
sed -i 's/^EXTRACT_PRIVATE.*=.*/EXTRACT_PRIVATE = YES/' migration_analysis/doxygen/Doxyfile

# Generate call graphs
sed -i 's/^HAVE_DOT.*=.*/HAVE_DOT = YES/' migration_analysis/doxygen/Doxyfile
sed -i 's/^CALL_GRAPH.*=.*/CALL_GRAPH = YES/' migration_analysis/doxygen/Doxyfile
sed -i 's/^CALLER_GRAPH.*=.*/CALLER_GRAPH = YES/' migration_analysis/doxygen/Doxyfile

# Output directory
sed -i 's|^OUTPUT_DIRECTORY.*=.*|OUTPUT_DIRECTORY = migration_analysis/doxygen/output|' migration_analysis/doxygen/Doxyfile

# File patterns
sed -i 's/^FILE_PATTERNS.*=.*/FILE_PATTERNS = *.c *.h/' migration_analysis/doxygen/Doxyfile
```

Or create a minimal config file manually:

```bash
cat > migration_analysis/doxygen/Doxyfile << 'EOF'
PROJECT_NAME           = "Migration Analysis"
OUTPUT_DIRECTORY       = migration_analysis/doxygen/output
INPUT                  = src/ include/
FILE_PATTERNS          = *.c *.h
RECURSIVE              = YES
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
EXTRACT_PRIVATE        = YES
HAVE_DOT               = YES
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
INCLUDE_GRAPH          = YES
INCLUDED_BY_GRAPH      = YES
GRAPHICAL_HIERARCHY    = YES
DOT_IMAGE_FORMAT       = svg
INTERACTIVE_SVG        = YES
SOURCE_BROWSER         = YES
REFERENCED_BY_RELATION = YES
REFERENCES_RELATION    = YES
EOF
```

## Running Doxygen

```bash
doxygen migration_analysis/doxygen/Doxyfile
```

**What you should see:** Progress messages as Doxygen processes files.

**Output location:** `migration_analysis/doxygen/output/html/index.html`

## Viewing the Results

```bash
# Open in browser
xdg-open migration_analysis/doxygen/output/html/index.html

# Or on a server, start a simple HTTP server:
cd migration_analysis/doxygen/output/html
python3 -m http.server 8000
# Then open http://localhost:8000 in browser
```

## Using Call Graphs for Migration Analysis

### Finding all functions that access a global:

1. Open the Doxygen HTML output
2. Go to "Files" → select the file containing your global
3. Find the global variable in the file documentation
4. Click on functions that reference it
5. View the "caller graph" for each function

### Example: Tracing g_database usage

1. Search for `g_database` in the documentation
2. Find functions that reference it (e.g., `db_query()`)
3. View `db_query()`'s caller graph
4. See the full chain: `main()` → `handle_request()` → `process_query()` → `db_query()` → accesses `g_database`

This shows you **impact scope** — how far the global's influence reaches.

## Interpreting Call Graphs

```
[main] → [handle_request] → [process_query] → [db_query]
                                                    ↓
                                              g_database
```

**What this tells you:**
- `g_database` is accessed deep in the call chain
- Migrating it affects `db_query`, but callers only need to pass a parameter
- The "blast radius" is contained

**Red flag pattern:**
```
[main] → [func1] → g_config
      → [func2] → g_config
      → [func3] → g_config
      → [func4] → g_config
```

Multiple top-level functions directly accessing a global = high coupling, harder migration.

## Troubleshooting Doxygen

### "dot: command not found"

```bash
sudo apt install graphviz
```

### Graphs not generated

Check `HAVE_DOT = YES` in Doxyfile.

### "Graph too large"

Doxygen limits graph complexity. Add to Doxyfile:
```
DOT_GRAPH_MAX_NODES    = 100
MAX_DOT_GRAPH_DEPTH    = 5
```

### Output is empty

- Check INPUT path is correct
- Check FILE_PATTERNS matches your files
- Check RECURSIVE = YES if files are in subdirectories

---

# Chapter 6: cscope: Cross-Reference Database

cscope builds a database of your code that allows instant queries for symbols, callers, callees, and more.

## Building the cscope Database

```bash
cd /path/to/your/project

# Generate list of source files
find . -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files

# Build database
cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
```

**Flags explained:**
- `-b` — Build database only (don't start interactive mode)
- `-q` — Build inverted index for faster searches
- `-k` — Kernel mode (don't search system headers)
- `-i` — Input file list
- `-f` — Database output file

## Using cscope Interactively

```bash
cscope -d -f migration_analysis/cscope/cscope.out
```

**The cscope menu:**
```
Find this C symbol:
Find this global definition:
Find functions called by this function:
Find functions calling this function:
Find this text string:
Change this text string:
Find this egrep pattern:
Find this file:
Find files #including this file:
```

**Navigation:**
- Use arrow keys to select query type
- Type your search term and press Enter
- Results show file, function, and line
- Press Enter on a result to view in editor
- Press Ctrl+D to exit

## cscope Queries for Migration Analysis

### Find all places a global is used:

```
Find this C symbol: g_database
```

Shows every reference to `g_database` in the codebase.

### Find where a global is defined:

```
Find this global definition: g_database
```

Shows the actual definition (not just declarations).

### Find all callers of a function:

```
Find functions calling this function: db_query
```

If `db_query` accesses `g_database`, this shows all functions that indirectly depend on the global.

### Find what a function calls:

```
Find functions called by this function: handle_request
```

Shows what `handle_request` calls — useful for tracing global access chains.

## Batch Queries with cscope

For scripting, use line-mode:

```bash
# Find all references to g_database
echo "0g_database" | cscope -d -f migration_analysis/cscope/cscope.out -L

# Find definition
echo "1g_database" | cscope -d -f migration_analysis/cscope/cscope.out -L

# Find callers of db_query
echo "3db_query" | cscope -d -f migration_analysis/cscope/cscope.out -L
```

**Query type numbers:**
- 0: Find symbol
- 1: Find global definition
- 2: Find functions called by
- 3: Find functions calling
- 4: Find text string
- 6: Find egrep pattern
- 7: Find file
- 8: Find files #including

## Creating a cscope Analysis Script

Save as `migration_analysis/scripts/cscope_analyze.sh`:

```bash
#!/bin/bash
# cscope_analyze.sh - Analyze globals using cscope
# Usage: ./cscope_analyze.sh <global_name>

GLOBAL="$1"
DB="migration_analysis/cscope/cscope.out"

if [ -z "$GLOBAL" ]; then
    echo "Usage: $0 <global_variable_name>"
    exit 1
fi

if [ ! -f "$DB" ]; then
    echo "cscope database not found. Building..."
    find . -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files
    cscope -b -q -k -i migration_analysis/cscope/cscope.files -f "$DB"
fi

echo "=== Analysis for: $GLOBAL ==="
echo ""

echo "Definition:"
echo "1$GLOBAL" | cscope -d -f "$DB" -L
echo ""

echo "All references:"
echo "0$GLOBAL" | cscope -d -f "$DB" -L | head -20
echo ""

ref_count=$(echo "0$GLOBAL" | cscope -d -f "$DB" -L | wc -l)
file_count=$(echo "0$GLOBAL" | cscope -d -f "$DB" -L | cut -d' ' -f1 | sort -u | wc -l)

echo "Summary:"
echo "  Total references: $ref_count"
echo "  Files involved: $file_count"
```

Run:
```bash
chmod +x migration_analysis/scripts/cscope_analyze.sh
./migration_analysis/scripts/cscope_analyze.sh g_database
```

## Editor Integration

### Vim:

```vim
" Add to ~/.vimrc
set cscopetag
set csto=0

" Load cscope database
if filereadable("migration_analysis/cscope/cscope.out")
    cs add migration_analysis/cscope/cscope.out
endif

" Keybindings
nmap <C-\>s :cs find s <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>g :cs find g <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>c :cs find c <C-R>=expand("<cword>")<CR><CR>
nmap <C-\>d :cs find d <C-R>=expand("<cword>")<CR><CR>
```

### Emacs:

```elisp
;; Add to ~/.emacs
(require 'xcscope)
(cscope-setup)
(setq cscope-database-file "migration_analysis/cscope/cscope.out")
```

### VS Code:

Install the "cscope" extension and configure the database path.

## Troubleshooting cscope

### "cscope: cannot find file..."

Files moved or deleted. Rebuild database:
```bash
find . -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files
cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
```

### "Database format error"

Corrupted database. Delete and rebuild:
```bash
rm migration_analysis/cscope/cscope.*
# Rebuild as above
```

### Large codebase is slow

Use the `-q` flag (inverted index) and exclude test/build directories:
```bash
find . -path ./build -prune -o -path ./test -prune -o \( -name "*.c" -o -name "*.h" \) -print > cscope.files
```

---

# Chapter 7: clang-query: AST-Level Analysis

clang-query searches the Abstract Syntax Tree (AST), finding code that grep misses — especially through macros.

## When You Need clang-query

**Scenario:** Your code uses macros:

```c
#define DB_QUERY(sql) do { \
    pthread_mutex_lock(&g_db_mutex); \
    db_execute(g_database, sql); \
    pthread_mutex_unlock(&g_db_mutex); \
} while(0)
```

grep for `g_database` won't find calls like `DB_QUERY("SELECT...")` because the expansion happens at compile time. clang-query searches the expanded AST.

## Setting Up clang-query

clang-query needs a compilation database to understand your build:

```bash
# If using CMake:
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
ln -s build/compile_commands.json .

# If using Make, use Bear to generate:
sudo apt install bear
bear -- make clean all
# Creates compile_commands.json
```

## Basic clang-query Usage

```bash
# Interactive mode
clang-query src/database.c

# At the clang-query prompt:
clang-query> match varDecl(hasName("g_database"))
```

## Useful Queries for Migration Analysis

### Find all variable declarations with a name:

```
match varDecl(hasName("g_database"))
```

### Find all references to a variable:

```
match declRefExpr(to(varDecl(hasName("g_database"))))
```

### Find static variables at file scope:

```
match varDecl(hasStaticStorageDuration(), unless(hasAncestor(functionDecl())))
```

### Find all global variables:

```
match varDecl(hasGlobalStorage())
```

## Creating a Query Script

Save as `migration_analysis/scripts/clang_analyze.sh`:

```bash
#!/bin/bash
# clang_analyze.sh - Find globals with clang-query
# Requires compile_commands.json

if [ ! -f "compile_commands.json" ]; then
    echo "ERROR: compile_commands.json not found"
    echo ""
    echo "Generate it with:"
    echo "  CMake: cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ."
    echo "  Make:  bear -- make"
    exit 1
fi

GLOBAL="${1:-}"

if [ -z "$GLOBAL" ]; then
    echo "Finding all static file-scope variables..."
    echo ""
    
    # Find all source files
    for src in $(find src -name "*.c"); do
        echo "=== $src ==="
        clang-query -c "match varDecl(hasStaticStorageDuration(), unless(hasAncestor(functionDecl())))" "$src" 2>/dev/null
        echo ""
    done
else
    echo "Finding all references to: $GLOBAL"
    echo ""
    
    for src in $(find src -name "*.c"); do
        result=$(clang-query -c "match declRefExpr(to(varDecl(hasName(\"$GLOBAL\"))))" "$src" 2>/dev/null)
        if [ -n "$result" ]; then
            echo "=== $src ==="
            echo "$result"
            echo ""
        fi
    done
fi
```

## Troubleshooting clang-query

### "error: no compilation database found"

Create compile_commands.json:
```bash
# With bear
bear -- make

# With CMake
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
```

### "fatal error: 'header.h' file not found"

clang-query needs correct include paths. They come from compile_commands.json. Ensure your build works first.

### Slow on large codebase

clang-query parses each file. Run on specific files:
```bash
clang-query src/database.c src/handler.c
```

---

# Chapter 8: Thread Sanitizer: Finding Data Races

Thread Sanitizer (TSan) finds data races at runtime by instrumenting your code.

## How TSan Works

1. You compile your code with `-fsanitize=thread`
2. TSan adds instrumentation to every memory access
3. At runtime, TSan tracks which threads access which memory
4. If two threads access the same memory without synchronization, TSan reports it

## Compiling with TSan

```bash
# With gcc
gcc -fsanitize=thread -g -O1 -o myapp src/*.c -lpthread

# With clang
clang -fsanitize=thread -g -O1 -o myapp src/*.c -lpthread
```

**Important flags:**
- `-fsanitize=thread` — Enable TSan
- `-g` — Debug info (for useful error messages)
- `-O1` — Some optimization (TSan needs it)
- `-lpthread` — Link pthread library

## Running with TSan

```bash
./myapp
```

If there are data races, TSan prints detailed reports:

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x7f8b8c0008a0 by thread T2:
    #0 increment /home/user/src/counter.c:15 (myapp+0x...)
    #1 worker_thread /home/user/src/worker.c:42 (myapp+0x...)

  Previous read of size 4 at 0x7f8b8c0008a0 by thread T1:
    #0 get_count /home/user/src/counter.c:20 (myapp+0x...)
    #1 main /home/user/src/main.c:30 (myapp+0x...)

  Location is global 'request_count' of size 4 at 0x7f8b8c0008a0
==================
```

## Interpreting TSan Output

**Key information:**
- **"data race"** — Two threads accessing same location unsafely
- **Thread numbers** — T1, T2, etc.
- **Stack traces** — Exactly where the accesses occur
- **Location** — The variable involved (often your global!)

**This tells you:**
- Which global has a race
- Which functions access it unsafely
- Which threads are involved

## TSan Analysis Script

Save as `migration_analysis/scripts/tsan_analyze.sh`:

```bash
#!/bin/bash
# tsan_analyze.sh - Build and run with TSan
# Usage: ./tsan_analyze.sh <build_command> <run_command>

BUILD_CMD="${1:-make}"
RUN_CMD="${2:-./myapp}"

echo "Building with Thread Sanitizer..."
echo ""

# Set compiler flags for TSan
export CC="gcc"
export CFLAGS="-fsanitize=thread -g -O1"
export LDFLAGS="-fsanitize=thread -lpthread"

# Clean and build
make clean 2>/dev/null
$BUILD_CMD

if [ $? -ne 0 ]; then
    echo "Build failed. Check your Makefile accepts CC/CFLAGS."
    echo ""
    echo "Manual compilation:"
    echo "  gcc -fsanitize=thread -g -O1 -o myapp src/*.c -lpthread"
    exit 1
fi

echo ""
echo "Running with TSan..."
echo "===================="
echo ""

# Run and capture TSan output
$RUN_CMD 2>&1 | tee migration_analysis/tsan/tsan_report.txt

echo ""
echo "===================="
echo ""

# Parse results
races=$(grep -c "WARNING: ThreadSanitizer: data race" migration_analysis/tsan/tsan_report.txt 2>/dev/null || echo 0)
echo "Data races found: $races"

if [ "$races" -gt 0 ]; then
    echo ""
    echo "Globals with data races:"
    grep "Location is global" migration_analysis/tsan/tsan_report.txt | \
        sed "s/.*global '\([^']*\)'.*/  - \1/" | \
        sort -u
fi

echo ""
echo "Full report: migration_analysis/tsan/tsan_report.txt"
```

Run:
```bash
mkdir -p migration_analysis/tsan
chmod +x migration_analysis/scripts/tsan_analyze.sh
./migration_analysis/scripts/tsan_analyze.sh "make" "./myapp"
```

## Troubleshooting TSan

### "FATAL: ThreadSanitizer: unexpected memory mapping"

Container or VM issue. Try:
```bash
# Increase vm.mmap_rnd_bits
sudo sysctl vm.mmap_rnd_bits=28
```

### Slow execution

TSan adds ~10x overhead. For long-running programs:
- Run shorter test scenarios
- Use TSan only for targeted testing

### Missing stack frames

Ensure `-g` flag is used. Check optimization level isn't too high.

### "Suppressions" for known issues

Create a suppressions file:
```bash
echo "race:third_party_lib" > tsan.supp
TSAN_OPTIONS="suppressions=tsan.supp" ./myapp
```

---

# Chapter 9: Infer: Static Analysis

Infer analyzes your code without running it, finding:
- Null pointer dereferences
- Memory leaks
- Data races (static analysis, not runtime)
- Buffer overflows

## Running Infer

### With Make:

```bash
infer run -- make clean all
```

### With CMake:

```bash
mkdir build && cd build
infer run -- cmake ..
infer run -- make
```

### Direct compilation:

```bash
infer run -- gcc -c src/database.c -o database.o
```

## Understanding Infer Output

Infer creates a report in `infer-out/report.txt`:

```
src/database.c:47: error: NULL_DEREFERENCE
  pointer `conn` last assigned on line 42 could be null and is dereferenced at line 47

src/handler.c:23: error: THREAD_SAFETY_violation
  Read/Write race. Non-private member `g_count` is not protected by a lock.
```

## Infer for Migration Analysis

Infer's THREAD_SAFETY analysis is particularly valuable:

```bash
# Run with thread safety focus
infer run --racerd-only -- make
```

This finds unprotected access to shared variables — exactly what we need for migration analysis.

## Infer Analysis Script

Save as `migration_analysis/scripts/infer_analyze.sh`:

```bash
#!/bin/bash
# infer_analyze.sh - Run Infer static analysis

echo "Running Infer static analysis..."
echo ""

# Check Infer is available
if ! command -v infer &>/dev/null; then
    echo "ERROR: Infer not installed"
    echo "See Chapter 4 for installation instructions"
    exit 1
fi

# Clean previous results
rm -rf infer-out

# Run Infer
infer run -- make clean all 2>&1 | tee migration_analysis/infer/infer_build.log

if [ $? -ne 0 ]; then
    echo ""
    echo "Infer run failed. Trying direct compilation..."
    
    # Try compiling each file directly
    rm -rf infer-out
    for src in src/*.c; do
        echo "Analyzing: $src"
        infer run -- gcc -c "$src" -Iinclude -o /dev/null 2>/dev/null
    done
fi

# Copy results
cp infer-out/report.txt migration_analysis/infer/ 2>/dev/null

echo ""
echo "=== Infer Results ==="
echo ""

if [ -f migration_analysis/infer/report.txt ]; then
    # Count issues by type
    echo "Issues found:"
    echo "  NULL_DEREFERENCE: $(grep -c NULL_DEREFERENCE migration_analysis/infer/report.txt 2>/dev/null || echo 0)"
    echo "  THREAD_SAFETY:    $(grep -c THREAD_SAFETY migration_analysis/infer/report.txt 2>/dev/null || echo 0)"
    echo "  RESOURCE_LEAK:    $(grep -c RESOURCE_LEAK migration_analysis/infer/report.txt 2>/dev/null || echo 0)"
    
    echo ""
    echo "Thread safety issues (relevant for migration):"
    grep -A2 "THREAD_SAFETY" migration_analysis/infer/report.txt 2>/dev/null || echo "  None found"
    
    echo ""
    echo "Full report: migration_analysis/infer/report.txt"
else
    echo "No report generated. Check infer_build.log for errors."
fi
```

## Troubleshooting Infer

### "Build failed"

Infer intercepts compiler calls. Ensure your build works normally first:
```bash
make clean all  # Should succeed without Infer
infer run -- make clean all
```

### "No issues found" (suspicious)

Infer might not be capturing the build:
```bash
# Check capture
infer capture -- make
ls infer-out/captured/  # Should have files
```

### Slow analysis

Infer can be slow on large codebases. Run on specific directories:
```bash
infer run -- gcc -c src/critical_module/*.c
```

---

# Chapter 10: Combining Tools for Complete Analysis

Each tool provides different insights. Here's how to combine them.

## The Analysis Workflow

```
1. grep analysis (from previous manual)
   ↓ List of globals with basic coupling
   
2. Doxygen
   ↓ Visual call graphs showing impact scope
   
3. cscope  
   ↓ Precise caller/callee relationships
   
4. clang-query (if macros hide globals)
   ↓ References through macro expansions
   
5. TSan (if multi-threaded)
   ↓ Proven data races (not guesses)
   
6. Infer (static analysis)
   ↓ Additional thread safety and null issues
   
7. Combined decision matrix
```

## Master Analysis Script

Save as `migration_analysis/scripts/full_analysis.sh`:

```bash
#!/bin/bash
# full_analysis.sh - Run all available analysis tools

set -e

SRC_DIR="${1:-src}"
echo "=========================================="
echo "    Full Migration Analysis"
echo "=========================================="
echo "Source directory: $SRC_DIR"
echo ""

# Create directories
mkdir -p migration_analysis/{reports,doxygen,cscope,clang,tsan,infer}

# 1. Doxygen
if command -v doxygen &>/dev/null; then
    echo "[1/5] Running Doxygen..."
    if [ ! -f migration_analysis/doxygen/Doxyfile ]; then
        doxygen -g migration_analysis/doxygen/Doxyfile >/dev/null
        # Configure for analysis
        sed -i "s|^INPUT.*=.*|INPUT = $SRC_DIR|" migration_analysis/doxygen/Doxyfile
        sed -i 's/^EXTRACT_ALL.*=.*/EXTRACT_ALL = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's/^HAVE_DOT.*=.*/HAVE_DOT = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's/^CALL_GRAPH.*=.*/CALL_GRAPH = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's|^OUTPUT_DIRECTORY.*=.*|OUTPUT_DIRECTORY = migration_analysis/doxygen/output|' migration_analysis/doxygen/Doxyfile
    fi
    doxygen migration_analysis/doxygen/Doxyfile 2>/dev/null
    echo "  Done. View: migration_analysis/doxygen/output/html/index.html"
else
    echo "[1/5] Doxygen: SKIPPED (not installed)"
fi
echo ""

# 2. cscope
if command -v cscope &>/dev/null; then
    echo "[2/5] Building cscope database..."
    find "$SRC_DIR" -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files
    cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
    echo "  Done. Use: cscope -d -f migration_analysis/cscope/cscope.out"
else
    echo "[2/5] cscope: SKIPPED (not installed)"
fi
echo ""

# 3. clang-query check
if command -v clang-query &>/dev/null; then
    echo "[3/5] clang-query: Available"
    if [ -f compile_commands.json ]; then
        echo "  compile_commands.json found"
    else
        echo "  WARNING: compile_commands.json not found"
        echo "  Generate with: bear -- make"
    fi
else
    echo "[3/5] clang-query: SKIPPED (not installed)"
fi
echo ""

# 4. TSan capability check
echo "[4/5] Thread Sanitizer..."
if gcc -fsanitize=thread -x c -c /dev/null -o /dev/null 2>/dev/null; then
    echo "  TSan available (gcc)"
    echo "  Build with: gcc -fsanitize=thread -g -O1 ..."
else
    echo "  TSan: not available with gcc"
fi
echo ""

# 5. Infer
if command -v infer &>/dev/null; then
    echo "[5/5] Infer: Available"
    echo "  Run with: infer run -- make"
else
    echo "[5/5] Infer: SKIPPED (not installed)"
fi
echo ""

echo "=========================================="
echo "Analysis setup complete."
echo ""
echo "Next steps:"
echo "1. Review Doxygen call graphs"
echo "2. Query globals with cscope"
echo "3. Build with TSan and run tests"
echo "4. Run Infer for static analysis"
echo "5. Combine findings into decisions.md"
```

---

# Chapter 11: Worked Example: Full Analysis

Let's analyze a multi-threaded C application using all tools.

## The Example Codebase

```
threadapp/
├── src/
│   ├── main.c
│   ├── server.c
│   ├── worker.c
│   ├── database.c
│   ├── cache.c
│   └── logging.c
├── include/
│   └── *.h
├── Makefile
└── migration_analysis/
```

Known globals from grep analysis:
- `g_config` — Configuration
- `g_database` — Database connection
- `g_cache` — Response cache
- `g_stats` — Request statistics
- `g_db_mutex` — Database lock

## Step 1: Run Doxygen

```bash
./migration_analysis/scripts/full_analysis.sh src
xdg-open migration_analysis/doxygen/output/html/index.html
```

**Findings from call graphs:**

- `g_database` accessed by: `db_query()`, `db_connect()`, `db_close()`
- Callers of `db_query()`: `handle_request()`, `cache_fill()`, `admin_query()`
- Impact scope: 3 direct accessors, 5 indirect callers

## Step 2: Query with cscope

```bash
cscope -d -f migration_analysis/cscope/cscope.out

# Query: Find functions calling db_query
# Results:
# src/worker.c     handle_request    47  result = db_query(sql);
# src/cache.c      cache_fill        23  data = db_query(cache_sql);
# src/admin.c      admin_query       89  db_query(admin_sql);
```

**Findings:**
- `db_query` called from 3 files
- Each caller is in a different module
- Coupling: MEDIUM

## Step 3: Build and Run with TSan

```bash
make clean
gcc -fsanitize=thread -g -O1 -o threadapp src/*.c -Iinclude -lpthread
./threadapp &
# Run some test requests
curl http://localhost:8080/test
kill %1
```

**TSan Output:**
```
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x... by thread T2:
    #0 cache_update src/cache.c:45
    
  Previous read of size 8 at 0x... by thread T3:
    #0 cache_lookup src/cache.c:32
    
  Location is global 'g_cache' of size 8 at 0x...
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T2:
    #0 increment_stats src/stats.c:15
    
  Previous read of size 4 at 0x... by thread T4:
    #0 get_stats src/stats.c:22
    
  Location is global 'g_stats' of size 4 at 0x...
```

**Findings:**
- `g_cache` — DATA RACE (cache_update vs cache_lookup)
- `g_stats` — DATA RACE (increment_stats vs get_stats)
- `g_database` — No race (protected by g_db_mutex)

## Step 4: Run Infer

```bash
infer run -- make clean all
cat infer-out/report.txt
```

**Infer Output:**
```
src/cache.c:45: error: THREAD_SAFETY_violation
  Read/Write race on `g_cache`

src/stats.c:15: error: THREAD_SAFETY_violation  
  Read/Write race on `g_stats`
  
src/database.c:67: error: NULL_DEREFERENCE
  `g_database` could be null when dereferenced
```

**Findings:**
- Confirms TSan races
- Additional issue: `g_database` null dereference possible

## Step 5: Combined Decision Matrix

| Global | Coupling | TSan | Infer | Decision | Priority |
|--------|----------|------|-------|----------|----------|
| g_config | LOW | Clean | Clean | MIGRATE | Low |
| g_database | MEDIUM | Clean | NULL_DEREF | MIGRATE (fix null check) | Medium |
| g_cache | MEDIUM | RACE | RACE | FIX FIRST, then MIGRATE | **High** |
| g_stats | LOW | RACE | RACE | FIX FIRST, then MIGRATE | **High** |
| g_db_mutex | NONE | N/A | Clean | KEEP | — |

## Step 6: Final Report

```markdown
# threadapp Migration Analysis

## Critical Issues (Fix Immediately)

### g_cache - Data Race
- **Evidence:** TSan + Infer both detect race
- **Location:** cache.c lines 32, 45
- **Fix:** Add mutex protection or use concurrent data structure

### g_stats - Data Race  
- **Evidence:** TSan + Infer both detect race
- **Location:** stats.c lines 15, 22
- **Fix:** Use atomic operations (atomic_int)

### g_database - Null Dereference
- **Evidence:** Infer static analysis
- **Location:** database.c line 67
- **Fix:** Add null check before use

## Migration Plan

1. **Week 1:** Fix data races in g_cache, g_stats
2. **Week 2:** Migrate g_stats to ServiceLocator (LOW coupling)
3. **Week 3:** Migrate g_config to ServiceLocator  
4. **Week 4:** Migrate g_database, g_cache to ServiceLocator

## Keep Global
- g_db_mutex (synchronization primitive)

## Evidence Summary
- Doxygen: Call graphs show contained impact scope
- cscope: Verified coupling measurements
- TSan: 2 data races found (runtime proof)
- Infer: Confirmed races + found null issue
```

---

# Chapter 12: Troubleshooting

## General Issues

### "Tool works on small test but fails on real code"

Complex codebases often have:
- Non-standard build systems
- Generated code
- Complex macros

**Solution:** Start with a subset:
```bash
# Analyze one directory at a time
doxygen ... INPUT=src/module1
```

### "Too much output / too many issues"

Prioritize:
1. Thread safety issues (TSan/Infer) — actual bugs
2. High-coupling globals — highest migration value
3. Everything else — later

### "Tools disagree"

Tools use different techniques:
- TSan: runtime detection (100% accurate for executed paths)
- Infer: static analysis (may have false positives)

**If TSan reports a race, it's real.** If only Infer reports it, investigate manually.

## Tool-Specific Issues

See each tool's chapter for specific troubleshooting.

---

# FAQ

**Q: Do I need all these tools?**

A: No. Doxygen + cscope give you 80% of the value. Add TSan if multi-threaded. Add Infer if you want comprehensive static analysis.

**Q: What if I can't install anything?**

A: Use the grep-based manual (UM-MIGRATION-ANALYSIS-001). It's less precise but works everywhere.

**Q: How accurate is TSan?**

A: TSan has no false positives — if it reports a race, the race exists. But it only finds races in executed code paths. Run comprehensive tests.

**Q: Is Infer worth the complexity?**

A: For large codebases, yes. For small projects, Cppcheck (`sudo apt install cppcheck`) is simpler.

**Q: Can I use these tools on C++?**

A: Yes, all these tools work on C++. Some options may differ.

---

# Glossary

**AST:** Abstract Syntax Tree. The compiler's representation of code structure.

**Call graph:** Visual diagram showing which functions call which.

**Data race:** Unsynchronized access to shared data from multiple threads.

**Static analysis:** Analyzing code without running it.

**TSan:** Thread Sanitizer. Runtime tool that detects data races.

---

# Quick Reference

## Tool Installation (Ubuntu/Debian)

```bash
sudo apt install doxygen graphviz cscope clang-tools
# Infer: see Chapter 4
```

## Quick Commands

```bash
# Doxygen
doxygen -g Doxyfile && doxygen Doxyfile

# cscope
find . -name "*.c" -o -name "*.h" > cscope.files
cscope -b -q -k
cscope -d  # interactive

# TSan
gcc -fsanitize=thread -g -O1 -o app src/*.c -lpthread

# Infer  
infer run -- make
```

## Files After Analysis

- `migration_analysis/doxygen/output/html/` — Call graphs
- `migration_analysis/cscope/cscope.out` — Cross-reference DB
- `migration_analysis/tsan/tsan_report.txt` — Race conditions
- `migration_analysis/infer/report.txt` — Static analysis
- `migration_analysis/reports/decisions.md` — Final decisions

---

*FAT-P Library — User Manual UM-MIGRATION-ANALYSIS-004*  
*Open Source Tools Edition (Linux)*  
*Last updated: January 2025*
