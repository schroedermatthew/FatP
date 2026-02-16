---
doc_id: UM-MIGRATION-ANALYSIS-003
doc_type: "User Manual"
title: "C Codebase Analysis with Minimal Tooling"
fatp_components: ["ServiceLocator"]
topics: ["global state", "grep", "PowerShell", "regex", "migration analysis", "vim", "VS Code", "editor plugins"]
constraints: ["no specialized tools", "basic Unix utilities", "cross-platform"]
cxx_standard: "C++20"
last_verified: "2025-01-09"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# User Manual - C Codebase Analysis with Minimal Tooling

### *grep, PowerShell, and editor plugins when specialized tools aren't available*

*FAT-P Library — January 2025*

---

## User Manual Card

**Component:** Minimal Analysis Toolchain  
**Primary use case:** Pre-migration analysis when cscope, clang-query, or commercial tools unavailable  
**Integration pattern:** Shell scripts + editor plugins for interactive exploration  
**Key tools:** grep/ripgrep (Linux), Select-String/findstr (Windows), nm, editor integrations  
**Common pitfalls:** Regex escaping differences, missing macro-expanded globals, false positives from comments  
**When to use:** Constrained environments, quick triage, supplementing other tools  

---

## Table of Contents

1. [Overview](#overview)
2. [Tool Equivalence Reference](#tool-equivalence-reference)
   - [Windows Environment Options](#windows-environment-options)
   - [MinGW/MSYS2 Setup](#mingwmsys2-setup)
   - [Git Bash Setup](#git-bash-setup)
   - [Command Mapping](#command-mapping)
3. [Finding Global Variables](#finding-global-variables)
4. [Tracing Symbol Usage](#tracing-symbol-usage)
5. [Finding Mutex Patterns](#finding-mutex-patterns)
6. [Finding Public API](#finding-public-api)
7. [Automation Scripts](#automation-scripts)
   - [Linux: Complete Analysis Script](#linux-complete-analysis-script)
   - [Windows PowerShell: Complete Analysis Script](#windows-powershell-complete-analysis-script)
   - [Windows with MinGW/MSYS2: Using Bash Scripts](#windows-with-mingwmsys2-using-bash-scripts)
   - [Symbol Lookup Script](#symbol-lookup-script)
8. [Editor Plugins and Integration](#editor-plugins-and-integration)
9. [Complete Analysis Workflow](#complete-analysis-workflow)
10. [Limitations and Workarounds](#limitations-and-workarounds)
11. [Quick Reference Card](#quick-reference-card)

---

## Overview

When specialized tools (cscope, clang-query, Understand) aren't available, you can still perform meaningful analysis using:

- **grep/ripgrep** — Pattern matching in files
- **nm** — Symbol extraction from object files
- **find** — File discovery
- **awk/sed** — Text transformation
- **Editor plugins** — Interactive navigation

**When to use this approach:**

| Situation | Use Minimal Tooling? |
|-----------|---------------------|
| SSH into production server | Yes |
| Quick triage of unfamiliar code | Yes |
| Embedded/cross-compile environment | Yes |
| CI/CD pipeline checks | Yes |
| Thorough pre-migration analysis | No — use proper tools |
| Large codebase (>500K lines) | No — too slow, too many false positives |

**Time estimate:** Hours for quick triage, 1-2 weeks for thorough analysis of medium codebase

---

## Tool Equivalence Reference

### Windows Environment Options

Windows users have three options for running these analysis commands:

| Environment | Pros | Cons | Best For |
|-------------|------|------|----------|
| **PowerShell** | Native, no install, .NET regex | Different syntax, some limitations | Quick tasks, integration with Windows tools |
| **MinGW/MSYS2** | Full Unix tools, same scripts as Linux | Requires install, path issues | Heavy grep/awk usage, reusing Linux scripts |
| **Git Bash** | Comes with Git, lightweight | Limited tool set | Already have Git installed |
| **WSL** | Full Linux environment | Heavier, filesystem boundary | Need Linux-only tools (TSan, Infer) |

### MinGW/MSYS2 Setup

MSYS2 provides a full Unix-like environment with pacman package manager.

**Installation:**
```powershell
# Option 1: Download installer from https://www.msys2.org/
# Run msys2-x86_64-<date>.exe

# Option 2: Use winget
winget install --id=MSYS2.MSYS2 -e

# Option 3: Use Chocolatey
choco install msys2
```

**Post-Installation Setup:**
```bash
# Open MSYS2 UCRT64 terminal (recommended for modern Windows)
# Update package database
pacman -Syu

# Install development tools
pacman -S --noconfirm \
    base-devel \
    mingw-w64-ucrt-x86_64-toolchain \
    mingw-w64-ucrt-x86_64-grep \
    mingw-w64-ucrt-x86_64-ripgrep \
    mingw-w64-ucrt-x86_64-universal-ctags \
    mingw-w64-ucrt-x86_64-cscope

# Verify installations
grep --version
rg --version
ctags --version
cscope --version
```

**Add to Windows PATH:**
```powershell
# Add MSYS2 binaries to Windows PATH (run as Administrator)
$msys2Path = "C:\msys64\ucrt64\bin"
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
[Environment]::SetEnvironmentVariable("PATH", "$currentPath;$msys2Path", "Machine")

# Restart terminal to pick up changes
```

**Using MSYS2 from PowerShell:**
```powershell
# Run single command
& "C:\msys64\usr\bin\bash.exe" -c "grep -rn 'static' src/"

# Run script
& "C:\msys64\usr\bin\bash.exe" ./analyze_codebase.sh

# Create PowerShell alias for convenience
function msys { & "C:\msys64\usr\bin\bash.exe" -c $args }
msys "grep --version"
```

**Using MSYS2 from CMD:**
```batch
:: Run command
C:\msys64\usr\bin\bash.exe -c "grep -rn 'static' src/"

:: Add to PATH and use directly
set PATH=%PATH%;C:\msys64\ucrt64\bin
grep -rn "static" src/
```

### Git Bash Setup

Git for Windows includes a minimal Unix environment.

**Location:** `C:\Program Files\Git\usr\bin\`

**Available tools:**
- grep, awk, sed, find, wc, sort, uniq
- Missing: ripgrep, ctags, cscope (install separately)

**Usage from PowerShell:**
```powershell
# Use Git's grep directly
& "C:\Program Files\Git\usr\bin\grep.exe" -rn "static" src/

# Create alias
Set-Alias grep "C:\Program Files\Git\usr\bin\grep.exe"
grep -rn "static" --include="*.c" src/

# Or add to PATH
$env:PATH += ";C:\Program Files\Git\usr\bin"
```

**Usage from CMD:**
```batch
set PATH=%PATH%;C:\Program Files\Git\usr\bin
grep -rn "static" src/
```

### Command Mapping

| Task | Linux (bash) | MinGW/MSYS2 | Windows (PowerShell) | Windows (cmd) |
|------|--------------|-------------|---------------------|---------------|
| Search file contents | `grep` | `grep` (same) | `Select-String` | `findstr` |
| Recursive search | `grep -r` | `grep -r` (same) | `Get-ChildItem -Recurse \| Select-String` | `findstr /s` |
| Find files | `find` | `find` (same) | `Get-ChildItem` | `dir /s` |
| Symbol listing | `nm` | `nm` (same) | `dumpbin /symbols` | `dumpbin /symbols` |
| Text substitution | `sed` | `sed` (same) | `-replace` operator | N/A |
| Column extraction | `awk` | `awk` (same) | `ForEach-Object { }` | N/A |
| Count lines | `wc -l` | `wc -l` (same) | `Measure-Object -Line` | `find /c /v ""` |

**Key insight:** With MinGW/MSYS2 installed, all Linux bash commands and scripts work unchanged on Windows.

### Path Handling in MinGW/MSYS2

MSYS2 automatically translates Windows paths, but some edge cases need attention:

```bash
# In MSYS2 terminal, Windows paths are mounted under /c/, /d/, etc.
cd /c/Projects/myproject
grep -rn "static" src/

# From Windows calling into MSYS2, use Windows paths
# MSYS2 translates automatically
C:\msys64\usr\bin\bash.exe -c "grep -rn 'static' C:/Projects/myproject/src/"

# For scripts, use portable path handling
PROJECT_ROOT="${1:-$(pwd)}"
# Works whether called from MSYS2 or Windows
```

**Common path issues and fixes:**

| Issue | Cause | Fix |
|-------|-------|-----|
| `grep: /c/...: No such file` | Mixed path styles | Use consistent forward slashes |
| `find: paths must precede expression` | Windows-style backslashes | Use forward slashes or quote paths |
| Script works in terminal but not from PowerShell | Line endings (CRLF vs LF) | Convert script to LF: `dos2unix script.sh` |
| `command not found` | PATH not set | Add MSYS2 bin to PATH or use full path |

### Using Bash Scripts from PowerShell (MinGW)

```powershell
# Method 1: Call bash explicitly
& "C:\msys64\usr\bin\bash.exe" "./analyze_codebase.sh" "C:/Projects/myproject" "C:/Projects/analysis"

# Method 2: Create wrapper function
function Invoke-BashScript {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Script,
        [Parameter(ValueFromRemainingArguments=$true)]
        [string[]]$Arguments
    )
    
    $bashPath = "C:\msys64\usr\bin\bash.exe"
    if (-not (Test-Path $bashPath)) {
        $bashPath = "C:\Program Files\Git\usr\bin\bash.exe"
    }
    
    # Convert Windows paths to forward slashes
    $convertedArgs = $Arguments | ForEach-Object { $_ -replace '\\', '/' }
    
    & $bashPath $Script @convertedArgs
}

# Usage
Invoke-BashScript "./analyze_codebase.sh" "C:\Projects\myproject" "C:\Projects\analysis"

# Method 3: Use MSYS2's mintty terminal for interactive work
Start-Process "C:\msys64\ucrt64.exe"
```

### MinGW vs Native Windows Tools Decision

| Scenario | Recommendation |
|----------|----------------|
| One-off quick search | PowerShell `Select-String` |
| Reusing Linux scripts | MinGW/MSYS2 |
| Team with mixed OS | MinGW (same scripts everywhere) |
| CI/CD pipeline | PowerShell (native) or MinGW (cross-platform) |
| Complex awk/sed processing | MinGW |
| Integration with Visual Studio | PowerShell + dumpbin |

### grep vs Select-String vs findstr

| Feature | grep | Select-String | findstr |
|---------|------|---------------|---------|
| Regex support | Full (ERE/BRE) | .NET regex | Limited |
| Recursive | `-r` | Via pipeline | `/s` |
| Context lines | `-A`, `-B`, `-C` | `-Context` | N/A |
| Invert match | `-v` | `-NotMatch` | `/v` |
| Case insensitive | `-i` | `-CaseSensitive:$false` | `/i` |
| Line numbers | `-n` | Default | `/n` |
| Count only | `-c` | `Measure-Object` | `/c` |
| Whole word | `-w` | `\b...\b` | `/w` (limited) |
| File pattern | `--include` | `-Include` | N/A |

### ripgrep (rg) - Recommended

ripgrep is faster than grep and available on all platforms:

```bash
# Install
# Linux
sudo apt install ripgrep
# or
cargo install ripgrep

# macOS
brew install ripgrep

# Windows
choco install ripgrep
# or
scoop install ripgrep
```

| Feature | grep | ripgrep (rg) |
|---------|------|--------------|
| Speed | Baseline | 2-10x faster |
| Respects .gitignore | No | Yes (default) |
| Unicode | Varies | Full |
| Multiline | Limited | `-U` flag |
| Replace | No | `--replace` |

---

## Finding Global Variables

### Pattern 1: File-Scope Static Variables

**Linux (grep):**
```bash
# Basic: find "static <type> <name>"
grep -rn "^static\s\+[a-zA-Z_]" --include="*.c" src/

# More precise: exclude static functions
grep -rn "^static\s\+\(const\s\+\)\?\(struct\s\+\)\?[a-zA-Z_][a-zA-Z0-9_]*\s\+\*\?[a-zA-Z_][a-zA-Z0-9_]*\s*[=;[]" --include="*.c" src/

# Exclude common false positives
grep -rn "^static\s" --include="*.c" src/ | \
    grep -v "static\s\+inline" | \
    grep -v "static\s\+void\s\+[a-z]" | \
    grep -v "static\s\+int\s\+[a-z].*("
```

**Linux (ripgrep):**
```bash
# ripgrep with PCRE2 for better regex
rg "^static\s+(?:const\s+)?(?:struct\s+)?[a-zA-Z_]\w*\s+\*?[a-zA-Z_]\w*\s*[=;\[]" --type c src/

# Exclude functions (lines containing parentheses before = or ;)
rg "^static\s+\w+.*[=;]" --type c src/ | rg -v "\("
```

**Windows (PowerShell):**
```powershell
# Basic search
Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern "^static\s+[a-zA-Z_]" |
    Where-Object { $_ -notmatch "static\s+(inline|void|int)\s+\w+\s*\(" }

# More precise with .NET regex
$pattern = "^static\s+(const\s+)?(struct\s+)?[a-zA-Z_]\w*\s+\*?[a-zA-Z_]\w*\s*[=;\[]"
Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern $pattern

# Output to file
Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern "^static\s+" |
    Where-Object { $_ -notmatch "\(" } |
    Out-File "static_globals.txt"
```

**Windows (cmd/findstr):**
```batch
:: Basic (limited regex)
findstr /s /n /r "^static " src\*.c

:: Better: use grep for Windows (Git Bash or GnuWin32)
"C:\Program Files\Git\usr\bin\grep.exe" -rn "^static " --include="*.c" src/
```

### Pattern 2: External Linkage Globals

**Linux:**
```bash
# Variables at file scope without 'static'
# This is tricky - need to exclude function definitions

# Step 1: Find file-scope declarations (start of line, not indented)
grep -rn "^[a-zA-Z_][a-zA-Z0-9_]*\s\+[a-zA-Z_][a-zA-Z0-9_]*\s*[=;]" --include="*.c" src/

# Step 2: Exclude lines with 'static' or that look like functions
grep -rn "^[a-zA-Z_]" --include="*.c" src/ | \
    grep -v "^static" | \
    grep -v "(.*)$" | \
    grep "[=;]$"
```

**PowerShell:**
```powershell
# File-scope non-static
$pattern = "^[a-zA-Z_]\w*\s+[a-zA-Z_]\w*\s*[=;]"
Get-ChildItem -Path src -Filter "*.c" -Recurse | 
    Select-String -Pattern $pattern |
    Where-Object { $_ -notmatch "^static" -and $_ -notmatch "\(" }
```

### Pattern 3: Using nm for Compiled Objects

**Linux:**
```bash
# List global data symbols
# B = BSS (uninitialized), D = Data (initialized), R = Read-only
nm -g *.o 2>/dev/null | grep -E "^[0-9a-f]+\s+[BDR]\s+"

# With file names
for obj in *.o; do
    echo "=== $obj ==="
    nm -g "$obj" 2>/dev/null | grep -E "\s+[BDR]\s+"
done

# From static library
nm -g libfoo.a | grep -E "\s+[BDR]\s+"

# From shared library
nm -D libfoo.so | grep -E "\s+[BDR]\s+"
```

**Windows (dumpbin):**
```powershell
# Requires Visual Studio Developer Command Prompt
# Or add to PATH: C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\<version>\bin\Hostx64\x64

# List symbols from object file
dumpbin /symbols foo.obj | Select-String "External\s+\|\s+(SECT|UNDEF)"

# From library
dumpbin /symbols foo.lib | Select-String "External"

# Global data only (exclude functions)
dumpbin /symbols foo.obj | 
    Select-String "External" |
    Where-Object { $_ -notmatch "notype.*\(\)" }
```

### Pattern 4: Extern Declarations in Headers

**Linux:**
```bash
# Find extern declarations
grep -rn "^extern\s" --include="*.h" include/

# Parse out variable names
grep -rn "^extern\s" --include="*.h" include/ | \
    sed 's/.*extern\s\+[a-zA-Z_][a-zA-Z0-9_]*\s\+\*\?\([a-zA-Z_][a-zA-Z0-9_]*\).*/\1/' | \
    sort -u
```

**PowerShell:**
```powershell
Get-ChildItem -Path include -Filter "*.h" -Recurse |
    Select-String -Pattern "^extern\s" |
    ForEach-Object {
        if ($_ -match "extern\s+\w+\s+\*?(\w+)") {
            $matches[1]
        }
    } | Sort-Object -Unique
```

---

## Tracing Symbol Usage

### Find All References to a Symbol

**Linux:**
```bash
SYMBOL="vfsList"

# Basic search
grep -rn "\b${SYMBOL}\b" --include="*.c" --include="*.h" src/

# With context
grep -rn -B2 -A2 "\b${SYMBOL}\b" --include="*.c" src/

# Count references per file
grep -rn "\b${SYMBOL}\b" --include="*.c" src/ | \
    cut -d: -f1 | sort | uniq -c | sort -rn

# Exclude comments (simple heuristic)
grep -rn "\b${SYMBOL}\b" --include="*.c" src/ | \
    grep -v "^\s*//" | \
    grep -v "^\s*/\*" | \
    grep -v "^\s*\*"
```

**ripgrep:**
```bash
SYMBOL="vfsList"

# Basic (word boundary automatic with \b)
rg "\b${SYMBOL}\b" --type c src/

# With context
rg "\b${SYMBOL}\b" --type c -B2 -A2 src/

# Stats
rg "\b${SYMBOL}\b" --type c --stats src/

# Files only
rg -l "\b${SYMBOL}\b" --type c src/
```

**PowerShell:**
```powershell
$Symbol = "vfsList"

# Basic search
Get-ChildItem -Path src -Include "*.c", "*.h" -Recurse |
    Select-String -Pattern "\b$Symbol\b"

# With context (3 lines before and after)
Get-ChildItem -Path src -Include "*.c", "*.h" -Recurse |
    Select-String -Pattern "\b$Symbol\b" -Context 3,3

# Count per file
Get-ChildItem -Path src -Include "*.c", "*.h" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Group-Object Path |
    Select-Object Count, Name |
    Sort-Object Count -Descending

# Files only
Get-ChildItem -Path src -Include "*.c", "*.h" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Select-Object -ExpandProperty Path -Unique
```

### Find Reads vs Writes

**Linux:**
```bash
SYMBOL="vfsList"

# Writes: symbol on left of assignment
grep -rn "\b${SYMBOL}\b\s*=" --include="*.c" src/ | grep -v "=="

# Reads: symbol not on left of assignment (heuristic)
grep -rn "\b${SYMBOL}\b" --include="*.c" src/ | \
    grep -v "\b${SYMBOL}\b\s*=" | \
    grep -v "=\s*\b${SYMBOL}\b"  # Also exclude as RHS to reduce noise

# Function calls with symbol as argument
grep -rn "([^)]*\b${SYMBOL}\b[^)]*)" --include="*.c" src/
```

**PowerShell:**
```powershell
$Symbol = "vfsList"

# Writes
Get-ChildItem -Path src -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b\s*=" |
    Where-Object { $_ -notmatch "==" }

# Reads (excluding writes)
Get-ChildItem -Path src -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Where-Object { $_ -notmatch "\b$Symbol\b\s*=" }
```

### Find Functions That Access a Global

**Linux:**
```bash
SYMBOL="vfsList"

# Find function definitions containing the symbol
# This is approximate - looks for function-like patterns before the usage

grep -rn "\b${SYMBOL}\b" --include="*.c" src/ | while read line; do
    file=$(echo "$line" | cut -d: -f1)
    lineno=$(echo "$line" | cut -d: -f2)
    
    # Search backwards for function definition
    head -n "$lineno" "$file" | tac | grep -m1 "^[a-zA-Z_].*(.*).*{" | \
        sed 's/(.*//' | awk '{print $NF}'
done | sort -u
```

**PowerShell:**
```powershell
$Symbol = "vfsList"

# Get files containing symbol
$files = Get-ChildItem -Path src -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Select-Object -ExpandProperty Path -Unique

foreach ($file in $files) {
    $content = Get-Content $file
    $lineNumbers = (Select-String -Path $file -Pattern "\b$Symbol\b").LineNumber
    
    foreach ($lineNo in $lineNumbers) {
        # Search backwards for function definition
        for ($i = $lineNo - 1; $i -ge 0; $i--) {
            if ($content[$i] -match "^[a-zA-Z_]\w*\s+\*?(\w+)\s*\([^)]*\)\s*\{?") {
                Write-Output "$($matches[1]) -> $Symbol (line $lineNo)"
                break
            }
        }
    }
} | Sort-Object -Unique
```

---

## Finding Mutex Patterns

### Find Mutex Declarations

**Linux:**
```bash
# POSIX mutexes
grep -rn "pthread_mutex_t" --include="*.c" --include="*.h" src/

# C11 mutexes
grep -rn "\bmtx_t\b" --include="*.c" --include="*.h" src/

# Windows critical sections
grep -rn "CRITICAL_SECTION" --include="*.c" --include="*.h" src/

# Custom mutex types (common patterns)
grep -rn "_mutex\|Mutex\|_lock\|Lock" --include="*.h" src/ | grep "typedef\|struct"

# All in one
grep -rn "pthread_mutex_t\|mtx_t\|CRITICAL_SECTION\|std::mutex" \
    --include="*.c" --include="*.h" --include="*.cpp" src/
```

**PowerShell:**
```powershell
# All mutex patterns
$patterns = @(
    "pthread_mutex_t",
    "\bmtx_t\b",
    "CRITICAL_SECTION",
    "std::mutex",
    "SRWLOCK"
)

$regex = $patterns -join "|"
Get-ChildItem -Path src -Include "*.c", "*.h", "*.cpp" -Recurse |
    Select-String -Pattern $regex
```

### Find Lock/Unlock Pairs

**Linux:**
```bash
# POSIX
grep -rn "pthread_mutex_lock\|pthread_mutex_unlock" --include="*.c" src/

# Windows
grep -rn "EnterCriticalSection\|LeaveCriticalSection" --include="*.c" src/
grep -rn "AcquireSRWLock\|ReleaseSRWLock" --include="*.c" src/

# C11
grep -rn "mtx_lock\|mtx_unlock" --include="*.c" src/

# Show lock with surrounding context
grep -rn -A10 "pthread_mutex_lock" --include="*.c" src/
```

**PowerShell:**
```powershell
# Find lock patterns with context
$lockPatterns = @(
    "pthread_mutex_lock",
    "EnterCriticalSection",
    "AcquireSRWLock",
    "mtx_lock",
    "\.lock\(\)"
)

$regex = $lockPatterns -join "|"
Get-ChildItem -Path src -Filter "*.c" -Recurse |
    Select-String -Pattern $regex -Context 0,10
```

### Find Globals Accessed Under Lock

**Linux:**
```bash
# For each global, check if it appears between lock and unlock
GLOBAL="sharedCounter"

grep -rn -A20 "pthread_mutex_lock\|EnterCriticalSection" --include="*.c" src/ | \
    grep -B20 "pthread_mutex_unlock\|LeaveCriticalSection" | \
    grep "\b${GLOBAL}\b"

# Simpler: check if global is near mutex operations
grep -rn -B5 -A5 "\b${GLOBAL}\b" --include="*.c" src/ | \
    grep -i "mutex\|lock\|critical"
```

**PowerShell:**
```powershell
$Global = "sharedCounter"

# Check if global appears near lock operations
Get-ChildItem -Path src -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Global\b" -Context 5,5 |
    Where-Object { $_.Context.PreContext + $_.Context.PostContext -match "mutex|lock|critical" }
```

---

## Finding Public API

### Exported Function Declarations

**Linux:**
```bash
# Common visibility patterns
grep -rn "^[A-Z_]*API\s" --include="*.h" include/
grep -rn "__attribute__.*visibility" --include="*.h" include/
grep -rn "__declspec.*dllexport" --include="*.h" include/

# Function declarations in headers (public by convention)
grep -rn "^[a-zA-Z_][a-zA-Z0-9_]*\s\+\*\?[a-zA-Z_][a-zA-Z0-9_]*\s*(.*)" \
    --include="*.h" include/ | grep -v "^static\|^#"
```

**PowerShell:**
```powershell
# API macros
Get-ChildItem -Path include -Filter "*.h" -Recurse |
    Select-String -Pattern "^[A-Z_]*API\s|__declspec.*dllexport|visibility"

# Public function declarations
Get-ChildItem -Path include -Filter "*.h" -Recurse |
    Select-String -Pattern "^[a-zA-Z_]\w*\s+\*?\w+\s*\([^)]*\)\s*;" |
    Where-Object { $_ -notmatch "^static|^#|^\s*//" }
```

### Using nm/dumpbin for Exported Symbols

**Linux:**
```bash
# Shared library exports
nm -D --defined-only libfoo.so | grep " T "

# Count public functions
nm -D --defined-only libfoo.so | grep " T " | wc -l

# Compare with static symbols (not exported)
nm libfoo.so | grep " t " | wc -l  # lowercase = local
```

**Windows:**
```powershell
# DLL exports
dumpbin /exports foo.dll

# Parse to list
dumpbin /exports foo.dll | 
    Select-String -Pattern "^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)" |
    ForEach-Object { $_.Matches.Groups[1].Value }
```

---

## Automation Scripts

### Linux: Complete Analysis Script

```bash
#!/bin/bash
# analyze_codebase.sh - Minimal tooling analysis

set -e

SRC_DIR="${1:-.}"
OUTPUT_DIR="${2:-./analysis_output}"
mkdir -p "$OUTPUT_DIR"

echo "=== C Codebase Analysis ==="
echo "Source: $SRC_DIR"
echo "Output: $OUTPUT_DIR"
echo ""

# --- Global Variables ---
echo "[1/5] Finding global variables..."

# Static globals
echo "## Static Globals" > "$OUTPUT_DIR/globals.md"
echo '```' >> "$OUTPUT_DIR/globals.md"
grep -rn "^static\s\+[a-zA-Z_]" --include="*.c" "$SRC_DIR" 2>/dev/null | \
    grep -v "static\s\+inline\|static\s\+void\s\+\|static\s\+int\s\+[a-z].*(" >> "$OUTPUT_DIR/globals.md" || true
echo '```' >> "$OUTPUT_DIR/globals.md"

# Count
STATIC_COUNT=$(grep -c "^static" "$OUTPUT_DIR/globals.md" 2>/dev/null || echo 0)
echo "  Found $STATIC_COUNT static globals"

# --- Mutex Patterns ---
echo "[2/5] Finding mutex patterns..."

echo "## Mutex Patterns" > "$OUTPUT_DIR/mutexes.md"
echo '```' >> "$OUTPUT_DIR/mutexes.md"
grep -rn "pthread_mutex\|mtx_t\|CRITICAL_SECTION\|_mutex\|_lock" \
    --include="*.c" --include="*.h" "$SRC_DIR" 2>/dev/null >> "$OUTPUT_DIR/mutexes.md" || true
echo '```' >> "$OUTPUT_DIR/mutexes.md"

MUTEX_COUNT=$(grep -c "mutex\|lock" "$OUTPUT_DIR/mutexes.md" 2>/dev/null || echo 0)
echo "  Found $MUTEX_COUNT mutex-related lines"

# --- Cross-references ---
echo "[3/5] Building cross-reference for globals..."

echo "## Cross-References" > "$OUTPUT_DIR/xrefs.md"
echo "" >> "$OUTPUT_DIR/xrefs.md"
echo "| Global | Files | References |" >> "$OUTPUT_DIR/xrefs.md"
echo "|--------|-------|------------|" >> "$OUTPUT_DIR/xrefs.md"

# Extract global names and count references
grep -oh "^static\s\+[a-zA-Z_][a-zA-Z0-9_\*\s]*\s\+\*\?\([a-zA-Z_][a-zA-Z0-9_]*\)" \
    --include="*.c" -r "$SRC_DIR" 2>/dev/null | \
    sed 's/.*\s\+\*\?\([a-zA-Z_][a-zA-Z0-9_]*\)$/\1/' | \
    sort -u | head -50 | while read global; do
    if [ -n "$global" ]; then
        refs=$(grep -rn "\b${global}\b" --include="*.c" "$SRC_DIR" 2>/dev/null | wc -l)
        files=$(grep -rl "\b${global}\b" --include="*.c" "$SRC_DIR" 2>/dev/null | wc -l)
        echo "| $global | $files | $refs |" >> "$OUTPUT_DIR/xrefs.md"
    fi
done

# --- Public API ---
echo "[4/5] Finding public API..."

echo "## Public API" > "$OUTPUT_DIR/api.md"
echo '```' >> "$OUTPUT_DIR/api.md"
grep -rn "^[A-Z_]*API\|__attribute__.*visibility\|__declspec.*dllexport" \
    --include="*.h" "$SRC_DIR" 2>/dev/null >> "$OUTPUT_DIR/api.md" || true
echo '```' >> "$OUTPUT_DIR/api.md"

API_COUNT=$(grep -c "API\|visibility\|dllexport" "$OUTPUT_DIR/api.md" 2>/dev/null || echo 0)
echo "  Found $API_COUNT API declarations"

# --- Summary ---
echo "[5/5] Generating summary..."

cat > "$OUTPUT_DIR/SUMMARY.md" << EOF
# Analysis Summary

**Generated:** $(date)
**Source Directory:** $SRC_DIR

## Statistics

| Metric | Count |
|--------|-------|
| Static globals | $STATIC_COUNT |
| Mutex patterns | $MUTEX_COUNT |
| API declarations | $API_COUNT |

## Files Generated

- globals.md - Static global variables
- mutexes.md - Mutex and lock patterns
- xrefs.md - Cross-reference counts
- api.md - Public API declarations

## Next Steps

1. Review globals.md for migration candidates
2. Check mutexes.md for thread safety patterns
3. Use xrefs.md to assess coupling
4. Preserve API surface from api.md

EOF

echo ""
echo "=== Analysis Complete ==="
echo "Results in: $OUTPUT_DIR"
cat "$OUTPUT_DIR/SUMMARY.md"
```

### Windows PowerShell: Complete Analysis Script

```powershell
# Analyze-Codebase.ps1 - Minimal tooling analysis for Windows

param(
    [Parameter(Mandatory=$true)]
    [string]$SourceDir,
    
    [string]$OutputDir = ".\analysis_output"
)

$ErrorActionPreference = "Continue"

# Create output directory
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

Write-Host "=== C Codebase Analysis ===" -ForegroundColor Cyan
Write-Host "Source: $SourceDir"
Write-Host "Output: $OutputDir"
Write-Host ""

# --- Global Variables ---
Write-Host "[1/5] Finding global variables..." -ForegroundColor Yellow

$globalsFile = Join-Path $OutputDir "globals.md"
@"
## Static Globals

``````
"@ | Out-File $globalsFile

$staticGlobals = Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse -ErrorAction SilentlyContinue |
    Select-String -Pattern "^static\s+[a-zA-Z_]" |
    Where-Object { $_ -notmatch "static\s+(inline|void|int)\s+\w+\s*\(" }

$staticGlobals | ForEach-Object { $_.ToString() } | Add-Content $globalsFile
"``````" | Add-Content $globalsFile

$staticCount = ($staticGlobals | Measure-Object).Count
Write-Host "  Found $staticCount static globals"

# --- Mutex Patterns ---
Write-Host "[2/5] Finding mutex patterns..." -ForegroundColor Yellow

$mutexFile = Join-Path $OutputDir "mutexes.md"
@"
## Mutex Patterns

``````
"@ | Out-File $mutexFile

$mutexPatterns = "pthread_mutex|mtx_t|CRITICAL_SECTION|_mutex|_lock|SRWLOCK"
$mutexLines = Get-ChildItem -Path $SourceDir -Include "*.c", "*.h" -Recurse -ErrorAction SilentlyContinue |
    Select-String -Pattern $mutexPatterns

$mutexLines | ForEach-Object { $_.ToString() } | Add-Content $mutexFile
"``````" | Add-Content $mutexFile

$mutexCount = ($mutexLines | Measure-Object).Count
Write-Host "  Found $mutexCount mutex-related lines"

# --- Cross-references ---
Write-Host "[3/5] Building cross-reference for globals..." -ForegroundColor Yellow

$xrefFile = Join-Path $OutputDir "xrefs.md"
@"
## Cross-References

| Global | Files | References |
|--------|-------|------------|
"@ | Out-File $xrefFile

# Extract global names
$globalNames = $staticGlobals | ForEach-Object {
    if ($_.Line -match "static\s+\w+[\s\*]+(\w+)\s*[=;\[]") {
        $matches[1]
    }
} | Sort-Object -Unique | Select-Object -First 50

foreach ($global in $globalNames) {
    if ([string]::IsNullOrWhiteSpace($global)) { continue }
    
    $refs = Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse -ErrorAction SilentlyContinue |
        Select-String -Pattern "\b$global\b"
    
    $refCount = ($refs | Measure-Object).Count
    $fileCount = ($refs | Select-Object -ExpandProperty Path -Unique | Measure-Object).Count
    
    "| $global | $fileCount | $refCount |" | Add-Content $xrefFile
}

# --- Public API ---
Write-Host "[4/5] Finding public API..." -ForegroundColor Yellow

$apiFile = Join-Path $OutputDir "api.md"
@"
## Public API

``````
"@ | Out-File $apiFile

$apiPatterns = "^[A-Z_]+API|__declspec.*dllexport|visibility"
$apiLines = Get-ChildItem -Path $SourceDir -Filter "*.h" -Recurse -ErrorAction SilentlyContinue |
    Select-String -Pattern $apiPatterns

$apiLines | ForEach-Object { $_.ToString() } | Add-Content $apiFile
"``````" | Add-Content $apiFile

$apiCount = ($apiLines | Measure-Object).Count
Write-Host "  Found $apiCount API declarations"

# --- Summary ---
Write-Host "[5/5] Generating summary..." -ForegroundColor Yellow

$summaryFile = Join-Path $OutputDir "SUMMARY.md"
@"
# Analysis Summary

**Generated:** $(Get-Date -Format "yyyy-MM-dd HH:mm")
**Source Directory:** $SourceDir

## Statistics

| Metric | Count |
|--------|-------|
| Static globals | $staticCount |
| Mutex patterns | $mutexCount |
| API declarations | $apiCount |

## Files Generated

- globals.md - Static global variables
- mutexes.md - Mutex and lock patterns  
- xrefs.md - Cross-reference counts
- api.md - Public API declarations

## Next Steps

1. Review globals.md for migration candidates
2. Check mutexes.md for thread safety patterns
3. Use xrefs.md to assess coupling
4. Preserve API surface from api.md

"@ | Out-File $summaryFile

Write-Host ""
Write-Host "=== Analysis Complete ===" -ForegroundColor Green
Write-Host "Results in: $OutputDir"
Get-Content $summaryFile
```

### Windows with MinGW/MSYS2: Using Bash Scripts

If you have MinGW/MSYS2 installed, you can use the Linux bash scripts directly on Windows.

**Running the Analysis Script from PowerShell:**
```powershell
# Method 1: Direct invocation
& "C:\msys64\usr\bin\bash.exe" ".\analyze_codebase.sh" "C:/Projects/myproject/src" "C:/Projects/myproject/analysis"

# Method 2: Using a wrapper function
function Invoke-Analysis {
    param(
        [string]$SourceDir = ".",
        [string]$OutputDir = "./analysis_output"
    )
    
    # Convert paths to forward slashes
    $src = $SourceDir -replace '\\', '/'
    $out = $OutputDir -replace '\\', '/'
    
    & "C:\msys64\usr\bin\bash.exe" "-c" "./analyze_codebase.sh '$src' '$out'"
}

Invoke-Analysis -SourceDir "C:\Projects\myproject\src" -OutputDir "C:\Projects\analysis"
```

**Running from CMD:**
```batch
:: Set PATH to include MSYS2
set PATH=%PATH%;C:\msys64\usr\bin;C:\msys64\ucrt64\bin

:: Run the script
bash analyze_codebase.sh C:/Projects/myproject/src C:/Projects/analysis

:: Or with explicit path
C:\msys64\usr\bin\bash.exe analyze_codebase.sh src analysis_output
```

**Running from MSYS2 Terminal:**
```bash
# Open MSYS2 UCRT64 terminal, navigate to project
cd /c/Projects/myproject

# Run script (same as Linux)
./analyze_codebase.sh src analysis_output

# Or with lookup script
./lookup_symbol.sh vfsList src
```

**Creating a Windows Batch Wrapper:**

Save as `analyze.bat`:
```batch
@echo off
setlocal

set BASH_EXE=C:\msys64\usr\bin\bash.exe
set SCRIPT_DIR=%~dp0

if not exist "%BASH_EXE%" (
    echo Error: MSYS2 not found at %BASH_EXE%
    echo Install MSYS2 from https://www.msys2.org/
    exit /b 1
)

:: Convert Windows paths to Unix-style
set SRC_DIR=%1
set OUT_DIR=%2
set SRC_DIR=%SRC_DIR:\=/%
set OUT_DIR=%OUT_DIR:\=/%

"%BASH_EXE%" "%SCRIPT_DIR%analyze_codebase.sh" "%SRC_DIR%" "%OUT_DIR%"
```

Usage:
```batch
analyze.bat C:\Projects\myproject\src C:\Projects\analysis
```

**Creating a PowerShell Module:**

Save as `MigrationAnalysis.psm1`:
```powershell
$script:BashPaths = @(
    "C:\msys64\usr\bin\bash.exe",
    "C:\Program Files\Git\usr\bin\bash.exe",
    "$env:LOCALAPPDATA\Programs\Git\usr\bin\bash.exe"
)

function Get-BashPath {
    foreach ($path in $script:BashPaths) {
        if (Test-Path $path) {
            return $path
        }
    }
    throw "No bash installation found. Install MSYS2 or Git for Windows."
}

function Invoke-CodebaseAnalysis {
    param(
        [Parameter(Mandatory=$true)]
        [string]$SourceDir,
        
        [string]$OutputDir = "./analysis_output",
        
        [string]$ScriptPath = "./analyze_codebase.sh"
    )
    
    $bash = Get-BashPath
    $src = $SourceDir -replace '\\', '/'
    $out = $OutputDir -replace '\\', '/'
    $script = $ScriptPath -replace '\\', '/'
    
    & $bash $script $src $out
}

function Invoke-SymbolLookup {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Symbol,
        
        [string]$SourceDir = ".",
        
        [string]$ScriptPath = "./lookup_symbol.sh"
    )
    
    $bash = Get-BashPath
    $src = $SourceDir -replace '\\', '/'
    $script = $ScriptPath -replace '\\', '/'
    
    & $bash $script $Symbol $src
}

Export-ModuleMember -Function Invoke-CodebaseAnalysis, Invoke-SymbolLookup
```

Usage:
```powershell
Import-Module .\MigrationAnalysis.psm1

Invoke-CodebaseAnalysis -SourceDir "C:\Projects\myproject\src"
Invoke-SymbolLookup -Symbol "vfsList" -SourceDir "C:\Projects\myproject\src"
```

### Symbol Lookup Script

**Linux:**
```bash
#!/bin/bash
# lookup_symbol.sh - Find everything about a symbol

SYMBOL="$1"
SRC_DIR="${2:-.}"

if [ -z "$SYMBOL" ]; then
    echo "Usage: $0 <symbol> [source_dir]"
    exit 1
fi

echo "=== Symbol Analysis: $SYMBOL ==="
echo ""

echo "### Definition:"
grep -rn "^\(static\s\+\)\?[a-zA-Z_].*\b${SYMBOL}\b\s*[=;(]" \
    --include="*.c" --include="*.h" "$SRC_DIR" | head -5

echo ""
echo "### All References:"
grep -rn "\b${SYMBOL}\b" --include="*.c" "$SRC_DIR" | head -20

echo ""
echo "### Files Involved:"
grep -rl "\b${SYMBOL}\b" --include="*.c" "$SRC_DIR" | sort -u

echo ""
echo "### Mutex Context:"
grep -rn -B3 -A3 "\b${SYMBOL}\b" --include="*.c" "$SRC_DIR" | \
    grep -i "mutex\|lock\|critical" | head -10

echo ""
echo "### Statistics:"
REFS=$(grep -rn "\b${SYMBOL}\b" --include="*.c" "$SRC_DIR" | wc -l)
FILES=$(grep -rl "\b${SYMBOL}\b" --include="*.c" "$SRC_DIR" | wc -l)
echo "Total references: $REFS"
echo "Files involved: $FILES"

if [ "$FILES" -le 3 ]; then
    echo "Coupling: LOW - good migration candidate"
elif [ "$FILES" -le 10 ]; then
    echo "Coupling: MEDIUM"
else
    echo "Coupling: HIGH - migration will be complex"
fi
```

**PowerShell:**
```powershell
# Lookup-Symbol.ps1

param(
    [Parameter(Mandatory=$true)]
    [string]$Symbol,
    
    [string]$SourceDir = "."
)

Write-Host "=== Symbol Analysis: $Symbol ===" -ForegroundColor Cyan
Write-Host ""

Write-Host "### Definition:" -ForegroundColor Yellow
Get-ChildItem -Path $SourceDir -Include "*.c", "*.h" -Recurse |
    Select-String -Pattern "(static\s+)?[a-zA-Z_].*\b$Symbol\b\s*[=;(]" |
    Select-Object -First 5

Write-Host ""
Write-Host "### All References:" -ForegroundColor Yellow
Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Select-Object -First 20

Write-Host ""
Write-Host "### Files Involved:" -ForegroundColor Yellow
Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b" |
    Select-Object -ExpandProperty Path -Unique

Write-Host ""
Write-Host "### Mutex Context:" -ForegroundColor Yellow
Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b" -Context 3,3 |
    Where-Object { $_.Context.PreContext + $_.Context.PostContext -match "mutex|lock|critical" } |
    Select-Object -First 10

Write-Host ""
Write-Host "### Statistics:" -ForegroundColor Yellow
$refs = Get-ChildItem -Path $SourceDir -Filter "*.c" -Recurse |
    Select-String -Pattern "\b$Symbol\b"
$refCount = ($refs | Measure-Object).Count
$fileCount = ($refs | Select-Object -ExpandProperty Path -Unique | Measure-Object).Count

Write-Host "Total references: $refCount"
Write-Host "Files involved: $fileCount"

if ($fileCount -le 3) {
    Write-Host "Coupling: LOW - good migration candidate" -ForegroundColor Green
} elseif ($fileCount -le 10) {
    Write-Host "Coupling: MEDIUM" -ForegroundColor Yellow
} else {
    Write-Host "Coupling: HIGH - migration will be complex" -ForegroundColor Red
}
```

---

## Editor Plugins and Integration

### Vim Configuration

Add to `~/.vimrc`:

```vim
" === Migration Analysis Helpers ===

" Find all references to word under cursor
nnoremap <leader>fr :execute 'vimgrep /\<' . expand('<cword>') . '\>/gj **/*.c **/*.h' <Bar> copen<CR>

" Find definition of word under cursor
nnoremap <leader>fd :execute 'vimgrep /^\s*\(static\s\+\)\?\w.*\<' . expand('<cword>') . '\>\s*[=;(]/gj **/*.c **/*.h' <Bar> copen<CR>

" Find globals (all static variables)
nnoremap <leader>fg :vimgrep /^static\s\+\w/gj **/*.c <Bar> copen<CR>

" Find mutex patterns
nnoremap <leader>fm :vimgrep /mutex\|lock\|CRITICAL_SECTION/gj **/*.c **/*.h <Bar> copen<CR>

" Count references to word under cursor
nnoremap <leader>fc :execute '!grep -r "\<' . expand('<cword>') . '\>" --include="*.c" . \| wc -l'<CR>

" Highlight all globals in current buffer
function! HighlightGlobals()
    syntax match GlobalVar /^static\s\+\w\+.*\s\+\zs\w\+\ze\s*[=;]/
    highlight GlobalVar ctermfg=Red guifg=Red
endfunction
command! HighlightGlobals call HighlightGlobals()

" Quick symbol lookup
function! SymbolLookup()
    let symbol = expand('<cword>')
    execute '!./lookup_symbol.sh ' . symbol
endfunction
nnoremap <leader>sl :call SymbolLookup()<CR>
```

### VS Code Settings and Tasks

`.vscode/tasks.json`:
```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Find References",
            "type": "shell",
            "command": "grep -rn \"\\b${input:symbol}\\b\" --include=\"*.c\" --include=\"*.h\" src/",
            "problemMatcher": [],
            "presentation": {
                "reveal": "always",
                "panel": "new"
            }
        },
        {
            "label": "Find Globals",
            "type": "shell",
            "command": "grep -rn \"^static\\s\" --include=\"*.c\" src/ | grep -v \"static\\s\\+inline\"",
            "problemMatcher": [],
            "presentation": {
                "reveal": "always",
                "panel": "new"
            }
        },
        {
            "label": "Find Mutex Patterns",
            "type": "shell",
            "command": "grep -rn \"mutex\\|lock\\|CRITICAL_SECTION\" --include=\"*.c\" --include=\"*.h\" src/",
            "problemMatcher": [],
            "presentation": {
                "reveal": "always",
                "panel": "new"
            }
        },
        {
            "label": "Analyze Symbol",
            "type": "shell",
            "command": "${workspaceFolder}/scripts/lookup_symbol.sh ${input:symbol}",
            "problemMatcher": [],
            "presentation": {
                "reveal": "always",
                "panel": "new"
            }
        },
        {
            "label": "Full Analysis",
            "type": "shell",
            "command": "${workspaceFolder}/scripts/analyze_codebase.sh ${workspaceFolder}/src ${workspaceFolder}/analysis_output",
            "problemMatcher": [],
            "presentation": {
                "reveal": "always",
                "panel": "new"
            }
        }
    ],
    "inputs": [
        {
            "id": "symbol",
            "description": "Symbol name to search",
            "default": "",
            "type": "promptString"
        }
    ]
}
```

`.vscode/keybindings.json` (user keybindings):
```json
[
    {
        "key": "ctrl+shift+r",
        "command": "workbench.action.tasks.runTask",
        "args": "Find References"
    },
    {
        "key": "ctrl+shift+g",
        "command": "workbench.action.tasks.runTask",
        "args": "Find Globals"
    },
    {
        "key": "ctrl+shift+m",
        "command": "workbench.action.tasks.runTask",
        "args": "Find Mutex Patterns"
    }
]
```

### VS Code Extension: Code Analysis Snippets

Create `.vscode/migration-analysis.code-snippets`:
```json
{
    "Grep for symbol": {
        "scope": "shellscript,powershell",
        "prefix": "grep-symbol",
        "body": [
            "grep -rn \"\\\\b${1:symbol}\\\\b\" --include=\"*.c\" --include=\"*.h\" ${2:src/}"
        ]
    },
    "Find static globals": {
        "scope": "shellscript,powershell",
        "prefix": "find-globals",
        "body": [
            "grep -rn \"^static\\\\s\" --include=\"*.c\" ${1:src/} | grep -v \"static\\\\s\\\\+inline\""
        ]
    },
    "PowerShell find references": {
        "scope": "powershell",
        "prefix": "ps-find-refs",
        "body": [
            "Get-ChildItem -Path ${1:src} -Include \"*.c\", \"*.h\" -Recurse |",
            "    Select-String -Pattern \"\\\\b${2:symbol}\\\\b\""
        ]
    },
    "PowerShell find globals": {
        "scope": "powershell",
        "prefix": "ps-find-globals",
        "body": [
            "Get-ChildItem -Path ${1:src} -Filter \"*.c\" -Recurse |",
            "    Select-String -Pattern \"^static\\\\s+[a-zA-Z_]\" |",
            "    Where-Object { \\$_ -notmatch \"static\\\\s+(inline|void|int)\\\\s+\\\\w+\\\\s*\\\\(\" }"
        ]
    }
}
```

### Emacs Configuration

Add to `~/.emacs` or `~/.emacs.d/init.el`:

```elisp
;; === Migration Analysis Helpers ===

(defun migration-find-references ()
  "Find all references to symbol at point."
  (interactive)
  (let ((symbol (thing-at-point 'symbol)))
    (grep (format "grep -rn \"\\b%s\\b\" --include=\"*.c\" --include=\"*.h\" ." symbol))))

(defun migration-find-globals ()
  "Find all static global variables."
  (interactive)
  (grep "grep -rn \"^static\\s\" --include=\"*.c\" . | grep -v \"static\\s\\+inline\""))

(defun migration-find-mutex ()
  "Find mutex patterns."
  (interactive)
  (grep "grep -rn \"mutex\\|lock\\|CRITICAL_SECTION\" --include=\"*.c\" --include=\"*.h\" ."))

(defun migration-symbol-lookup ()
  "Run full symbol analysis."
  (interactive)
  (let ((symbol (thing-at-point 'symbol)))
    (compile (format "./lookup_symbol.sh %s" symbol))))

;; Keybindings
(global-set-key (kbd "C-c m r") 'migration-find-references)
(global-set-key (kbd "C-c m g") 'migration-find-globals)
(global-set-key (kbd "C-c m m") 'migration-find-mutex)
(global-set-key (kbd "C-c m s") 'migration-symbol-lookup)

;; Highlight static globals
(add-hook 'c-mode-hook
          (lambda ()
            (font-lock-add-keywords nil
              '(("^static\\s-+\\w+.*\\s-+\\(\\w+\\)\\s-*[=;]" 1 font-lock-warning-face)))))
```

### JetBrains (CLion) Live Templates

Go to Settings → Editor → Live Templates → Create new template:

**grep-refs:**
```
grep -rn "\b$SYMBOL$\b" --include="*.c" --include="*.h" $DIR$
```

**find-globals:**
```
grep -rn "^static\s" --include="*.c" $DIR$ | grep -v "static\s\+inline"
```

**ps-refs:**
```
Get-ChildItem -Path $DIR$ -Include "*.c", "*.h" -Recurse | Select-String -Pattern "\b$SYMBOL$\b"
```

### Git Hook for Pre-Commit Analysis

`.git/hooks/pre-commit`:
```bash
#!/bin/bash
# Check for new globals in staged files

echo "Checking for new global variables..."

# Get list of staged .c files
STAGED_C_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep '\.c$')

if [ -z "$STAGED_C_FILES" ]; then
    exit 0
fi

# Check for new static globals
NEW_GLOBALS=$(echo "$STAGED_C_FILES" | xargs grep -n "^static\s" 2>/dev/null | \
    grep -v "static\s\+inline\|static\s\+void\s\+\|static\s\+int\s\+[a-z].*(" || true)

if [ -n "$NEW_GLOBALS" ]; then
    echo ""
    echo "WARNING: New static global variables detected:"
    echo "$NEW_GLOBALS"
    echo ""
    echo "Consider whether these should be migrated to ServiceLocator."
    echo "To bypass this check, use: git commit --no-verify"
    echo ""
    
    # Uncomment to block commit:
    # exit 1
fi

exit 0
```

---

## Complete Analysis Workflow

### Quick Triage (30 minutes)

```bash
# 1. Find globals count
grep -rc "^static\s" --include="*.c" src/ | awk -F: '{sum += $2} END {print "Static globals:", sum}'

# 2. Find mutex usage
grep -rc "mutex\|lock" --include="*.c" src/ | awk -F: '{sum += $2} END {print "Mutex lines:", sum}'

# 3. Largest files (most likely to have globals)
find src -name "*.c" -exec wc -l {} \; | sort -rn | head -10

# 4. Quick coupling check for suspected global
SYMBOL="globalConfig"
echo "Files using $SYMBOL:"
grep -rl "\b${SYMBOL}\b" --include="*.c" src/ | wc -l
```

### Standard Analysis (Half Day)

```bash
# Run full analysis script
./analyze_codebase.sh src/ analysis_output/

# Review output files
less analysis_output/SUMMARY.md
less analysis_output/globals.md
less analysis_output/xrefs.md

# Deep dive on high-coupling globals
for global in $(head -10 analysis_output/xrefs.md | tail -8 | awk '{print $2}'); do
    ./lookup_symbol.sh "$global" >> analysis_output/deep_dive.md
done
```

### Thorough Analysis (Multiple Days)

```bash
# Day 1: Inventory
./analyze_codebase.sh src/ analysis_output/

# Day 2: Deep analysis of each global
cat analysis_output/globals.md | grep -v "^#\|^\`" | while read line; do
    global=$(echo "$line" | grep -oE "[a-zA-Z_][a-zA-Z0-9_]*\s*[=;]" | sed 's/[=;]//;s/\s//g')
    if [ -n "$global" ]; then
        echo "=== $global ===" >> analysis_output/all_symbols.md
        ./lookup_symbol.sh "$global" >> analysis_output/all_symbols.md
    fi
done

# Day 3: Thread safety review
grep -rn -B5 -A10 "pthread_mutex_lock\|EnterCriticalSection" --include="*.c" src/ > analysis_output/lock_regions.txt

# Day 4: Decision making
# Review all output, create migration plan
```

---

## Limitations and Workarounds

| Limitation | Impact | Workaround |
|------------|--------|------------|
| Macros hide globals | Miss `#define`-based globals | Search for macro patterns too |
| Comments matched | False positives | Exclude `//` and `/* */` lines |
| String literals matched | False positives | Manual review |
| No AST understanding | Miss complex patterns | Supplement with clang-query when available |
| No thread analysis | Can't prove safety | Manual review + runtime testing |
| Slow on large codebases | Time consuming | Use ripgrep, limit scope |

### Macro-Expanded Globals

```bash
# Find potential macro globals
grep -rn "^#define.*\bglobal\b\|^#define.*\bstatic\b" --include="*.h" src/

# Check for DECLARE_* macros
grep -rn "^#define\s\+DECLARE_" --include="*.h" src/

# Expand macros and search (requires gcc)
gcc -E -I include src/main.c | grep "^static"
```

### Excluding Comments

```bash
# Simple heuristic (not perfect)
grep -rn "^static" --include="*.c" src/ | \
    grep -v "^\s*//" | \
    grep -v "^\s*/\*" | \
    grep -v "^\s*\*"

# Better: use cpp to strip comments
cpp -fpreprocessed src/file.c | grep "^static"
```

### Performance Tips

```bash
# Use ripgrep instead of grep
rg "^static" --type c src/

# Limit search depth
find src -maxdepth 2 -name "*.c" -exec grep -l "pattern" {} \;

# Search specific directories
grep -rn "pattern" src/core/ src/util/

# Parallel processing
find src -name "*.c" | xargs -P4 grep -l "pattern"
```

---

## Quick Reference Card

### Bash One-Liners

```bash
# Find static globals
grep -rn "^static\s\+[a-zA-Z_]" --include="*.c" src/

# Count references
grep -c "\bsymbol\b" **/*.c

# Files containing symbol
grep -rl "\bsymbol\b" --include="*.c" src/

# Mutex patterns
grep -rn "mutex\|lock" --include="*.c" src/

# Near mutex context
grep -rn -B3 -A3 "\bsymbol\b" --include="*.c" src/ | grep -i mutex
```

### PowerShell One-Liners

```powershell
# Find static globals
gci src -r -fi *.c | sls "^static\s+[a-zA-Z_]"

# Count references
(gci src -r -fi *.c | sls "\bsymbol\b").Count

# Files containing symbol
gci src -r -fi *.c | sls "\bsymbol\b" | select -u Path

# Mutex patterns
gci src -r -fi *.c,*.h | sls "mutex|lock"

# Near mutex context
gci src -r -fi *.c | sls "\bsymbol\b" -co 3,3 | ? { $_.Context -match "mutex" }
```

### MinGW/MSYS2 from PowerShell

```powershell
# Set up bash alias (add to $PROFILE)
$bashExe = "C:\msys64\usr\bin\bash.exe"
if (-not (Test-Path $bashExe)) { $bashExe = "C:\Program Files\Git\usr\bin\bash.exe" }
function bash { & $bashExe -c $args }

# Find static globals
bash "grep -rn '^static\s' --include='*.c' src/"

# Count references  
bash "grep -c '\bsymbol\b' src/*.c"

# Run analysis script
bash "./analyze_codebase.sh src analysis_output"

# Symbol lookup
bash "./lookup_symbol.sh vfsList src"
```

### MinGW/MSYS2 from CMD

```batch
:: Add to PATH (one-time in session)
set PATH=%PATH%;C:\msys64\ucrt64\bin;C:\msys64\usr\bin

:: Find static globals
grep -rn "^static\s" --include="*.c" src/

:: Count references
grep -c "\bsymbol\b" src/*.c

:: Run analysis script
bash analyze_codebase.sh src analysis_output
```

### Git Bash One-Liners

```bash
# These work in Git Bash terminal on Windows
# Same syntax as Linux

# Find static globals
grep -rn "^static\s" --include="*.c" src/

# Mutex patterns
grep -rn "mutex\|lock\|CRITICAL_SECTION" --include="*.c" src/

# Note: ripgrep/ctags not included, install separately:
# choco install ripgrep universal-ctags
```

### Common Regex Patterns

| Pattern | Matches |
|---------|---------|
| `^static\s+` | Static declarations at line start |
| `\b\w+\b` | Word (symbol) |
| `\w+\s*=` | Assignment |
| `\w+\s*;` | Declaration end |
| `\w+\s*\(` | Function call |
| `pthread_mutex_\w+` | POSIX mutex functions |

---

*FAT-P Library — User Manual UM-MIGRATION-ANALYSIS-003*  
*Last updated: January 2025*
