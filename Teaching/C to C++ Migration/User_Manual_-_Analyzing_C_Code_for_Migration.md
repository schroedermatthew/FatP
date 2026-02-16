---
doc_id: UM-MIGRATION-ANALYSIS-001
doc_type: "User Manual"
title: "Analyzing C Code for Migration"
fatp_components: ["ServiceLocator"]
topics: ["global state", "C migration", "codebase analysis", "grep", "cross-reference", "thread safety", "dependency injection"]
constraints: ["legacy C code", "minimal tooling", "cross-platform", "no commercial tools"]
cxx_standard: "C++20"
last_verified: "2025-01-09"
audience: ["C developers", "C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# User Manual - Analyzing C Code for Migration

### *Finding global state in C code and deciding what to modernize*

*FAT-P Library — January 2025*

---

## User Manual Card

**What this does:** Helps you find global variables in C code and decide which ones to migrate to modern C++ patterns  
**Time required:** 4-8 hours for a small codebase (<10K lines), 2-5 days for medium (10K-100K lines)  
**Minimum tools needed:** A text editor and `grep` (present on all Unix systems)  
**Skills assumed:** Basic command line usage (cd, ls, running commands)  
**Skills NOT assumed:** Regular expressions, shell scripting, C++ knowledge  
**End result:** A list of global variables with recommendations: migrate, keep as-is, or investigate further  
**Read next:** Migration Guide - Global State to ServiceLocator (when ready to migrate)

---

## Table of Contents

1. [Why Are You Reading This?](#chapter-1-why-are-you-reading-this)
2. [What Is Global State and Why Does It Matter?](#chapter-2-what-is-global-state-and-why-does-it-matter)
3. [Setting Up Your Environment](#chapter-3-setting-up-your-environment)
4. [Finding Global Variables](#chapter-4-finding-global-variables)
5. [Understanding What You Found](#chapter-5-understanding-what-you-found)
6. [Measuring How Tangled Each Global Is](#chapter-6-measuring-how-tangled-each-global-is)
7. [Checking for Thread Safety Issues](#chapter-7-checking-for-thread-safety-issues)
8. [Making Migration Decisions](#chapter-8-making-migration-decisions)
9. [Worked Example: Analyzing a Real Codebase](#chapter-9-worked-example-analyzing-a-real-codebase)
10. [Troubleshooting](#chapter-10-troubleshooting)
11. [FAQ](#faq)
12. [Glossary](#glossary)
13. [Quick Reference](#quick-reference)

---

# Chapter 1: Why Are You Reading This?

## The Situation

You have C code. Maybe it's code you inherited. Maybe it's code your team wrote years ago. Maybe it's a library you depend on. The code works, but it has a problem: **global variables are scattered throughout**, and this causes real pain:

- **Testing is hard.** You can't test one function in isolation because it secretly depends on global state set up somewhere else.
- **Bugs are mysterious.** A function fails, but the cause is a global variable modified by completely unrelated code.
- **Parallelism is dangerous.** Two threads access the same global, and sometimes the program crashes or produces wrong results.
- **Reuse is impossible.** You can't use a module in a new project because it drags in globals that conflict with your existing code.

You've heard that modern C++ has better patterns — dependency injection, service locators, explicit parameter passing — but you don't know which globals to change, which to leave alone, or how to even find them all.

**This manual solves that problem.**

## What You Will Accomplish

By the end of this manual, you will have:

1. **A complete list** of every global variable in your codebase
2. **Coupling scores** showing how tangled each global is (used in 2 files? 20 files?)
3. **Thread safety assessment** for each global
4. **Migration recommendations** based on evidence, not guessing

You will NOT yet have migrated anything. This manual is about **analysis and decision-making**. The actual migration is a separate step (covered in the Migration Guide).

## What This Manual Is NOT

This is not a guide to:
- Learning C or C++ (you need basic familiarity)
- Using advanced static analysis tools (we use simple tools that work everywhere)
- Automated refactoring (we find problems; fixing them is separate)
- Proving your code is correct (we find evidence; judgment is still required)

---

# Chapter 2: What Is Global State and Why Does It Matter?

Before searching for global variables, you need to understand what you're looking for and why it matters.

## What Is a Global Variable?

A **global variable** is data that exists for the entire lifetime of your program and can be accessed from multiple places in your code without being explicitly passed as a parameter.

In C, global variables come in two flavors:

### Flavor 1: File-Scope Static Variables

These are declared with `static` at file scope (outside any function). They can only be accessed within the same `.c` file:

```c
/* In config.c */
static int verbosity_level = 0;      /* Only config.c can see this */
static Config* current_config = NULL; /* Only config.c can see this */

void set_verbosity(int level) {
    verbosity_level = level;  /* Accesses the global */
}
```

The word "static" here means "private to this file" — other files cannot directly access `verbosity_level`. But it's still global state because it persists for the whole program and affects function behavior invisibly.

### Flavor 2: External Linkage Variables

These are declared without `static` at file scope. They can be accessed from any file:

```c
/* In globals.c */
int error_count = 0;           /* Any file can access this */
Database* main_database = NULL; /* Any file can access this */

/* In other_file.c */
extern int error_count;  /* Declaration: "this exists somewhere else" */

void handle_error(void) {
    error_count++;  /* Accesses the global from globals.c */
}
```

External globals are the most dangerous because any code anywhere can read or modify them.

## Why Global State Causes Problems

### Problem 1: Hidden Dependencies

Consider this function:

```c
int calculate_tax(int income) {
    return income * tax_rate / 100;  /* Where does tax_rate come from? */
}
```

Looking at the function signature, you'd think it only depends on `income`. But it secretly depends on a global `tax_rate`. If someone calls this function without setting up `tax_rate` first, they get wrong results or crashes. The dependency is **hidden**.

With explicit parameters, the dependency is **visible**:

```c
int calculate_tax(int income, int tax_rate) {
    return income * tax_rate / 100;  /* Dependency is obvious */
}
```

### Problem 2: Testing Difficulty

To test the first version, you must:
1. Set the global `tax_rate` to a known value
2. Call `calculate_tax`
3. Check the result
4. Reset `tax_rate` so other tests aren't affected

If you forget step 4, your next test might fail mysteriously. If two tests run in parallel, they fight over the global.

The second version is trivial to test: just pass different values.

### Problem 3: Thread Safety

If two threads call this simultaneously:

```c
static int counter = 0;

void increment(void) {
    counter++;  /* NOT thread-safe! */
}
```

Both threads might read `counter` as 0, both add 1, and both write 1 — so the counter ends up at 1 instead of 2. This is called a **data race**, and it causes bugs that appear randomly and are nearly impossible to reproduce.

### Problem 4: Initialization Order

```c
/* In file_a.c */
static Config* config = load_config();  /* When does this run? */

/* In file_b.c */  
static Logger* logger = create_logger(config);  /* What if config isn't ready yet? */
```

In C, the order of static initialization across files is **undefined**. Your code might work on one compiler and crash on another.

## Which Globals Should Be Migrated?

Not all globals are bad. Some should stay global:

| Keep Global | Migrate Away From Global |
|-------------|-------------------------|
| Mutex/lock variables (they coordinate threads) | Configuration that could vary per-context |
| Memory allocator state (bootstrapping problem) | Database connections |
| Truly process-wide state (like locale) | Logger instances |
| One-time initialization flags | Caches that could be per-module |
| Hardware registers / OS handles | User preferences |

**The goal of analysis is to figure out which is which.**

## What Will You Migrate To?

This manual helps you find globals. The actual migration target is typically **ServiceLocator** — a pattern where:

1. Services (like a database connection or logger) are registered in a central registry
2. Code that needs a service asks the registry for it
3. For testing, you can register mock services instead of real ones
4. The dependency is explicit ("I need a logger") rather than hidden

But that's for the Migration Guide. First, let's find the globals.

---

# Chapter 3: Setting Up Your Environment

This chapter ensures you have working tools before you start analysis. **Do not skip this chapter.** Most failures happen because of environment problems, not analysis problems.

## What Tools Do You Need?

Here is the complete list of tools, from essential to optional:

| Tool | Required? | What It Does | If You Don't Have It |
|------|-----------|--------------|---------------------|
| `grep` | **YES** | Searches text in files | Cannot proceed (but you have it — see below) |
| `find` | **YES** | Finds files by name/type | Cannot proceed (but you have it) |
| Text editor | **YES** | View and edit files | Cannot proceed |
| `wc` | **YES** | Counts lines | Cannot proceed (but you have it) |
| `sort`, `uniq` | **YES** | Organize results | Cannot proceed (but you have them) |
| `ripgrep` (`rg`) | No | Faster grep | Use grep (slower but works) |
| `ctags` | No | Indexes symbols | Use grep patterns (less convenient) |
| `cscope` | No | Cross-references code | Use grep patterns (more manual) |
| `doxygen` | No | Generates call graphs | Skip visualization |

**The minimum viable toolset is: grep + find + a text editor.** These exist on every Unix system and in Git Bash on Windows.

## Step 1: Verify Your Essential Tools

Open a terminal and run these commands. Every single one should succeed.

### On Linux or macOS:

```bash
# Check each tool - all should print version info or similar
grep --version
find --version
wc --version
sort --version
uniq --version
```

**What you should see:** Version information for each tool. The exact version doesn't matter for basic usage.

**What to do if a command fails:** This would be extraordinary — these tools are part of POSIX and exist on essentially every Unix system. If one is missing, your system has serious problems. Try: `which grep` to see if it's in a non-standard location.

### On Windows (PowerShell):

```powershell
# PowerShell has built-in equivalents
# No installation needed, but syntax is different

# Test that PowerShell works
$PSVersionTable.PSVersion

# Test Select-String (PowerShell's grep)
"test" | Select-String "test"
```

**What you should see:** Version number for PowerShell (5.1 or higher), and "test" highlighted when you run Select-String.

### On Windows (with Git Bash or MinGW):

If you have Git installed, you have Git Bash, which includes Unix tools:

```bash
# Open Git Bash (not PowerShell, not CMD)
grep --version
find --version
```

**What you should see:** Version info. Git Bash typically includes recent versions of these tools.

## Step 2: Choose Your Environment

Based on what's available, pick your environment:

| Your Situation | Use This | Why |
|----------------|----------|-----|
| Linux or macOS | Native terminal | Everything works |
| Windows with Git installed | Git Bash | Unix tools included |
| Windows without Git | PowerShell | Built-in, syntax differs |
| Windows wanting Unix tools | Install MSYS2 | Full Unix environment |
| Locked-down corporate machine | Whatever works | See troubleshooting |

**For the rest of this manual, I provide commands in two forms:**
1. **Bash** (works on Linux, macOS, Git Bash, MSYS2)
2. **PowerShell** (works on Windows without additional tools)

Pick one and stick with it. Don't mix them.

## Step 3: Install Optional Tools (If Desired)

Optional tools make analysis faster and easier, but you can do everything with just grep. Try to install them; if installation fails, skip them and use the fallback methods documented later.

### Linux (Ubuntu/Debian):

```bash
# Try to install optional tools
sudo apt update
sudo apt install -y ripgrep universal-ctags cscope

# Check what succeeded
echo "=== Installation Results ==="
command -v rg && echo "ripgrep: OK" || echo "ripgrep: FAILED (will use grep)"
command -v ctags && echo "ctags: OK" || echo "ctags: FAILED (will use grep patterns)"
command -v cscope && echo "cscope: OK" || echo "cscope: FAILED (will use grep patterns)"
```

### Linux (RHEL/CentOS/Fedora):

```bash
# RHEL/CentOS often need EPEL repository
sudo dnf install -y epel-release  # Skip if on Fedora
sudo dnf install -y ripgrep ctags cscope

# If ripgrep isn't available:
# Download from https://github.com/BurntSushi/ripgrep/releases
```

### macOS:

```bash
# Using Homebrew (install from https://brew.sh if needed)
brew install ripgrep universal-ctags cscope

# If you don't have Homebrew and can't install it, just use grep
```

### Windows (Chocolatey):

```powershell
# As Administrator
choco install ripgrep universal-ctags

# cscope is harder on Windows - skip it, use grep patterns
```

### Windows (MSYS2):

```bash
# In MSYS2 terminal
pacman -S mingw-w64-ucrt-x86_64-ripgrep mingw-w64-ucrt-x86_64-ctags cscope
```

## Step 4: Verify What You Have

Run this diagnostic script to see exactly what tools are available:

### Bash version (Linux/macOS/Git Bash):

```bash
#!/bin/bash
echo "========================================"
echo "    Code Analysis Environment Check"
echo "========================================"
echo ""

check_tool() {
    local name="$1"
    local cmd="$2"
    local required="$3"
    local fallback="$4"
    
    printf "%-12s: " "$name"
    if command -v "$cmd" &>/dev/null; then
        version=$("$cmd" --version 2>&1 | head -1)
        echo "OK ($version)"
        return 0
    else
        if [ "$required" = "yes" ]; then
            echo "MISSING - REQUIRED"
        else
            echo "missing - will use: $fallback"
        fi
        return 1
    fi
}

echo "Essential tools (must have all):"
echo "--------------------------------"
check_tool "grep" "grep" "yes" ""
check_tool "find" "find" "yes" ""
check_tool "wc" "wc" "yes" ""
check_tool "sort" "sort" "yes" ""

echo ""
echo "Optional tools (faster/easier):"
echo "--------------------------------"
check_tool "ripgrep" "rg" "no" "grep"
check_tool "ctags" "ctags" "no" "grep patterns"
check_tool "cscope" "cscope" "no" "grep patterns"

echo ""
echo "========================================"
echo ""

# Save results
HAVE_RG=$(command -v rg &>/dev/null && echo "yes" || echo "no")
HAVE_CTAGS=$(command -v ctags &>/dev/null && echo "yes" || echo "no")
HAVE_CSCOPE=$(command -v cscope &>/dev/null && echo "yes" || echo "no")

echo "Your analysis profile:"
if [ "$HAVE_RG" = "yes" ] && [ "$HAVE_CTAGS" = "yes" ] && [ "$HAVE_CSCOPE" = "yes" ]; then
    echo "  FULL - All optional tools available"
    echo "  Time estimate: Baseline"
elif [ "$HAVE_RG" = "yes" ] || [ "$HAVE_CTAGS" = "yes" ]; then
    echo "  PARTIAL - Some optional tools available"  
    echo "  Time estimate: 1.5x baseline"
else
    echo "  MINIMAL - Using grep only"
    echo "  Time estimate: 2x baseline (still works fine)"
fi
```

Save this as `check_environment.sh`, run it with `bash check_environment.sh`, and note what profile you have.

### PowerShell version:

```powershell
Write-Host "========================================"
Write-Host "    Code Analysis Environment Check"
Write-Host "========================================"
Write-Host ""

function Test-Tool {
    param($Name, $Command, $Required, $Fallback)
    
    Write-Host -NoNewline ("{0,-12}: " -f $Name)
    
    $cmd = Get-Command $Command -ErrorAction SilentlyContinue
    if ($cmd) {
        Write-Host "OK" -ForegroundColor Green
        return $true
    } else {
        if ($Required) {
            Write-Host "MISSING - REQUIRED" -ForegroundColor Red
        } else {
            Write-Host "missing - will use: $Fallback" -ForegroundColor Yellow
        }
        return $false
    }
}

Write-Host "Essential tools (built into PowerShell):"
Write-Host "-----------------------------------------"
Write-Host "Select-String : OK (PowerShell built-in)"
Write-Host "Get-ChildItem : OK (PowerShell built-in)"  
Write-Host "Measure-Object: OK (PowerShell built-in)"
Write-Host "Sort-Object   : OK (PowerShell built-in)"

Write-Host ""
Write-Host "Optional tools (install separately):"
Write-Host "-------------------------------------"
$hasRg = Test-Tool "ripgrep" "rg" $false "Select-String"
$hasCtags = Test-Tool "ctags" "ctags" $false "Select-String patterns"

Write-Host ""
Write-Host "========================================"

# Git Bash check
Write-Host ""
$gitBash = Test-Path "C:\Program Files\Git\usr\bin\bash.exe"
if ($gitBash) {
    Write-Host "Git Bash: Available (can use bash commands too)" -ForegroundColor Green
} else {
    Write-Host "Git Bash: Not found (PowerShell-only mode)" -ForegroundColor Yellow
}
```

Save as `Check-Environment.ps1`, run with `.\Check-Environment.ps1`.

## Step 5: Set Up Your Analysis Directory

Create a dedicated directory for your analysis work:

### Bash:

```bash
# Navigate to your project
cd /path/to/your/project

# Create analysis directory
mkdir -p migration_analysis/{reports,scripts,temp}

# Verify
ls -la migration_analysis/
```

### PowerShell:

```powershell
# Navigate to your project
cd C:\path\to\your\project

# Create analysis directory  
New-Item -ItemType Directory -Force -Path migration_analysis\reports
New-Item -ItemType Directory -Force -Path migration_analysis\scripts
New-Item -ItemType Directory -Force -Path migration_analysis\temp

# Verify
Get-ChildItem migration_analysis
```

You now have:
- `migration_analysis/reports/` — where analysis results go
- `migration_analysis/scripts/` — where helper scripts go
- `migration_analysis/temp/` — scratch space

## Step 6: Know Your Source Layout

Before analyzing, understand what you're analyzing:

```bash
# How big is the codebase?
find . -name "*.c" -o -name "*.h" | wc -l

# How many lines of code?
find . -name "*.c" -o -name "*.h" | xargs wc -l | tail -1

# Where are the source files?
find . -name "*.c" | head -20
```

**Write down:**
- Source directory: _____________ (e.g., `src/`, `source/`, or `.`)
- Header directory: _____________ (e.g., `include/`, `inc/`, or same as source)
- Approximate file count: _____________
- Approximate line count: _____________

## What If Installation Failed?

If you couldn't install optional tools, that's fine. The rest of this manual provides two versions of every technique:

1. **With optional tools** — faster, more convenient
2. **Grep-only fallback** — slower but always works

Just use the fallback versions. The results are identical; only the speed differs.

## What If You're On a Locked-Down System?

Corporate machines often restrict software installation. Options:

1. **Use what's there.** Grep and find are essentially always available.

2. **Portable binaries.** ripgrep has standalone executables that don't need installation:
   - Download from https://github.com/BurntSushi/ripgrep/releases
   - Put the `rg` executable in your project directory
   - Run as `./rg` instead of `rg`

3. **Run in a container.** If Docker is available:
   ```bash
   docker run -v $(pwd):/src -w /src -it ubuntu:22.04 bash
   apt update && apt install -y ripgrep ctags cscope
   # Now you have all tools
   ```

4. **Use WSL on Windows.** Even locked-down Windows often has WSL enabled:
   ```powershell
   wsl --install  # If not already installed
   # Then use Linux commands in WSL
   ```

---

# Chapter 4: Finding Global Variables

Now we actually search for global variables. This chapter gives you exact commands that work.

## Understanding What We're Searching For

Remember the two types of globals:

1. **Static file-scope:** `static int foo = 0;` at file scope
2. **External linkage:** `int foo = 0;` at file scope (no `static`)

We'll search for both.

## Method 1: Finding Static Globals

Static globals start with `static` at the beginning of a line (not indented inside a function).

### Bash command:

```bash
# Search for static declarations at line start
grep -rn "^static\s" --include="*.c" src/
```

Let me explain this command piece by piece:
- `grep` — the search tool
- `-r` — recursive (search subdirectories)
- `-n` — show line numbers
- `"^static\s"` — the pattern (explained below)
- `--include="*.c"` — only search `.c` files
- `src/` — the directory to search (adjust to your source directory)

**The pattern `^static\s` means:**
- `^` — start of line
- `static` — literal text "static"
- `\s` — followed by whitespace (space or tab)

### PowerShell command:

```powershell
Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern "^static\s"
```

### What you'll see:

```
src/config.c:15:static int verbose_mode = 0;
src/config.c:16:static Config* g_config = NULL;
src/database.c:8:static Connection* db_conn = NULL;
src/utils.c:22:static char error_buffer[256];
```

Each line shows: `filename:line_number:matching_line`

### Problem: This matches too much

The above also matches **static functions** (which aren't global variables):

```
src/config.c:42:static void helper_function(void) {
```

We need to filter these out.

### Refined search (Bash):

```bash
# Find static declarations, exclude things that look like functions
grep -rn "^static\s" --include="*.c" src/ | grep -v "static\s\+\w\+\s*("
```

The second grep removes lines containing `static <word> (` which are function definitions.

**Better yet, use this more precise pattern:**

```bash
# Match: static <type> <name> = or static <type> <name>;
# This catches variable declarations more accurately
grep -rn "^static\s\+[a-zA-Z_][a-zA-Z0-9_]*\s" --include="*.c" src/ | \
    grep -v "^static\s\+\(inline\|void\|int\|char\|unsigned\|const\)\s\+[a-zA-Z_][a-zA-Z0-9_]*\s*(" 
```

This is getting complex. Let's use a script instead.

### Practical script for finding static globals (Bash):

Save this as `migration_analysis/scripts/find_static_globals.sh`:

```bash
#!/bin/bash
# find_static_globals.sh - Find static global variables in C code
# Usage: ./find_static_globals.sh <source_directory>

SRC_DIR="${1:-.}"
OUTPUT="migration_analysis/reports/static_globals.txt"

echo "Searching for static globals in: $SRC_DIR"
echo "Output: $OUTPUT"
echo ""

# Find all static declarations at file scope
grep -rn "^static\s" --include="*.c" "$SRC_DIR" | \
    # Remove static functions (have parentheses before = or ;)
    grep -v "^static.*(.*).*{" | \
    grep -v "^static\s\+inline\s" | \
    # Remove lines that are clearly function definitions
    grep -v "^static\s\+void\s\+[a-z].*(" | \
    grep -v "^static\s\+int\s\+[a-z].*(" | \
    grep -v "^static\s\+char\s\+\*\?[a-z].*(" | \
    grep -v "^static\s\+const\s\+.*(" | \
    # Keep only lines with = or ; (declarations/definitions)
    grep "[=;]" \
    > "$OUTPUT"

COUNT=$(wc -l < "$OUTPUT")
echo "Found $COUNT potential static globals"
echo ""
echo "First 10 results:"
head -10 "$OUTPUT"
echo ""
echo "Review full results in: $OUTPUT"
```

Run it:

```bash
chmod +x migration_analysis/scripts/find_static_globals.sh
./migration_analysis/scripts/find_static_globals.sh src
```

### Practical script for finding static globals (PowerShell):

Save as `migration_analysis/scripts/Find-StaticGlobals.ps1`:

```powershell
# Find-StaticGlobals.ps1 - Find static global variables in C code
# Usage: .\Find-StaticGlobals.ps1 -SourceDir src

param(
    [string]$SourceDir = ".",
    [string]$OutputFile = "migration_analysis\reports\static_globals.txt"
)

Write-Host "Searching for static globals in: $SourceDir"
Write-Host "Output: $OutputFile"
Write-Host ""

# Find all static declarations at file scope
$results = Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse | 
    Select-String -Pattern "^static\s" |
    Where-Object { 
        $line = $_.Line
        # Exclude function definitions
        $line -notmatch "static\s+(inline|void|int|char|unsigned)\s+\w+\s*\(" -and
        # Must have = or ; (variable declaration)
        $line -match "[=;]"
    }

# Save results
$results | ForEach-Object { $_.ToString() } | Out-File $OutputFile -Encoding UTF8

$count = ($results | Measure-Object).Count
Write-Host "Found $count potential static globals"
Write-Host ""
Write-Host "First 10 results:"
$results | Select-Object -First 10 | ForEach-Object { Write-Host $_.ToString() }
Write-Host ""
Write-Host "Review full results in: $OutputFile"
```

Run it:

```powershell
.\migration_analysis\scripts\Find-StaticGlobals.ps1 -SourceDir src
```

## Method 2: Finding External Globals

External globals are harder to find because they don't have a distinctive keyword like `static`. They're just declarations at file scope without `static`.

### Strategy: Find extern declarations in headers

Header files often declare external globals with `extern`:

```bash
# Find extern declarations
grep -rn "^extern\s" --include="*.h" include/
```

This shows you what globals are **exported** for use by other files.

### Strategy: Find the definitions

The `extern` declarations tell other files "this variable exists somewhere." The actual definition is in a `.c` file:

```bash
# Find where extern variables are defined
# First, extract variable names from extern declarations
grep -rn "^extern\s" --include="*.h" include/ | \
    grep -oE "extern\s+[a-zA-Z_][a-zA-Z0-9_\s\*]+\s+([a-zA-Z_][a-zA-Z0-9_]*)" | \
    grep -oE "[a-zA-Z_][a-zA-Z0-9_]*$" | \
    sort -u
```

This extracts the variable names. Then search for where each is defined.

### Practical script for finding external globals (Bash):

Save as `migration_analysis/scripts/find_extern_globals.sh`:

```bash
#!/bin/bash
# find_extern_globals.sh - Find external linkage globals
# Usage: ./find_extern_globals.sh <source_directory> <header_directory>

SRC_DIR="${1:-.}"
HDR_DIR="${2:-$SRC_DIR}"
OUTPUT="migration_analysis/reports/extern_globals.txt"

echo "Searching for extern globals"
echo "Source: $SRC_DIR"
echo "Headers: $HDR_DIR"
echo "Output: $OUTPUT"
echo ""

# Clear output
> "$OUTPUT"

# Step 1: Find extern declarations in headers
echo "=== Extern Declarations (from headers) ===" >> "$OUTPUT"
grep -rn "^extern\s" --include="*.h" "$HDR_DIR" >> "$OUTPUT" 2>/dev/null

echo "" >> "$OUTPUT"
echo "=== Potential External Global Definitions (from source) ===" >> "$OUTPUT"

# Step 2: Find file-scope declarations without static
# This is tricky - we look for lines that:
# - Start with a type (not indented)
# - Don't have 'static'
# - Have an '=' or ';' (declaration/definition)
# - Don't look like functions

grep -rn "^[a-zA-Z_][a-zA-Z0-9_]*\s\+[a-zA-Z_][a-zA-Z0-9_\*]*\s*[=;]" --include="*.c" "$SRC_DIR" | \
    grep -v "^static\|^\s" | \
    grep -v "return\|if\|while\|for\|switch" >> "$OUTPUT" 2>/dev/null

COUNT=$(grep -c "^[^=]" "$OUTPUT")
echo "Found approximately $COUNT extern-related items"
echo "Review: $OUTPUT"
echo ""
echo "NOTE: This has more false positives than static globals."
echo "Manual review required."
```

## Method 3: Using ctags (If Available)

If you installed ctags, it can extract all variables more accurately than grep:

```bash
# Check if ctags is available
if command -v ctags &>/dev/null; then
    # Generate tags with extra information
    ctags --fields=+niazS --extras=+q -R \
        --languages=C \
        -f migration_analysis/temp/tags \
        src/
    
    # Extract variables (kind = 'v')
    grep "\tv\t" migration_analysis/temp/tags | \
        cut -f1,2 | \
        sort > migration_analysis/reports/ctags_variables.txt
    
    echo "Variables found by ctags:"
    wc -l migration_analysis/reports/ctags_variables.txt
else
    echo "ctags not available - using grep methods instead"
fi
```

## Creating Your Global Inventory

Now let's combine the results into a useful inventory.

### Create the inventory script (Bash):

Save as `migration_analysis/scripts/create_inventory.sh`:

```bash
#!/bin/bash
# create_inventory.sh - Create a global variable inventory
# Usage: ./create_inventory.sh <source_directory>

SRC_DIR="${1:-.}"
REPORT_DIR="migration_analysis/reports"
INVENTORY="$REPORT_DIR/global_inventory.md"

echo "Creating global variable inventory..."
echo ""

# Run the finding scripts first
./migration_analysis/scripts/find_static_globals.sh "$SRC_DIR" 2>/dev/null
./migration_analysis/scripts/find_extern_globals.sh "$SRC_DIR" 2>/dev/null

# Create inventory markdown
cat > "$INVENTORY" << 'EOF'
# Global Variable Inventory

Generated: $(date)

## Summary

| Category | Count |
|----------|-------|
EOF

STATIC_COUNT=$(wc -l < "$REPORT_DIR/static_globals.txt" 2>/dev/null || echo 0)
EXTERN_COUNT=$(grep -c "extern" "$REPORT_DIR/extern_globals.txt" 2>/dev/null || echo 0)

echo "| Static globals | $STATIC_COUNT |" >> "$INVENTORY"
echo "| Extern globals | ~$EXTERN_COUNT |" >> "$INVENTORY"

cat >> "$INVENTORY" << 'EOF'

## Static Globals (file-scope, internal linkage)

These can only be accessed within their own .c file, but still cause testing
and maintenance problems.

```
EOF

head -50 "$REPORT_DIR/static_globals.txt" >> "$INVENTORY"

cat >> "$INVENTORY" << 'EOF'
```

(See static_globals.txt for complete list)

## Extern Globals (external linkage)

These can be accessed from any file and are the highest risk.

```
EOF

head -50 "$REPORT_DIR/extern_globals.txt" >> "$INVENTORY"

cat >> "$INVENTORY" << 'EOF'
```

(See extern_globals.txt for complete list)

## Next Steps

1. Review each global in the lists above
2. For each global, measure coupling (see Chapter 6)
3. Check thread safety (see Chapter 7)
4. Make migration decisions (see Chapter 8)
EOF

echo "Inventory created: $INVENTORY"
echo ""
echo "Summary:"
echo "  Static globals: $STATIC_COUNT"
echo "  Extern globals: ~$EXTERN_COUNT"
```

Run everything:

```bash
chmod +x migration_analysis/scripts/*.sh
./migration_analysis/scripts/create_inventory.sh src
```

## Verifying Your Results

Before proceeding, sanity-check your results:

### Are results reasonable?

```bash
# How many globals did you find?
wc -l migration_analysis/reports/static_globals.txt

# Compare to codebase size
total_lines=$(find src -name "*.c" | xargs wc -l | tail -1 | awk '{print $1}')
global_count=$(wc -l < migration_analysis/reports/static_globals.txt)

echo "Codebase: $total_lines lines"
echo "Static globals: $global_count"
echo "Ratio: 1 global per $(( total_lines / global_count )) lines"
```

**Typical ratios:**
- Well-designed code: 1 global per 500-2000 lines
- Average legacy code: 1 global per 100-500 lines
- Global-heavy code: 1 global per 20-100 lines

If you found 0 globals in a large codebase, something went wrong. If you found thousands, either the codebase is very large or the search is too broad.

### Spot-check a few results

Open your source files and verify a few of the reported globals actually exist:

```bash
# Pick a random result
shuf migration_analysis/reports/static_globals.txt | head -1

# Look at it in context
# (this shows the file and line number - go look at it)
```

Does the line actually contain a global variable? If not, refine your search patterns.

---

# Chapter 5: Understanding What You Found

You now have lists of globals. But what do they mean? This chapter helps you understand and categorize your findings.

## Reading the Results

Each line in your results looks like:

```
src/database.c:47:static Connection* db_connection = NULL;
```

This tells you:
- **File:** `src/database.c`
- **Line:** 47
- **Content:** `static Connection* db_connection = NULL;`

## Categorizing Your Globals

Not all globals are equal. Let's categorize them:

### Category 1: Configuration / Settings

```c
static int log_level = LOG_INFO;
static bool debug_mode = false;
static char config_path[256] = "/etc/app.conf";
```

**Characteristics:** Set once at startup, read many times  
**Risk level:** Medium  
**Migration candidate:** Yes — these work well in ServiceLocator

### Category 2: Singleton Resources

```c
static Database* g_database = NULL;
static Logger* g_logger = NULL;
static Cache* g_cache = NULL;
```

**Characteristics:** Initialized once, used throughout program  
**Risk level:** High (thread safety, testing difficulty)  
**Migration candidate:** Yes — primary target for ServiceLocator

### Category 3: Counters and Statistics

```c
static int request_count = 0;
static int error_count = 0;
static size_t bytes_processed = 0;
```

**Characteristics:** Modified frequently, read for reporting  
**Risk level:** High (thread safety if concurrent)  
**Migration candidate:** Maybe — depends on access patterns

### Category 4: Caches and Lookup Tables

```c
static HashTable* name_cache = NULL;
static int prime_table[1000];
```

**Characteristics:** Built once or lazily, read many times  
**Risk level:** Medium  
**Migration candidate:** Depends on whether multiple instances make sense

### Category 5: Synchronization Primitives

```c
static pthread_mutex_t global_lock = PTHREAD_MUTEX_INITIALIZER;
static atomic_int shutdown_flag = 0;
```

**Characteristics:** Coordinates threads  
**Risk level:** LOW — these NEED to be global  
**Migration candidate:** No — keep these global

### Category 6: Bootstrap / Initialization State

```c
static bool initialized = false;
static int init_error = 0;
```

**Characteristics:** Track one-time initialization  
**Risk level:** Low  
**Migration candidate:** Usually no — bootstrapping problem

### Create a categorization worksheet

Save as `migration_analysis/reports/categorization.md`:

```markdown
# Global Variable Categorization

Review each global and assign it to a category.

| Global | File | Category | Risk | Migrate? | Notes |
|--------|------|----------|------|----------|-------|
| db_connection | database.c | Singleton | High | Yes | Used everywhere |
| log_level | config.c | Config | Medium | Yes | Set at startup |
| global_lock | sync.c | Mutex | Low | No | Must stay global |
| | | | | | |

## Category Legend

- **Config**: Configuration values, set once
- **Singleton**: Single-instance resources (DB, logger)  
- **Counter**: Statistics and counters
- **Cache**: Lookup tables and caches
- **Mutex**: Synchronization primitives
- **Bootstrap**: Initialization flags

## Risk Legend

- **High**: Thread safety issues likely, hard to test
- **Medium**: Some testing difficulty
- **Low**: Probably fine as-is

## Migration Legend

- **Yes**: Good candidate for ServiceLocator
- **No**: Should remain global
- **Maybe**: Need more information
```

Fill this in as you analyze each global. You don't need to do them all at once — start with the ones that look most problematic.

---

# Chapter 6: Measuring How Tangled Each Global Is

A global used in 2 files is easy to migrate. A global used in 50 files is a major undertaking. This chapter measures **coupling** — how tangled each global is with the rest of the codebase.

## What Is Coupling?

**Coupling** measures how many parts of the code depend on a global. Higher coupling means:
- More places to change during migration
- More risk of breaking something
- More testing required
- Harder to understand the code

## Measuring Coupling: The File Count

The simplest coupling metric: **how many files reference this global?**

### Bash command:

```bash
# Count files that reference a specific global
GLOBAL="db_connection"
grep -rl "\b${GLOBAL}\b" --include="*.c" src/ | wc -l
```

This shows you how many .c files contain the word `db_connection`.

### PowerShell command:

```powershell
$Global = "db_connection"
(Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern "\b$Global\b" | 
    Select-Object -ExpandProperty Path -Unique).Count
```

## Measuring Coupling: The Reference Count

File count tells you spread; reference count tells you intensity:

```bash
# Count total references to a global
GLOBAL="db_connection"
grep -rn "\b${GLOBAL}\b" --include="*.c" src/ | wc -l
```

## Coupling Analysis Script

Let's automate this for all your globals.

### Bash version:

Save as `migration_analysis/scripts/analyze_coupling.sh`:

```bash
#!/bin/bash
# analyze_coupling.sh - Measure coupling for each global
# Usage: ./analyze_coupling.sh <source_directory>

SRC_DIR="${1:-.}"
GLOBALS_FILE="migration_analysis/reports/static_globals.txt"
OUTPUT="migration_analysis/reports/coupling_analysis.md"

echo "Analyzing coupling for each global..."
echo ""

# Create output header
cat > "$OUTPUT" << EOF
# Coupling Analysis

Generated: $(date)

## Coupling Scores

| Global | Files | References | Coupling Level |
|--------|-------|------------|----------------|
EOF

# Extract global variable names from the globals file
# This gets the variable name from lines like:
# src/file.c:10:static int my_var = 0;

extract_varname() {
    # Try to extract the variable name from a static declaration
    echo "$1" | sed -n 's/.*static\s\+[a-zA-Z_][a-zA-Z0-9_\*\s]*\s\+\*\?\([a-zA-Z_][a-zA-Z0-9_]*\)\s*[=;[].*/\1/p'
}

# Process each global
while IFS= read -r line; do
    # Skip empty lines
    [ -z "$line" ] && continue
    
    # Extract file and variable name
    file=$(echo "$line" | cut -d: -f1)
    varname=$(extract_varname "$line")
    
    # Skip if we couldn't extract a name
    [ -z "$varname" ] && continue
    
    # Count files containing this global
    file_count=$(grep -rl "\b${varname}\b" --include="*.c" "$SRC_DIR" 2>/dev/null | wc -l)
    
    # Count total references
    ref_count=$(grep -rn "\b${varname}\b" --include="*.c" "$SRC_DIR" 2>/dev/null | wc -l)
    
    # Determine coupling level
    if [ "$file_count" -le 1 ]; then
        level="NONE (file-local)"
    elif [ "$file_count" -le 3 ]; then
        level="LOW"
    elif [ "$file_count" -le 10 ]; then
        level="MEDIUM"
    else
        level="HIGH"
    fi
    
    # Output
    echo "| $varname | $file_count | $ref_count | $level |" >> "$OUTPUT"
    
    # Progress indicator
    echo -n "."
    
done < "$GLOBALS_FILE"

echo ""
echo ""

# Add summary section
cat >> "$OUTPUT" << 'EOF'

## Coupling Level Guide

| Level | Files | Meaning | Migration Effort |
|-------|-------|---------|------------------|
| NONE | 1 | Only used in declaring file | Trivial |
| LOW | 2-3 | Used in a few related files | Easy (hours) |
| MEDIUM | 4-10 | Used across a module | Moderate (days) |
| HIGH | 11+ | Used throughout codebase | Significant (weeks) |

## Recommendations

- **Start with LOW coupling globals** — quick wins, build confidence
- **MEDIUM coupling** — do these second, one at a time
- **HIGH coupling** — plan carefully, may need facade pattern
- **NONE** — might not need migration; consider if testing benefits justify it

## Next Steps

1. Sort by coupling level (LOW first)
2. For LOW coupling: proceed to migration
3. For MEDIUM/HIGH: check thread safety first (Chapter 7)
4. Make final decisions (Chapter 8)
EOF

echo "Coupling analysis complete: $OUTPUT"
echo ""

# Show summary
echo "Summary:"
echo "--------"
grep -c "LOW" "$OUTPUT" | xargs echo "LOW coupling:"
grep -c "MEDIUM" "$OUTPUT" | xargs echo "MEDIUM coupling:"
grep -c "HIGH" "$OUTPUT" | xargs echo "HIGH coupling:"
```

### PowerShell version:

Save as `migration_analysis/scripts/Analyze-Coupling.ps1`:

```powershell
# Analyze-Coupling.ps1 - Measure coupling for each global
param(
    [string]$SourceDir = ".",
    [string]$GlobalsFile = "migration_analysis\reports\static_globals.txt",
    [string]$OutputFile = "migration_analysis\reports\coupling_analysis.md"
)

Write-Host "Analyzing coupling for each global..."
Write-Host ""

# Read globals file
$globals = Get-Content $GlobalsFile -ErrorAction SilentlyContinue
if (-not $globals) {
    Write-Host "No globals file found. Run Find-StaticGlobals.ps1 first." -ForegroundColor Red
    exit 1
}

# Create output
$output = @"
# Coupling Analysis

Generated: $(Get-Date)

## Coupling Scores

| Global | Files | References | Coupling Level |
|--------|-------|------------|----------------|
"@

foreach ($line in $globals) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    
    # Extract variable name
    if ($line -match "static\s+\w+[\s\*]+(\w+)\s*[=;\[]") {
        $varname = $matches[1]
    } else {
        continue
    }
    
    # Count files containing this global
    $refs = Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse -ErrorAction SilentlyContinue |
        Select-String -Pattern "\b$varname\b" -ErrorAction SilentlyContinue
    
    $fileCount = ($refs | Select-Object -ExpandProperty Path -Unique | Measure-Object).Count
    $refCount = ($refs | Measure-Object).Count
    
    # Determine coupling level
    $level = switch ($fileCount) {
        { $_ -le 1 } { "NONE (file-local)" }
        { $_ -le 3 } { "LOW" }
        { $_ -le 10 } { "MEDIUM" }
        default { "HIGH" }
    }
    
    $output += "`n| $varname | $fileCount | $refCount | $level |"
    Write-Host -NoNewline "."
}

# Add guide sections
$output += @"

## Coupling Level Guide

| Level | Files | Meaning | Migration Effort |
|-------|-------|---------|------------------|
| NONE | 1 | Only used in declaring file | Trivial |
| LOW | 2-3 | Used in a few related files | Easy (hours) |
| MEDIUM | 4-10 | Used across a module | Moderate (days) |
| HIGH | 11+ | Used throughout codebase | Significant (weeks) |

## Recommendations

- **Start with LOW coupling globals** — quick wins
- **MEDIUM coupling** — do these second
- **HIGH coupling** — plan carefully
"@

$output | Out-File $OutputFile -Encoding UTF8

Write-Host ""
Write-Host "Coupling analysis complete: $OutputFile"
```

## Interpreting Your Coupling Results

After running the script, you'll have a table like:

| Global | Files | References | Coupling Level |
|--------|-------|------------|----------------|
| db_connection | 12 | 47 | HIGH |
| log_level | 3 | 8 | LOW |
| g_cache | 7 | 23 | MEDIUM |
| mutex | 15 | 31 | HIGH |

**Reading this:**

- `db_connection` is used in 12 files with 47 total uses — HIGH coupling means migrating this will touch many files
- `log_level` is only in 3 files — LOW coupling, easy migration target
- `mutex` is HIGH but should NOT be migrated (it's a synchronization primitive)

**Priority order for migration:**
1. LOW coupling + should migrate → do these first
2. MEDIUM coupling + should migrate → do these second
3. HIGH coupling + should migrate → do these last, carefully
4. Any coupling + should NOT migrate → leave alone

---

# Chapter 7: Checking for Thread Safety Issues

If your code is multi-threaded (or might become multi-threaded), globals are dangerous. This chapter helps you find thread safety problems.

## Do You Need This Chapter?

**Yes if:**
- Your code creates threads (`pthread_create`, `CreateThread`, `std::thread`)
- Your code might be called from multiple threads
- Your code is a library that others might use in threaded contexts
- You're not sure

**Probably no if:**
- Simple single-threaded command-line tool
- Script-like code that runs once and exits
- You're certain about single-threaded context

When in doubt, do this analysis. Thread bugs are the worst kind of bugs.

## Finding Threading Indicators

First, let's see if the code uses threads at all:

```bash
# Look for thread-related patterns
echo "=== Thread Usage Indicators ==="

echo "POSIX threads:"
grep -rn "pthread_create\|pthread_mutex\|pthread_cond" --include="*.c" --include="*.h" src/ | wc -l

echo "C11 threads:"
grep -rn "thrd_create\|mtx_lock\|cnd_wait" --include="*.c" --include="*.h" src/ | wc -l

echo "Windows threads:"
grep -rn "CreateThread\|CRITICAL_SECTION\|InitializeCriticalSection" --include="*.c" --include="*.h" src/ | wc -l

echo "Atomic operations:"
grep -rn "atomic_\|__atomic\|InterlockedIncrement" --include="*.c" --include="*.h" src/ | wc -l
```

If all these return 0, the code probably doesn't use threads directly. But it might still be called from threaded contexts.

## Finding Unprotected Globals

A global accessed from multiple threads without synchronization is a **data race**. Let's find candidates:

### Step 1: Find globals that look thread-unsafe

```bash
# Globals without 'atomic' or 'mutex' nearby
# This is a heuristic - not perfect

for global in $(cat migration_analysis/reports/static_globals.txt | \
    sed -n 's/.*static\s\+[a-zA-Z_][a-zA-Z0-9_\*\s]*\s\+\*\?\([a-zA-Z_][a-zA-Z0-9_]*\)\s*[=;[].*/\1/p'); do
    
    # Check if this global appears near mutex operations
    near_mutex=$(grep -rn -B5 -A5 "\b${global}\b" --include="*.c" src/ | \
        grep -i "mutex\|lock\|atomic\|critical" | wc -l)
    
    if [ "$near_mutex" -eq 0 ]; then
        echo "UNPROTECTED: $global"
    else
        echo "protected: $global"
    fi
done
```

### Step 2: Manual review checklist

For each global marked UNPROTECTED, ask:

1. **Is it read-only after initialization?**
   - Set once at startup, never modified → probably safe
   - Modified during operation → needs protection

2. **Is it only accessed from one thread?**
   - Only the main thread uses it → safe but fragile
   - Multiple threads → needs protection

3. **Is the type naturally atomic?**
   - Aligned pointer/int reads are atomic on most platforms → might be safe for flags
   - Compound operations (read-modify-write) are NOT atomic → needs protection
   - When in doubt, assume NOT safe

## Creating a Thread Safety Assessment

Save as `migration_analysis/reports/thread_safety.md`:

```markdown
# Thread Safety Assessment

Generated: [date]

## Threading Context

- [ ] Code creates threads directly
- [ ] Code is called from threaded contexts
- [ ] Code is a library (might be used in threads)
- [ ] Unknown (assume threaded)

## Assessment per Global

| Global | Multi-thread Access? | Protected? | Assessment |
|--------|---------------------|------------|------------|
| db_connection | Yes (called from request handlers) | No | UNSAFE |
| log_level | Read-only after init | N/A | SAFE |
| request_count | Yes (incremented per request) | No | UNSAFE |
| global_mutex | N/A (is the protection) | N/A | KEEP |

## Assessment Guide

- **SAFE**: Read-only, or single-threaded access
- **UNSAFE**: Multi-thread access without protection — FIX BEFORE MIGRATING
- **KEEP**: Synchronization primitive — do not migrate

## Unsafe Globals Requiring Attention

List globals that are UNSAFE and explain the issue:

### db_connection

**Problem:** Global database connection accessed from multiple request handler threads without synchronization.

**Evidence:** 
- `request_handler.c` accesses `db_connection` (line 47)
- `admin_handler.c` accesses `db_connection` (line 82)
- No mutex visible in either location

**Risk:** Corrupted queries, crashes, wrong results

**Fix options:**
1. Add mutex protection (quick fix)
2. Connection pool (better)
3. Per-thread connection (best, natural fit for ServiceLocator)

### request_count

**Problem:** Counter incremented from multiple threads using `request_count++` which is not atomic.

**Evidence:**
- `request_handler.c:51`: `request_count++;`
- Multiple threads execute this code path

**Risk:** Lost increments (counter shows 95 when 100 requests occurred)

**Fix options:**
1. Atomic type: `static atomic_int request_count = 0;`
2. Mutex protection (heavier)
```

## What To Do With Thread-Unsafe Globals

**Before migration, you must fix thread safety issues.** Options:

1. **Add synchronization (mutex/atomic)**
   - Quick fix
   - Keep the global but make it safe
   - Migrate later when protected

2. **Migrate to thread-local storage**
   - Each thread gets its own copy
   - Good for caches, buffers
   - `_Thread_local` (C11) or `__thread` (GCC)

3. **Migrate to ServiceLocator with per-context instances**
   - The proper long-term fix
   - Each context (request, session, etc.) gets its own instance
   - No sharing, no races

For this manual, just **document the issues**. Fixing is done during migration.

---

# Chapter 8: Making Migration Decisions

You now have:
- List of all globals
- Categories (config, singleton, mutex, etc.)
- Coupling scores (LOW, MEDIUM, HIGH)
- Thread safety assessment

Now we make decisions.

## The Decision Framework

For each global, answer these questions in order:

```
Q1: Is it a synchronization primitive (mutex, atomic flag, condition variable)?
    → YES: DO NOT MIGRATE. Keep global.
    → NO: Continue to Q2

Q2: Is it bootstrap/initialization state (one-time init flags, allocator state)?
    → YES: Probably DO NOT MIGRATE. These need to be global.
    → NO: Continue to Q3

Q3: Does it have known thread safety issues?
    → YES: FIX FIRST, then re-evaluate
    → NO: Continue to Q4

Q4: What is the coupling level?
    → NONE/LOW: Good migration candidate. Effort: hours.
    → MEDIUM: Migration candidate if benefits justify. Effort: days.
    → HIGH: Consider partial migration or facade. Effort: weeks.

Q5: What are the benefits of migration?
    - Better testability?
    - Enable multiple instances?
    - Clearer dependencies?
    → If benefits outweigh effort: MIGRATE
    → If not: KEEP GLOBAL (for now)
```

## Decision Matrix Template

Fill this out for your codebase. Save as `migration_analysis/reports/decisions.md`:

```markdown
# Migration Decisions

## Decision Summary

| Global | Decision | Priority | Effort | Rationale |
|--------|----------|----------|--------|-----------|
| db_connection | MIGRATE | High | Days | Testing, thread safety |
| log_level | MIGRATE | Low | Hours | Testability |
| global_mutex | KEEP | N/A | N/A | Sync primitive |
| request_count | FIX FIRST | High | Hours | Thread safety |
| g_cache | DEFER | Low | Weeks | High coupling |

## Decisions by Category

### MIGRATE NOW (Low effort, high value)

| Global | Current Location | Target | Notes |
|--------|-----------------|--------|-------|
| log_level | config.c | ServiceLocator | 3 files affected |
| | | | |

### MIGRATE NEXT (Medium effort)

| Global | Current Location | Target | Notes |
|--------|-----------------|--------|-------|
| db_connection | database.c | ServiceLocator | Fix thread safety first |
| | | | |

### FIX FIRST (Safety issues)

| Global | Issue | Fix |
|--------|-------|-----|
| request_count | Data race | Add atomic |
| | | |

### DEFER (High effort, evaluate later)

| Global | Coupling | Reason to Defer |
|--------|----------|-----------------|
| g_cache | HIGH (15 files) | Major effort, unclear benefit |
| | | |

### KEEP GLOBAL (Do not migrate)

| Global | Reason |
|--------|--------|
| global_mutex | Synchronization primitive |
| init_flag | Bootstrap state |
| | |

## Migration Order

1. Fix thread safety issues
2. Migrate LOW coupling globals
3. Migrate MEDIUM coupling globals
4. Re-evaluate HIGH coupling globals
5. Document remaining globals

## Success Criteria

After migration:
- [ ] No more "can't test because global state" problems
- [ ] Each service is explicitly passed or obtained from ServiceLocator
- [ ] Thread safety verified with sanitizers
- [ ] No functional regressions
```

## Priority Guidelines

**Migrate first (highest value):**
- Database connections, network clients → enable connection pooling, testing
- Loggers → enable per-module log levels, testing
- Configuration → enable runtime reconfiguration, testing

**Migrate second:**
- Caches → enable cache invalidation strategies
- Statistics → enable per-component metrics

**Usually keep global:**
- Mutexes, locks, sync primitives
- Allocator state
- One-time initialization flags
- Process-wide settings (locale, timezone)

**Evaluate case-by-case:**
- Hardware handles
- Plugin registries
- Signal handlers

---

# Chapter 9: Worked Example: Analyzing a Real Codebase

Let's walk through analyzing a small codebase step by step.

## The Example Codebase

Imagine a simple web server with these files:

```
webserver/
├── src/
│   ├── main.c
│   ├── server.c
│   ├── handler.c
│   ├── database.c
│   ├── config.c
│   └── logging.c
├── include/
│   ├── server.h
│   ├── database.h
│   ├── config.h
│   └── logging.h
└── migration_analysis/
```

## Step 1: Check Environment

```bash
cd webserver
bash check_environment.sh

# Output:
# grep: OK
# find: OK
# ripgrep: missing - will use grep
# Your profile: MINIMAL
```

We have the essentials. Let's proceed.

## Step 2: Create Analysis Directory

```bash
mkdir -p migration_analysis/{reports,scripts,temp}
```

## Step 3: Find Static Globals

```bash
grep -rn "^static\s" --include="*.c" src/ | grep -v "static.*(.*).*{" | grep "[=;]"

# Output:
# src/config.c:5:static int port = 8080;
# src/config.c:6:static int max_connections = 100;
# src/config.c:7:static char* document_root = NULL;
# src/logging.c:3:static FILE* log_file = NULL;
# src/logging.c:4:static int log_level = LOG_INFO;
# src/database.c:8:static Connection* db_conn = NULL;
# src/database.c:9:static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
# src/handler.c:12:static int request_count = 0;
# src/server.c:15:static bool running = true;
```

Found 9 static globals.

## Step 4: Categorize

| Global | Category | Notes |
|--------|----------|-------|
| port | Config | Server port |
| max_connections | Config | Connection limit |
| document_root | Config | Web root path |
| log_file | Singleton | Log output handle |
| log_level | Config | Verbosity |
| db_conn | Singleton | Database connection |
| db_mutex | Mutex | Protects db_conn |
| request_count | Counter | Statistics |
| running | Bootstrap | Server run flag |

## Step 5: Measure Coupling

```bash
for var in port max_connections document_root log_file log_level db_conn db_mutex request_count running; do
    files=$(grep -rl "\b${var}\b" --include="*.c" src/ | wc -l)
    refs=$(grep -rn "\b${var}\b" --include="*.c" src/ | wc -l)
    echo "$var: $files files, $refs refs"
done

# Output:
# port: 2 files, 4 refs
# max_connections: 2 files, 3 refs
# document_root: 2 files, 5 refs
# log_file: 1 files, 8 refs
# log_level: 3 files, 6 refs
# db_conn: 2 files, 12 refs
# db_mutex: 1 files, 6 refs
# request_count: 2 files, 4 refs
# running: 2 files, 5 refs
```

Most are LOW coupling (1-3 files). Good news!

## Step 6: Check Thread Safety

```bash
# Check for threading
grep -rn "pthread" --include="*.c" src/
# src/database.c:9:static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
# src/database.c:25:    pthread_mutex_lock(&db_mutex);
# src/database.c:28:    pthread_mutex_unlock(&db_mutex);
# src/server.c:45:    pthread_create(&thread, NULL, handle_request, client);

# Threading is used! Check which globals are protected:
# db_conn - protected by db_mutex ✓
# request_count - NOT protected! ✗
```

Found issue: `request_count` is accessed from multiple threads without protection.

## Step 7: Make Decisions

| Global | Coupling | Thread-Safe | Decision | Priority |
|--------|----------|-------------|----------|----------|
| port | LOW | Yes (read-only) | MIGRATE | Low |
| max_connections | LOW | Yes (read-only) | MIGRATE | Low |
| document_root | LOW | Yes (read-only) | MIGRATE | Low |
| log_file | NONE | Yes (single file) | MIGRATE | Medium |
| log_level | LOW | Yes (read-only) | MIGRATE | Low |
| db_conn | LOW | Yes (mutex protected) | MIGRATE | High |
| db_mutex | NONE | N/A | KEEP | - |
| request_count | LOW | NO | FIX FIRST | Urgent |
| running | LOW | Partial | KEEP | - |

## Step 8: Document Findings

```markdown
# Webserver Migration Analysis

## Summary

- 9 globals found
- 6 candidates for migration
- 1 thread safety issue requiring immediate fix
- 2 should remain global

## Immediate Action Required

### request_count (UNSAFE)

Change from:
```c
static int request_count = 0;
```

To:
```c
#include <stdatomic.h>
static atomic_int request_count = 0;
```

## Migration Plan

### Phase 1: Quick wins (config globals)
- port, max_connections, document_root, log_level
- All LOW coupling, read-only after init
- Migrate to ServiceLocator for testability

### Phase 2: Singletons
- log_file, db_conn
- Enable mock logger/database for testing

### Phase 3: Keep global
- db_mutex (synchronization)
- running (signal handling)

## Estimated Effort

- Fix thread safety: 30 minutes
- Phase 1 migration: 2-3 hours
- Phase 2 migration: 4-6 hours
- Total: ~1 day
```

That's a complete analysis of a small codebase!

---

# Chapter 10: Troubleshooting

This chapter addresses problems you'll encounter.

## Installation Problems

### "Package not found" when installing tools

**On Ubuntu/Debian:**
```bash
# Update package list first
sudo apt update

# Search for the package
apt-cache search ripgrep
apt-cache search ctags

# Package might have different name
# ripgrep might be: ripgrep
# ctags might be: universal-ctags, exuberant-ctags, ctags
```

**On RHEL/CentOS:**
```bash
# Enable EPEL repository first
sudo dnf install epel-release

# Then try again
sudo dnf install ripgrep ctags
```

**If nothing works:** Skip the tool and use grep fallback.

### "Permission denied" when installing

**No sudo access?**
```bash
# Install to user directory instead
pip install --user ripgrep  # If available via pip
npm install -g ripgrep      # If available via npm

# Or download static binary
wget https://github.com/BurntSushi/ripgrep/releases/download/13.0.0/ripgrep-13.0.0-x86_64-unknown-linux-musl.tar.gz
tar xzf ripgrep*.tar.gz
mv ripgrep*/rg ~/bin/  # Add ~/bin to PATH
```

### Tool installed but "command not found"

```bash
# Find where it installed
which ripgrep
find /usr -name "rg" 2>/dev/null
find ~/.local -name "rg" 2>/dev/null

# Add to PATH
export PATH="$PATH:/path/to/directory"
# Make permanent by adding to ~/.bashrc
```

## Search Problems

### "No matches found" but globals exist

**Check you're in the right directory:**
```bash
pwd
ls src/  # Do .c files exist here?
```

**Check file extension:**
```bash
# Are files .c or .C or .cpp?
ls src/*.c src/*.C src/*.cpp 2>/dev/null
```

**Check the search pattern:**
```bash
# Start with a simple search
grep -r "static" src/

# If that works, make it more specific
grep -rn "^static" --include="*.c" src/
```

### Too many results

**The search pattern is too broad:**
```bash
# Instead of:
grep -rn "static" src/  # Matches too much

# Use:
grep -rn "^static\s" --include="*.c" src/  # File-scope only
```

**Include only source files:**
```bash
# Exclude build directories, generated files
grep -rn "^static\s" --include="*.c" --exclude-dir={build,CMakeFiles,.git} src/
```

### Results include false positives

**You're matching function definitions as well as variables:**

The solution is the filtering shown in Chapter 4. If still getting false positives, review manually — some cases can only be distinguished by human judgment.

## Output Problems

### Results look garbled

**Line ending issue (Windows):**
```bash
# Check file type
file yourscript.sh

# If it says "CRLF line terminators":
dos2unix yourscript.sh
# Or:
sed -i 's/\r$//' yourscript.sh
```

**Encoding issue:**
```bash
# Force UTF-8
export LC_ALL=C.UTF-8

# Or force ASCII
export LC_ALL=C
```

### Can't save to file

**Permission denied:**
```bash
# Check if directory is writable
ls -la migration_analysis/
touch migration_analysis/reports/test.txt  # Can you create files?

# If not, use a different directory
mkdir -p ~/my_analysis_results
# Use ~/my_analysis_results instead
```

## Script Problems

### "Bad interpreter" error

```bash
./script.sh
# bash: ./script.sh: /bin/bash^M: bad interpreter

# The script has Windows line endings
dos2unix script.sh
# Or:
sed -i 's/\r$//' script.sh
```

### Script runs but produces nothing

**Check the script step by step:**
```bash
# Run commands from the script manually
grep -rn "^static\s" --include="*.c" src/
# Does this produce output?

# Check exit codes
grep -rn "^static\s" --include="*.c" src/
echo $?  # 0 = matches found, 1 = no matches, 2 = error
```

## Understanding Problems

### "I found globals but don't know what they mean"

Open the source file and look at context:
```bash
# Find where the global is defined
grep -n "static.*my_variable" src/

# Look at that line and surrounding code
# What functions use it?
# Is it configuration? A resource handle? A counter?
```

### "I don't know if something is a function or variable"

Look for parentheses:
```c
static int my_function(void);   // Function (has parentheses)
static int my_variable = 0;     // Variable (no parentheses, has =)
static int my_variable;         // Variable (no parentheses)
```

### "I can't tell if a global is thread-safe"

Ask these questions:
1. Is it only written once (initialization) and then read-only? → Probably safe
2. Is it protected by a mutex whenever accessed? → Safe if mutex used correctly
3. Is it an `atomic_` type? → Safe for simple operations
4. Otherwise → Assume unsafe

When in doubt, mark it as "needs investigation" and ask someone with threading expertise.

---

# FAQ

## General Questions

**Q: How long does analysis take?**

A: Rough estimates:
- Small codebase (<10K lines): 4-8 hours
- Medium codebase (10K-100K lines): 2-5 days
- Large codebase (>100K lines): 1-3 weeks

**Q: Do I need to analyze everything before migrating anything?**

A: No. Once you've found a few LOW-coupling globals, you can start migrating them while continuing analysis.

**Q: What if I find too many globals to handle?**

A: Prioritize. Start with:
1. Globals causing active problems (test failures, bugs)
2. Globals in code you're actively modifying
3. LOW coupling globals (quick wins)

Document the rest for future work.

## Technical Questions

**Q: What's the difference between `static` and `extern`?**

A: `static` at file scope means "private to this file." Other files can't access it directly. `extern` (or no storage class) means "accessible from other files."

**Q: Why can't grep find all my globals accurately?**

A: Grep searches text patterns. It doesn't understand C syntax. Some globals are declared through macros, complex types, or patterns that don't match the regex. For 100% accuracy, you'd need a C parser like clang-query.

**Q: Is this analysis methodology valid for C++ code too?**

A: Partially. C++ adds:
- Class static members (grep for `static.*::`)
- Global objects with constructors (harder to find)
- Template instantiations (very hard to find)

For C++, consider using clang-based tools.

## Decision Questions

**Q: Should I migrate ALL globals?**

A: No. Some globals are correct and necessary:
- Synchronization primitives
- Memory allocator state
- Process-wide settings

Only migrate globals that cause testing, maintenance, or safety problems.

**Q: What if stakeholders disagree on priorities?**

A: Use evidence:
- Coupling scores are objective
- Thread safety is objective
- Testing difficulty is demonstrable

Frame decisions around risk and effort, not opinion.

**Q: What if I'm not sure about a decision?**

A: Mark it "DEFER - needs investigation" and revisit later. You don't have to decide everything now.

---

# Glossary

**Coupling:** How many parts of the codebase depend on something. Higher coupling = more dependencies = harder to change.

**Data race:** When two threads access the same data without synchronization, and at least one is a write. Causes unpredictable bugs.

**External linkage:** A variable or function accessible from other translation units (other .c files). Declared without `static`.

**File scope:** A declaration outside any function. File-scope variables exist for the whole program.

**Global variable:** Data that persists for program lifetime and is accessible without being passed as a parameter.

**Internal linkage:** A variable or function accessible only within its own translation unit. Declared with `static`.

**Mutex:** Mutual exclusion lock. Ensures only one thread accesses protected data at a time.

**ServiceLocator:** A pattern where services are registered in a central registry and looked up by name or type.

**Static (storage class):** At file scope, means internal linkage (private to this file). At block scope, means persistent storage.

**Thread-local storage:** Each thread gets its own copy of a variable. `_Thread_local` in C11.

**Translation unit:** A .c file plus all headers it includes. The unit of compilation.

---

# Quick Reference

## Essential Commands

### Find static globals:
```bash
grep -rn "^static\s" --include="*.c" src/ | grep -v "(.*).*{" | grep "[=;]"
```

### Find extern declarations:
```bash
grep -rn "^extern\s" --include="*.h" include/
```

### Count coupling for a global:
```bash
GLOBAL="my_var"
grep -rl "\b${GLOBAL}\b" --include="*.c" src/ | wc -l  # Files
grep -rn "\b${GLOBAL}\b" --include="*.c" src/ | wc -l  # References
```

### Check for threading:
```bash
grep -rn "pthread\|thread\|mutex\|atomic" --include="*.c" --include="*.h" src/
```

## Decision Quick Guide

| Question | Answer → Decision |
|----------|------------------|
| Is it a mutex/lock? | Yes → KEEP |
| Is it bootstrap state? | Yes → KEEP |
| Thread safety issue? | Yes → FIX FIRST |
| Coupling HIGH? | Yes → DEFER or PARTIAL |
| Coupling LOW? | Yes → MIGRATE |

## File Checklist

After analysis, you should have:
- [ ] `migration_analysis/reports/static_globals.txt`
- [ ] `migration_analysis/reports/extern_globals.txt`
- [ ] `migration_analysis/reports/coupling_analysis.md`
- [ ] `migration_analysis/reports/thread_safety.md`
- [ ] `migration_analysis/reports/decisions.md`

---

*FAT-P Library — User Manual UM-MIGRATION-ANALYSIS-001*  
*Last updated: January 2025*
