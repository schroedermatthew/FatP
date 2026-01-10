---
doc_id: UM-MIGRATION-ANALYSIS-005
doc_type: "User Manual"
title: "Analyzing C Code for Migration (Open Source Tools - Windows/MinGW)"
fatp_components: ["ServiceLocator"]
topics: ["global state", "C migration", "codebase analysis", "doxygen", "cscope", "Windows", "MinGW", "MSYS2", "static analysis"]
constraints: ["legacy C code", "Windows environment", "MinGW toolchain", "open source tools"]
cxx_standard: "C++17"
last_verified: "2025-01-09"
audience: ["C developers", "C++ developers", "Windows developers", "AI assistants"]
status: "reviewed"
---

# User Manual - Analyzing C Code for Migration (Open Source Tools - Windows/MinGW)

### *Deep analysis using Doxygen, cscope, and static analyzers on Windows*

*FAT-P Library — January 2025*

---

## User Manual Card

**What this does:** Deep analysis of C code using specialized tools on Windows  
**Platform:** Windows 10/11 with MSYS2/MinGW or Visual Studio  
**Time required:** 2-4 hours setup, then 1-3 days analysis  
**Tools covered:** Doxygen, cscope, Dr. Memory, Cppcheck, clang-tidy  
**Skills assumed:** Basic Windows command line, can install software  
**Skills NOT assumed:** Tool-specific knowledge, Unix experience  
**Key difference from Linux:** Thread Sanitizer (TSan) is NOT available on Windows; alternatives provided  
**Prerequisite:** Complete grep-based analysis first (User Manual UM-MIGRATION-ANALYSIS-002 or 003)  
**Read next:** Migration Guide - Global State to ServiceLocator

---

## Table of Contents

1. [Windows Analysis: What's Different](#chapter-1-windows-analysis-whats-different)
2. [Choosing Your Windows Environment](#chapter-2-choosing-your-windows-environment)
3. [Setting Up MSYS2/MinGW](#chapter-3-setting-up-msys2mingw)
4. [Installing Analysis Tools](#chapter-4-installing-analysis-tools)
5. [Doxygen: Call Graphs and Documentation](#chapter-5-doxygen-call-graphs-and-documentation)
6. [cscope: Cross-Reference Database](#chapter-6-cscope-cross-reference-database)
7. [Thread Safety Analysis (Windows Alternatives)](#chapter-7-thread-safety-analysis-windows-alternatives)
8. [Static Analysis with Cppcheck and clang-tidy](#chapter-8-static-analysis-with-cppcheck-and-clang-tidy)
9. [Using Visual Studio Analysis Tools](#chapter-9-using-visual-studio-analysis-tools)
10. [Combining Tools for Complete Analysis](#chapter-10-combining-tools-for-complete-analysis)
11. [Worked Example](#chapter-11-worked-example)
12. [Troubleshooting](#chapter-12-troubleshooting)
13. [FAQ](#faq)
14. [Glossary](#glossary)
15. [Quick Reference](#quick-reference)

---

# Chapter 1: Windows Analysis: What's Different

## Linux vs Windows Tool Availability

| Tool | Linux | Windows | Windows Alternative |
|------|-------|---------|---------------------|
| **Doxygen** | ✓ Native | ✓ Native | — |
| **cscope** | ✓ Native | ✓ MSYS2 | — |
| **Thread Sanitizer** | ✓ Native | ✗ Not available | Dr. Memory, App Verifier |
| **Infer** | ✓ Native | ✗ Not available | Cppcheck, PVS-Studio |
| **clang-query** | ✓ Native | ~ Partial | clang-tidy |
| **Cppcheck** | ✓ Native | ✓ Native | — |

## The Key Limitation: No TSan on Windows

**Thread Sanitizer (TSan) does not work on Windows.** This is a significant gap because TSan provides runtime proof of data races.

**Windows alternatives:**
- **Dr. Memory** — Runtime memory and race detection
- **Application Verifier** — Microsoft's runtime verification tool
- **Intel Inspector** — Commercial, but free for open source
- **Static analysis** — Cppcheck, clang-tidy (find potential races without running code)

## When Windows Is Fine

Windows tools are perfectly adequate for:
- Call graph generation (Doxygen)
- Cross-reference queries (cscope)
- Static bug finding (Cppcheck, clang-tidy)
- Basic race detection (Dr. Memory)

## When to Consider WSL or a Linux VM

If you need TSan or Infer:
1. **WSL (Windows Subsystem for Linux):** Full Linux in Windows
2. **Docker:** Run Linux tools in containers
3. **Virtual machine:** VirtualBox/VMware with Linux

This manual covers native Windows tools. For Linux tools on Windows, see the Linux manual with WSL.

---

# Chapter 2: Choosing Your Windows Environment

## Option 1: MSYS2/MinGW (Recommended)

MSYS2 provides a Unix-like environment with package management. It includes:
- MinGW compilers (gcc, clang)
- Unix tools (grep, find, bash)
- Easy package installation (pacman)

**Best for:** Developers who want Unix tools on Windows.

## Option 2: Native Windows + Chocolatey

Use native Windows tools installed via Chocolatey package manager.

**Best for:** Developers who prefer native Windows.

## Option 3: Visual Studio

Use Visual Studio's built-in analysis tools.

**Best for:** Developers already using Visual Studio.

## Option 4: WSL (Windows Subsystem for Linux)

Run full Linux inside Windows.

**Best for:** When you need Linux-only tools (TSan, Infer).

## This Manual's Focus

This manual primarily covers **MSYS2/MinGW** because:
- Most analysis tools are available
- Familiar to Unix developers
- Scripts from Linux manual work with minor changes

Native Windows and Visual Studio sections are also included.

---

# Chapter 3: Setting Up MSYS2/MinGW

## Installing MSYS2

### Method 1: Direct Download (Recommended)

1. Go to https://www.msys2.org/
2. Download the installer (msys2-x86_64-xxxxxxxx.exe)
3. Run the installer
4. Accept default installation path: `C:\msys64`
5. Complete installation

### Method 2: Using winget

```powershell
winget install MSYS2.MSYS2
```

### Method 3: Using Chocolatey

```powershell
# As Administrator
choco install msys2
```

## First-Time MSYS2 Setup

After installation, open "MSYS2 UCRT64" from the Start menu (not "MSYS2 MSYS").

```bash
# Update package database
pacman -Syu

# If it asks to close the terminal, close it and reopen, then:
pacman -Su
```

## Installing Base Development Tools

```bash
# Install compilers and basic tools
pacman -S --noconfirm \
    mingw-w64-ucrt-x86_64-gcc \
    mingw-w64-ucrt-x86_64-clang \
    mingw-w64-ucrt-x86_64-make \
    make \
    git \
    base-devel
```

## Understanding MSYS2 Environments

MSYS2 has multiple environments:

| Environment | Use For | Start Menu Name |
|-------------|---------|-----------------|
| **UCRT64** | Modern Windows development (recommended) | MSYS2 UCRT64 |
| MINGW64 | Legacy MinGW development | MSYS2 MINGW64 |
| CLANG64 | Clang-based development | MSYS2 CLANG64 |
| MSYS | Unix tools only (no Windows integration) | MSYS2 MSYS |

**Always use UCRT64** for this manual.

## Verify Your Setup

```bash
# Check compilers
gcc --version
clang --version

# Check make
make --version

# Check you're in UCRT64
echo $MINGW_PREFIX
# Should show: /ucrt64
```

## Adding MSYS2 to Windows PATH (Optional)

If you want to run MSYS2 tools from PowerShell or CMD:

1. Open System Properties → Advanced → Environment Variables
2. Edit PATH
3. Add: `C:\msys64\ucrt64\bin`
4. Add: `C:\msys64\usr\bin`

Or via PowerShell (as Administrator):
```powershell
[Environment]::SetEnvironmentVariable("Path", $env:Path + ";C:\msys64\ucrt64\bin;C:\msys64\usr\bin", "Machine")
```

## Navigating to Your Project

In MSYS2, Windows paths work but use forward slashes:

```bash
# Windows path: C:\Users\YourName\Projects\myapp
cd /c/Users/YourName/Projects/myapp

# Or with spaces (use quotes):
cd "/c/Users/Your Name/My Projects"
```

---

# Chapter 4: Installing Analysis Tools

## 4.1 Doxygen (MSYS2)

```bash
# In MSYS2 UCRT64
pacman -S mingw-w64-ucrt-x86_64-doxygen mingw-w64-ucrt-x86_64-graphviz
```

### Verify:

```bash
doxygen --version
dot -V
```

### Native Windows Alternative (Chocolatey):

```powershell
# In PowerShell as Administrator
choco install doxygen.install graphviz
```

### If Installation Fails:

**"Package not found" in MSYS2:**
```bash
# Update package database
pacman -Sy
pacman -Ss doxygen  # Search for correct package name
```

**Download directly:**
1. Go to https://www.doxygen.nl/download.html
2. Download Windows installer
3. Install to C:\Program Files\doxygen
4. Add to PATH manually

---

## 4.2 cscope (MSYS2)

```bash
# In MSYS2 UCRT64
pacman -S cscope
```

### Verify:

```bash
cscope --version
```

### If Not in UCRT64 Repos:

```bash
# Try the MSYS repo
pacman -S cscope

# Or build from source
pacman -S base-devel ncurses-devel
wget https://sourceforge.net/projects/cscope/files/cscope/v15.9/cscope-15.9.tar.gz
tar xzf cscope-15.9.tar.gz
cd cscope-15.9
./configure
make
make install
```

### Native Windows:

cscope doesn't have a native Windows build. Options:
- Use MSYS2 (recommended)
- Use WSL
- Use alternative tools (Visual Studio "Find All References")

---

## 4.3 Cppcheck (Static Analysis)

Cppcheck is an excellent static analyzer that works natively on Windows.

### MSYS2:

```bash
pacman -S mingw-w64-ucrt-x86_64-cppcheck
```

### Chocolatey:

```powershell
choco install cppcheck
```

### Direct Download:

1. Go to https://cppcheck.sourceforge.io/
2. Download Windows installer
3. Install

### Verify:

```bash
cppcheck --version
```

---

## 4.4 clang-tidy

clang-tidy provides lint checks and can detect some thread safety issues through annotations.

### MSYS2:

```bash
pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra
```

### Verify:

```bash
clang-tidy --version
```

### With Visual Studio:

clang-tidy is included with Visual Studio 2019+ with the "C++ Clang tools" component.

---

## 4.5 Dr. Memory (Thread/Memory Analysis)

Dr. Memory is a Windows alternative to Valgrind/TSan.

### Download:

1. Go to https://drmemory.org/
2. Download the latest release
3. Extract to `C:\DrMemory`
4. Add `C:\DrMemory\bin64` to PATH

### Verify:

```powershell
drmemory --version
```

### Chocolatey:

```powershell
choco install drmemory
```

---

## 4.6 Application Verifier (Microsoft)

Application Verifier is Microsoft's free runtime verification tool.

### Installation:

1. Download Windows SDK from https://developer.microsoft.com/windows/downloads/windows-sdk/
2. During installation, select "Application Verifier"
3. Or install via Visual Studio Installer

### Verify:

Application Verifier is a GUI tool. Search for "Application Verifier" in Start menu.

---

## Installation Summary Script

Save as `check_tools.sh` (run in MSYS2):

```bash
#!/bin/bash
echo "========================================"
echo "  Windows Analysis Tools Check"
echo "========================================"
echo ""

check_tool() {
    printf "%-15s: " "$1"
    if command -v "$2" &>/dev/null; then
        echo "OK"
        return 0
    else
        echo "NOT FOUND - $3"
        return 1
    fi
}

echo "Essential tools:"
check_tool "gcc" "gcc" "pacman -S mingw-w64-ucrt-x86_64-gcc"
check_tool "clang" "clang" "pacman -S mingw-w64-ucrt-x86_64-clang"
check_tool "make" "make" "pacman -S make"

echo ""
echo "Analysis tools:"
check_tool "Doxygen" "doxygen" "pacman -S mingw-w64-ucrt-x86_64-doxygen"
check_tool "Graphviz" "dot" "pacman -S mingw-w64-ucrt-x86_64-graphviz"
check_tool "cscope" "cscope" "pacman -S cscope"
check_tool "Cppcheck" "cppcheck" "pacman -S mingw-w64-ucrt-x86_64-cppcheck"
check_tool "clang-tidy" "clang-tidy" "pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra"

echo ""
echo "Windows-specific:"
if [ -f "/c/DrMemory/bin64/drmemory.exe" ] || command -v drmemory &>/dev/null; then
    echo "Dr. Memory    : OK"
else
    echo "Dr. Memory    : NOT FOUND - download from drmemory.org"
fi

echo ""
echo "========================================"
```

---

# Chapter 5: Doxygen: Call Graphs and Documentation

Doxygen works identically on Windows and Linux. This chapter covers Windows-specific considerations.

## Creating Configuration

```bash
# In MSYS2, navigate to your project
cd /c/Users/YourName/Projects/myapp

# Create config
mkdir -p migration_analysis/doxygen
doxygen -g migration_analysis/doxygen/Doxyfile
```

## Configuring for Windows Paths

Edit `Doxyfile` and adjust paths. Use forward slashes or escaped backslashes:

```
# Forward slashes (recommended)
INPUT = src/ include/
OUTPUT_DIRECTORY = migration_analysis/doxygen/output

# Or Windows-style (escape backslashes)
INPUT = src\ include\
```

## Minimal Configuration

Create `migration_analysis/doxygen/Doxyfile`:

```
PROJECT_NAME           = "Migration Analysis"
OUTPUT_DIRECTORY       = migration_analysis/doxygen/output
INPUT                  = src include
FILE_PATTERNS          = *.c *.h
RECURSIVE              = YES
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
HAVE_DOT               = YES
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
DOT_IMAGE_FORMAT       = svg
```

## Running Doxygen

```bash
doxygen migration_analysis/doxygen/Doxyfile
```

## Viewing Results

### From MSYS2:

```bash
# Open in default browser
start migration_analysis/doxygen/output/html/index.html
```

### From Windows Explorer:

Navigate to `migration_analysis\doxygen\output\html\` and double-click `index.html`.

## Troubleshooting Doxygen on Windows

### "dot not found"

Graphviz not installed or not in PATH:
```bash
# MSYS2
pacman -S mingw-w64-ucrt-x86_64-graphviz

# Or add to PATH in Doxyfile:
DOT_PATH = C:/msys64/ucrt64/bin
```

### Graphs not generating

Check Graphviz is working:
```bash
echo "digraph { A -> B }" | dot -Tpng -o test.png
start test.png
```

### Long paths fail

Windows has path length limits. Use shorter paths or enable long paths:
```powershell
# PowerShell as Administrator
New-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem" -Name "LongPathsEnabled" -Value 1 -PropertyType DWORD -Force
```

---

# Chapter 6: cscope: Cross-Reference Database

## Building the Database (MSYS2)

```bash
cd /c/Users/YourName/Projects/myapp

# Create file list
find src include -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files

# Build database
cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
```

## Using cscope

```bash
cscope -d -f migration_analysis/cscope/cscope.out
```

Navigation and queries work the same as Linux.

## Windows Path Considerations

cscope stores paths from the file list. Ensure consistent path format:

```bash
# Convert to Unix-style paths
find src include -name "*.c" -o -name "*.h" | sed 's|\\|/|g' > cscope.files
```

## Editor Integration (Windows)

### Visual Studio Code:

1. Install "cscope" extension
2. Set database path in settings:
```json
{
    "cscope.database": "migration_analysis/cscope/cscope.out"
}
```

### Vim (via MSYS2 or gVim):

```vim
" Add to _vimrc
set cscopetag
if filereadable("migration_analysis/cscope/cscope.out")
    cs add migration_analysis/cscope/cscope.out
endif
```

## If cscope Won't Work

Alternative: Use Visual Studio's "Find All References" or VS Code's "Go to References" features. Less powerful but work without additional tools.

---

# Chapter 7: Thread Safety Analysis (Windows Alternatives)

## The TSan Gap

Thread Sanitizer (TSan) is the gold standard for finding data races, but it **does not work on Windows**. This chapter covers alternatives.

## Option 1: Dr. Memory (Recommended)

Dr. Memory includes experimental thread race detection.

### Running Dr. Memory:

```powershell
# Build your app normally
gcc -g -o myapp.exe src/*.c

# Run with Dr. Memory
drmemory -light -- myapp.exe
```

### For thread checking:

```powershell
drmemory -light -check_leaks -check_uninit -- myapp.exe
```

### Interpreting Output:

```
Error #1: UNINITIALIZED READ
# reading uninitialized memory
...

Error #2: LEAK
# memory not freed
```

Dr. Memory's thread race detection is less complete than TSan, but it catches memory issues that often correlate with races.

## Option 2: Application Verifier

Application Verifier is a Microsoft tool that monitors runtime behavior.

### Setup:

1. Open Application Verifier (search in Start menu)
2. File → Add Application
3. Browse to your .exe
4. Check "Basics" and "Locks" tests
5. Click Save

### Running:

Simply run your application normally. Application Verifier monitors in the background.

### Viewing Results:

Errors are logged to Event Viewer (Windows Logs → Application) or appear as debugger breaks.

### Lock Verifier Checks:

- Critical section corruption
- Lock hierarchy violations
- Orphaned critical sections
- Lock contention issues

## Option 3: Intel Inspector (Commercial)

Intel Inspector is comprehensive but commercial. Free for open-source projects.

### Capabilities:

- Memory errors
- Threading errors (data races, deadlocks)

### Usage:

```powershell
inspxe-cl -collect ti3 -- myapp.exe
inspxe-cl -report problems
```

## Option 4: Static Analysis for Thread Safety

When runtime detection isn't available, static analysis helps.

### Cppcheck:

```bash
cppcheck --enable=warning,style,performance --inconclusive src/
```

Cppcheck has limited threading checks but catches obvious issues.

### clang-tidy with thread safety annotations:

If your code uses Clang's thread safety annotations:

```c
// In your code
#include <pthread.h>

pthread_mutex_t g_mutex __attribute__((capability("mutex")));
int g_counter __attribute__((guarded_by(g_mutex)));
```

Then:

```bash
clang-tidy -checks='clang-analyzer-*,bugprone-*,concurrency-*' src/*.c
```

## Option 5: Use WSL for TSan

If you need TSan, use Windows Subsystem for Linux:

```powershell
# Install WSL if not already
wsl --install

# Open WSL
wsl
```

Then follow the Linux manual for TSan.

## Thread Safety Script (Windows)

Save as `migration_analysis/scripts/check_thread_safety.ps1`:

```powershell
# check_thread_safety.ps1 - Run thread safety checks on Windows

param(
    [string]$ExePath = ".\myapp.exe",
    [string]$SourceDir = "src"
)

Write-Host "=== Thread Safety Analysis ===" -ForegroundColor Cyan
Write-Host ""

# Check for tools
$hasDrMemory = Get-Command drmemory -ErrorAction SilentlyContinue
$hasCppcheck = Get-Command cppcheck -ErrorAction SilentlyContinue
$hasClangTidy = Get-Command clang-tidy -ErrorAction SilentlyContinue

# 1. Static analysis with Cppcheck
if ($hasCppcheck) {
    Write-Host "[1] Running Cppcheck..." -ForegroundColor Yellow
    cppcheck --enable=all --inconclusive --quiet $SourceDir 2>&1 | 
        Tee-Object -FilePath "migration_analysis\reports\cppcheck_report.txt"
    Write-Host "  Report: migration_analysis\reports\cppcheck_report.txt"
} else {
    Write-Host "[1] Cppcheck not available - install with: choco install cppcheck" -ForegroundColor Red
}

Write-Host ""

# 2. clang-tidy
if ($hasClangTidy) {
    Write-Host "[2] Running clang-tidy..." -ForegroundColor Yellow
    Get-ChildItem -Path $SourceDir -Filter "*.c" | ForEach-Object {
        clang-tidy $_.FullName -- -I include 2>&1
    } | Tee-Object -FilePath "migration_analysis\reports\clang_tidy_report.txt"
    Write-Host "  Report: migration_analysis\reports\clang_tidy_report.txt"
} else {
    Write-Host "[2] clang-tidy not available" -ForegroundColor Red
}

Write-Host ""

# 3. Dr. Memory (if exe exists)
if ($hasDrMemory -and (Test-Path $ExePath)) {
    Write-Host "[3] Running Dr. Memory..." -ForegroundColor Yellow
    Write-Host "  This will run your application. Make sure it exits on its own."
    Write-Host "  Press Enter to continue or Ctrl+C to skip..."
    Read-Host
    
    drmemory -light -logdir migration_analysis\reports\drmemory -- $ExePath
    Write-Host "  Report: migration_analysis\reports\drmemory\"
} else {
    Write-Host "[3] Dr. Memory: Skipped (not installed or exe not found)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Analysis Complete ===" -ForegroundColor Cyan
```

---

# Chapter 8: Static Analysis with Cppcheck and clang-tidy

## Cppcheck

Cppcheck is a dedicated C/C++ static analyzer.

### Basic Usage:

```bash
# In MSYS2 or PowerShell (if in PATH)
cppcheck --enable=all src/
```

### Comprehensive Analysis:

```bash
cppcheck \
    --enable=all \
    --inconclusive \
    --std=c11 \
    --suppress=missingIncludeSystem \
    -I include \
    --output-file=migration_analysis/reports/cppcheck.txt \
    src/
```

### What Cppcheck Finds:

- Null pointer dereferences
- Memory leaks
- Buffer overflows
- Uninitialized variables
- Style issues

### For Migration Analysis:

Look for globals in Cppcheck output:
```bash
cppcheck --enable=all src/ 2>&1 | grep -i "global\|static"
```

### Cppcheck GUI (Windows):

Cppcheck comes with a GUI. Run `cppcheck-gui` and:
1. File → New Project
2. Add source paths
3. Analyze → Check all

## clang-tidy

clang-tidy provides lint checks and can use Clang's powerful analysis.

### Basic Usage:

```bash
clang-tidy src/main.c -- -I include
```

### Useful Checks for Migration:

```bash
clang-tidy \
    -checks='clang-analyzer-*,bugprone-*,cert-*,concurrency-*' \
    src/*.c \
    -- -I include
```

### With compile_commands.json:

If you have a compilation database:

```bash
clang-tidy -p compile_commands.json src/main.c
```

### Generating compile_commands.json on Windows:

With CMake:
```bash
cmake -G "MinGW Makefiles" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON .
```

With Bear (MSYS2):
```bash
pacman -S bear
bear -- make
```

## Static Analysis Script

Save as `migration_analysis/scripts/static_analysis.sh`:

```bash
#!/bin/bash
# static_analysis.sh - Run static analysis tools

SRC_DIR="${1:-src}"
REPORT_DIR="migration_analysis/reports"

mkdir -p "$REPORT_DIR"

echo "=== Static Analysis ==="
echo ""

# Cppcheck
if command -v cppcheck &>/dev/null; then
    echo "[1/2] Running Cppcheck..."
    cppcheck \
        --enable=all \
        --inconclusive \
        --std=c11 \
        -I include \
        --output-file="$REPORT_DIR/cppcheck.txt" \
        "$SRC_DIR" 2>&1
    
    errors=$(grep -c "error:" "$REPORT_DIR/cppcheck.txt" 2>/dev/null || echo 0)
    warnings=$(grep -c "warning:" "$REPORT_DIR/cppcheck.txt" 2>/dev/null || echo 0)
    echo "  Errors: $errors, Warnings: $warnings"
    echo "  Report: $REPORT_DIR/cppcheck.txt"
else
    echo "[1/2] Cppcheck not available"
fi

echo ""

# clang-tidy
if command -v clang-tidy &>/dev/null; then
    echo "[2/2] Running clang-tidy..."
    > "$REPORT_DIR/clang_tidy.txt"
    
    for src in "$SRC_DIR"/*.c; do
        echo "  Checking: $src"
        clang-tidy \
            -checks='clang-analyzer-*,bugprone-*' \
            "$src" \
            -- -I include 2>&1 >> "$REPORT_DIR/clang_tidy.txt"
    done
    
    issues=$(grep -c "warning:" "$REPORT_DIR/clang_tidy.txt" 2>/dev/null || echo 0)
    echo "  Issues: $issues"
    echo "  Report: $REPORT_DIR/clang_tidy.txt"
else
    echo "[2/2] clang-tidy not available"
fi

echo ""
echo "=== Analysis Complete ==="
```

---

# Chapter 9: Using Visual Studio Analysis Tools

If you have Visual Studio, it includes powerful analysis tools.

## Code Analysis (Built-in)

### Enable Analysis:

1. Open your project in Visual Studio
2. Project → Properties → Code Analysis
3. Enable "Enable Code Analysis on Build"
4. Select ruleset (e.g., "Microsoft Native Recommended Rules")

### Run Manually:

1. Analyze → Run Code Analysis → On Solution

### Relevant Rules for Migration:

- C26100-C26199: Concurrency warnings
- C26400-C26499: Lifetime and ownership
- C6001-C6999: General warnings

## C++ Core Guidelines Checker

Visual Studio can check against C++ Core Guidelines:

1. Project → Properties → Code Analysis
2. Enable "C++ Core Check"

### Useful Guidelines for Global Analysis:

- `I.2`: Avoid non-const global variables
- `I.3`: Avoid singletons
- `CP`: Concurrency guidelines

## Address Sanitizer (ASan) in Visual Studio

Visual Studio 2019+ includes ASan (but not TSan):

1. Project → Properties → C/C++ → General
2. Enable Address Sanitizer: Yes

### Building and Running:

```
Build → Build Solution
Debug → Start Without Debugging
```

ASan catches memory errors but not data races (TSan would be needed for that).

## Exporting Analysis Results

### For Code Analysis:

1. Run analysis
2. View → Error List
3. Copy results or export to CSV

### Creating a Report:

```powershell
# Run MSBuild with analysis
msbuild MyProject.sln /p:RunCodeAnalysis=true /p:CodeAnalysisLogFile=analysis.xml
```

---

# Chapter 10: Combining Tools for Complete Analysis

## Windows Analysis Workflow

```
1. grep/PowerShell analysis (from previous manual)
   ↓ List of globals with basic coupling
   
2. Doxygen
   ↓ Visual call graphs
   
3. cscope (MSYS2)
   ↓ Cross-reference queries
   
4. Cppcheck + clang-tidy
   ↓ Static analysis issues
   
5. Dr. Memory / Application Verifier
   ↓ Runtime issues (limited race detection)
   
6. Combined decision matrix
```

## Master Analysis Script (MSYS2)

Save as `migration_analysis/scripts/full_analysis.sh`:

```bash
#!/bin/bash
# full_analysis.sh - Run all available analysis tools (Windows/MSYS2)

SRC_DIR="${1:-src}"

echo "=========================================="
echo "    Full Migration Analysis (Windows)"
echo "=========================================="
echo "Source: $SRC_DIR"
echo ""

mkdir -p migration_analysis/{reports,doxygen,cscope}

# 1. Doxygen
if command -v doxygen &>/dev/null; then
    echo "[1/4] Running Doxygen..."
    if [ ! -f migration_analysis/doxygen/Doxyfile ]; then
        doxygen -g migration_analysis/doxygen/Doxyfile >/dev/null
        sed -i "s|^INPUT.*=.*|INPUT = $SRC_DIR|" migration_analysis/doxygen/Doxyfile
        sed -i 's/^EXTRACT_ALL.*=.*/EXTRACT_ALL = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's/^HAVE_DOT.*=.*/HAVE_DOT = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's/^CALL_GRAPH.*=.*/CALL_GRAPH = YES/' migration_analysis/doxygen/Doxyfile
        sed -i 's|^OUTPUT_DIRECTORY.*=.*|OUTPUT_DIRECTORY = migration_analysis/doxygen/output|' migration_analysis/doxygen/Doxyfile
    fi
    doxygen migration_analysis/doxygen/Doxyfile 2>/dev/null
    echo "  Output: migration_analysis/doxygen/output/html/index.html"
else
    echo "[1/4] Doxygen: SKIPPED"
fi
echo ""

# 2. cscope
if command -v cscope &>/dev/null; then
    echo "[2/4] Building cscope database..."
    find "$SRC_DIR" -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files
    cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
    echo "  Database: migration_analysis/cscope/cscope.out"
else
    echo "[2/4] cscope: SKIPPED"
fi
echo ""

# 3. Cppcheck
if command -v cppcheck &>/dev/null; then
    echo "[3/4] Running Cppcheck..."
    cppcheck --enable=all --inconclusive -I include \
        --output-file=migration_analysis/reports/cppcheck.txt \
        "$SRC_DIR" 2>&1
    echo "  Report: migration_analysis/reports/cppcheck.txt"
else
    echo "[3/4] Cppcheck: SKIPPED"
fi
echo ""

# 4. clang-tidy
if command -v clang-tidy &>/dev/null; then
    echo "[4/4] Running clang-tidy..."
    > migration_analysis/reports/clang_tidy.txt
    for f in "$SRC_DIR"/*.c; do
        clang-tidy "$f" -- -I include 2>&1 >> migration_analysis/reports/clang_tidy.txt
    done
    echo "  Report: migration_analysis/reports/clang_tidy.txt"
else
    echo "[4/4] clang-tidy: SKIPPED"
fi

echo ""
echo "=========================================="
echo "Analysis complete."
echo ""
echo "Next steps:"
echo "1. Review Doxygen call graphs"
echo "2. Query with cscope"
echo "3. Review static analysis reports"
echo "4. Run Dr. Memory/Application Verifier for runtime issues"
echo "5. Update migration_analysis/reports/decisions.md"
```

---

# Chapter 11: Worked Example

Analyzing a Windows application with threading.

## The Example

```
winapp/
├── src/
│   ├── main.c
│   ├── server.c
│   ├── worker.c
│   ├── database.c
│   └── cache.c
├── include/
└── Makefile
```

Known globals (from PowerShell analysis):
- `g_config` — Configuration
- `g_database` — Database connection
- `g_cache` — Response cache
- `g_stats` — Request statistics
- `g_cs_db` — CRITICAL_SECTION for database

## Step 1: Setup (MSYS2)

```bash
cd /c/Users/Dev/Projects/winapp
./migration_analysis/scripts/full_analysis.sh src
```

## Step 2: Review Doxygen

```bash
start migration_analysis/doxygen/output/html/index.html
```

**Findings:**
- `g_database` accessed by 3 functions
- Call depth: 4 levels from `main()`

## Step 3: cscope Queries

```bash
cscope -d -f migration_analysis/cscope/cscope.out
# Find callers of cache_update
```

**Findings:**
- `cache_update` called from 2 threads: main thread, worker thread

## Step 4: Static Analysis

```bash
cat migration_analysis/reports/cppcheck.txt | grep -i thread
cat migration_analysis/reports/clang_tidy.txt | grep -i concurr
```

**Findings:**
- Warning: `g_stats` modified without synchronization
- Warning: `g_cache` accessed in multiple contexts

## Step 5: Dr. Memory

```bash
# Build
gcc -g -o winapp.exe src/*.c -I include

# Run with Dr. Memory (in PowerShell)
drmemory -light -- ./winapp.exe
```

**Findings:**
- Memory leak in `cache_create()`
- Uninitialized read in `stats_get()`

## Step 6: Decision Matrix

| Global | Coupling | Static Analysis | Dr. Memory | Decision |
|--------|----------|-----------------|------------|----------|
| g_config | LOW | Clean | Clean | MIGRATE |
| g_database | MEDIUM | Clean | Clean | MIGRATE |
| g_cache | MEDIUM | Warning | Clean | INVESTIGATE |
| g_stats | LOW | Warning | Uninit read | FIX + MIGRATE |
| g_cs_db | NONE | N/A | N/A | KEEP |

## Step 7: Final Report

```markdown
# winapp Migration Analysis (Windows)

## Issues Found

### g_stats - Uninitialized + Possible Race
- **Static Analysis:** Multiple access warning
- **Dr. Memory:** Uninitialized read
- **Fix:** Initialize properly, add InterlockedIncrement

### g_cache - Investigate Thread Safety
- **Static Analysis:** Warning about multiple contexts
- **Action:** Review code manually, may need CRITICAL_SECTION

## Migration Plan

1. **Fix g_stats issues** (Day 1)
2. **Review g_cache thread safety** (Day 1)  
3. **Migrate g_config** (Day 2)
4. **Migrate g_database with g_cs_db** (Day 2-3)
5. **Migrate g_cache** (Day 3)

## Limitations

- No TSan available on Windows
- Thread race detection is incomplete
- Consider WSL testing for critical components
```

---

# Chapter 12: Troubleshooting

## MSYS2 Issues

### "pacman: command not found"

You're not in an MSYS2 terminal. Open "MSYS2 UCRT64" from Start menu.

### Package not found

```bash
# Update database
pacman -Sy

# Search for package
pacman -Ss doxygen
```

### Wrong environment

Check you're in UCRT64:
```bash
echo $MINGW_PREFIX
# Should show: /ucrt64
```

If not, close and open "MSYS2 UCRT64" (not MSYS2 MSYS).

## Path Issues

### "File not found" for Windows paths

Convert paths in MSYS2:
```bash
# Windows: C:\Users\Dev\project
# MSYS2: /c/Users/Dev/project

cd /c/Users/Dev/project
```

### Tools not found after installation

Add to PATH:
```bash
export PATH="/ucrt64/bin:$PATH"
# Add to ~/.bashrc to make permanent
```

## Tool-Specific Issues

### Doxygen graphs missing

```bash
# Check Graphviz
which dot
dot -V

# If not found:
pacman -S mingw-w64-ucrt-x86_64-graphviz
```

### cscope database errors

Rebuild from scratch:
```bash
rm migration_analysis/cscope/cscope.*
find src -name "*.c" -o -name "*.h" > migration_analysis/cscope/cscope.files
cscope -b -q -k -i migration_analysis/cscope/cscope.files -f migration_analysis/cscope/cscope.out
```

### Dr. Memory "application not found"

Use full path:
```powershell
drmemory -- C:\full\path\to\myapp.exe
```

---

# FAQ

**Q: Can I use TSan on Windows?**

A: No. TSan does not support Windows. Use Dr. Memory, Application Verifier, or WSL for TSan.

**Q: Is MSYS2 required?**

A: No, but it's the easiest way to get Unix tools. Alternatives: Git Bash, WSL, or Chocolatey for native Windows tools.

**Q: Why use UCRT64 vs MINGW64?**

A: UCRT64 uses Microsoft's Universal C Runtime, better for modern Windows. MINGW64 uses older msvcrt.dll.

**Q: Can I run these tools from PowerShell?**

A: Yes, if you add MSYS2 to your PATH. Doxygen and Cppcheck also have native Windows builds.

**Q: How do I get proper thread safety analysis?**

A: Best option is WSL with TSan. On native Windows, combine static analysis (Cppcheck, clang-tidy) with runtime tools (Dr. Memory, Application Verifier).

---

# Glossary

**Application Verifier:** Microsoft tool for runtime verification of Windows applications.

**CRITICAL_SECTION:** Windows synchronization primitive, similar to mutex.

**Dr. Memory:** Open source memory analysis tool for Windows.

**MSYS2:** Unix-like environment for Windows with package manager.

**UCRT64:** MSYS2 environment using Microsoft's Universal C Runtime.

---

# Quick Reference

## MSYS2 Installation

```bash
# Install tools
pacman -S mingw-w64-ucrt-x86_64-doxygen \
          mingw-w64-ucrt-x86_64-graphviz \
          mingw-w64-ucrt-x86_64-cppcheck \
          mingw-w64-ucrt-x86_64-clang-tools-extra \
          cscope
```

## Quick Commands

```bash
# Doxygen
doxygen -g Doxyfile && doxygen Doxyfile

# cscope
find src -name "*.[ch]" > cscope.files && cscope -b -q -k

# Cppcheck
cppcheck --enable=all src/

# clang-tidy
clang-tidy src/*.c -- -I include
```

## Windows-Specific Commands

```powershell
# Dr. Memory
drmemory -light -- .\myapp.exe

# Build with debug info
gcc -g -o myapp.exe src\*.c
```

## Files After Analysis

- `migration_analysis/doxygen/output/html/` — Call graphs
- `migration_analysis/cscope/cscope.out` — Cross-reference DB
- `migration_analysis/reports/cppcheck.txt` — Static analysis
- `migration_analysis/reports/clang_tidy.txt` — Lint results
- `migration_analysis/reports/decisions.md` — Final decisions

---

*FAT-P Library — User Manual UM-MIGRATION-ANALYSIS-005*  
*Open Source Tools Edition (Windows/MinGW)*  
*Last updated: January 2025*
