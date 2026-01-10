---
doc_id: UM-MIGRATION-ANALYSIS-002
doc_type: "User Manual"
title: "C Codebase Analysis with Open Source Tools (Windows)"
fatp_components: ["ServiceLocator"]
topics: ["global state", "cscope", "doxygen", "clang-query", "static analysis", "migration analysis", "Windows", "PowerShell"]
constraints: ["no commercial tools", "Windows environment", "compilable codebase"]
cxx_standard: "C++17"
last_verified: "2025-01-09"
audience: ["C++ developers", "library maintainers", "AI assistants"]
status: "reviewed"
---

# User Manual - C Codebase Analysis with Open Source Tools (Windows)

### *Step-by-step guide for analyzing C codebases on Windows using PowerShell*

*FAT-P Library — January 2025*

---

## User Manual Card

**Component:** Migration Analysis Toolchain (Windows)  
**Primary use case:** Pre-migration analysis of C codebase to identify global state, dependencies, and thread safety  
**Integration pattern:** Run tools in sequence, document findings, make migration decisions  
**Key tools:** Doxygen, cscope, ctags, clang-query, Clang Static Analyzer, Dr. Memory  
**Platform notes:** Some Linux tools have limited Windows support; alternatives provided  
**Common pitfalls:** Path escaping, missing Visual Studio components, WSL fallback needed for some tools  

---

## Table of Contents

1. [Overview](#overview)
2. [Prerequisites](#prerequisites)
3. [Windows-Specific Considerations](#windows-specific-considerations)
4. [Phase 1: Environment Setup](#phase-1-environment-setup)
5. [Phase 2: Initial Orientation with Doxygen](#phase-2-initial-orientation-with-doxygen)
6. [Phase 3: Global Inventory with clang-query and ctags](#phase-3-global-inventory-with-clang-query-and-ctags)
7. [Phase 4: Cross-Reference Analysis with cscope](#phase-4-cross-reference-analysis-with-cscope)
8. [Phase 5: Thread Safety Analysis](#phase-5-thread-safety-analysis)
9. [Phase 6: Synthesize Findings](#phase-6-synthesize-findings)
10. [Phase 7: Ongoing Validation](#phase-7-ongoing-validation)
11. [Complete Example: Analyzing a Sample Project](#complete-example-analyzing-a-sample-project)
12. [Quick Reference](#quick-reference)
13. [Appendix: WSL Fallback for Linux-Only Tools](#appendix-wsl-fallback-for-linux-only-tools)

---

## Overview

This guide walks through analyzing a C codebase on Windows using PowerShell and free, open-source tools. By the end, you will have:

1. Visual call graphs showing code structure
2. Complete inventory of global variables
3. Usage map for each global (who reads, who writes)
4. Thread safety assessment
5. Documentation supporting migration decisions

**Alternative approach:** If you prefer Unix-style commands (grep, awk, sed), see [User Manual - C Codebase Analysis Minimal Tooling](User_Manual_-_C_Codebase_Analysis_Minimal_Tooling.md) which covers MinGW/MSYS2 setup and allows using the same bash scripts as Linux.

**Time estimate:** 3-5 days for a medium codebase (50K-200K lines)

**Tools used:**

| Tool | Purpose | Windows Support | Install Method |
|------|---------|-----------------|----------------|
| Doxygen + Graphviz | Call graphs, documentation | Native | Chocolatey/Installer |
| Universal Ctags | Symbol indexing | Native | Chocolatey/Scoop |
| cscope | Cross-referencing | Limited | Manual/WSL |
| LLVM/Clang | AST queries, static analysis | Native | Installer |
| Dr. Memory | Memory/thread errors | Native | Installer |
| Clang Static Analyzer | Bug finding | Native | With LLVM |

**Windows limitations:**

| Tool | Linux | Windows | Workaround |
|------|-------|---------|------------|
| ThreadSanitizer | Full support | Not supported | Use Dr. Memory or WSL |
| Infer | Full support | Experimental | Use WSL |
| cscope | Native | Requires build | Use WSL or pre-built binary |

---

## Prerequisites

**Required:**
- Windows 10/11 (64-bit)
- PowerShell 5.1+ (built-in) or PowerShell 7+
- Administrator access for tool installation
- Target codebase source code
- Ability to compile the codebase (Visual Studio or MinGW)
- ~2 GB disk space for tools and generated output

**Recommended:**
- Windows Terminal (better experience)
- Visual Studio 2019/2022 with C++ workload
- WSL2 (for Linux-only tools)

**Assumed knowledge:**
- Basic PowerShell proficiency
- Understanding of C compilation
- Familiarity with MSBuild or CMake

---

## Windows-Specific Considerations

### Path Handling

PowerShell handles both forward and back slashes, but some tools are picky:

```powershell
# Use forward slashes for cross-platform compatibility
$ProjectRoot = "C:/Projects/myproject"

# Or use Join-Path for safety
$ProjectRoot = Join-Path $env:USERPROFILE "Projects\myproject"

# Convert to Unix-style paths when needed
$UnixPath = $ProjectRoot -replace '\\', '/'
```

### Execution Policy

You may need to allow script execution:

```powershell
# Check current policy
Get-ExecutionPolicy

# Allow local scripts (run as Administrator)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

### Environment Variables

```powershell
# Temporary (current session)
$env:PATH += ";C:\Program Files\LLVM\bin"

# Permanent (requires restart)
[Environment]::SetEnvironmentVariable("PATH", $env:PATH + ";C:\Tools", "User")
```

### Line Endings

Windows uses CRLF; some tools expect LF:

```powershell
# Convert file to Unix line endings
(Get-Content -Raw $file) -replace "`r`n", "`n" | Set-Content -NoNewline $file
```

---

## Phase 1: Environment Setup

### Step 1.1: Install Package Manager

Choose one (Chocolatey is most comprehensive):

**Option A: Chocolatey (Recommended)**
```powershell
# Run as Administrator
Set-ExecutionPolicy Bypass -Scope Process -Force
[System.Net.ServicePointManager]::SecurityProtocol = [System.Net.ServicePointManager]::SecurityProtocol -bor 3072
iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))

# Restart PowerShell after installation
```

**Option B: Scoop (User-level, no admin)**
```powershell
# Run as regular user
Set-ExecutionPolicy RemoteSigned -Scope CurrentUser
irm get.scoop.sh | iex
```

**Option C: winget (Built into Windows 11)**
```powershell
# Already installed on Windows 11
winget --version
```

### Step 1.2: Install Tools

**Using Chocolatey:**
```powershell
# Run as Administrator
choco install -y doxygen.install graphviz llvm universal-ctags

# Verify installations
refreshenv
doxygen --version
dot -V
clang --version
ctags --version
```

**Using Scoop:**
```powershell
# Add extras bucket for more packages
scoop bucket add extras
scoop bucket add main

scoop install doxygen graphviz llvm universal-ctags
```

**Using winget:**
```powershell
winget install --id=DimitriVanHeesch.Doxygen -e
winget install --id=Graphviz.Graphviz -e
winget install --id=LLVM.LLVM -e
# ctags not available via winget - use chocolatey or manual install
```

### Step 1.3: Install cscope (Manual)

cscope doesn't have a native Windows installer. Options:

**Option A: Pre-built binary**
```powershell
# Download from https://github.com/nickvence/cscope-win32/releases
# Or use this automation:

$cscopeUrl = "https://github.com/nickvence/cscope-win32/releases/download/v15.9/cscope-15.9-win64.zip"
$cscopeDir = "C:\Tools\cscope"

New-Item -ItemType Directory -Force -Path $cscopeDir
Invoke-WebRequest -Uri $cscopeUrl -OutFile "$cscopeDir\cscope.zip"
Expand-Archive -Path "$cscopeDir\cscope.zip" -DestinationPath $cscopeDir -Force
Remove-Item "$cscopeDir\cscope.zip"

# Add to PATH
$env:PATH += ";$cscopeDir"
[Environment]::SetEnvironmentVariable("PATH", $env:PATH, "User")
```

**Option B: Use WSL (see Appendix)**

### Step 1.4: Verify Installation

```powershell
# Create verification script
function Test-ToolInstallation {
    $tools = @{
        "doxygen" = "doxygen --version"
        "graphviz" = "dot -V"
        "clang" = "clang --version"
        "ctags" = "ctags --version"
        "cscope" = "cscope --version"
    }
    
    foreach ($tool in $tools.GetEnumerator()) {
        Write-Host "Checking $($tool.Key)... " -NoNewline
        try {
            $null = Invoke-Expression $tool.Value 2>&1
            Write-Host "OK" -ForegroundColor Green
        }
        catch {
            Write-Host "MISSING" -ForegroundColor Red
        }
    }
}

Test-ToolInstallation
```

**Expected output:**
```
Checking doxygen... OK
Checking graphviz... OK
Checking clang... OK
Checking ctags... OK
Checking cscope... OK
```

### Step 1.5: Create Analysis Directory Structure

```powershell
# Set your project root
$ProjectRoot = "C:\Projects\myproject"  # Adjust to your project
$AnalysisDir = Join-Path $ProjectRoot "migration_analysis"

# Create structure
$folders = @("doxygen", "cscope", "clang", "reports", "scripts")
foreach ($folder in $folders) {
    New-Item -ItemType Directory -Force -Path (Join-Path $AnalysisDir $folder)
}

# Create analysis log
$logFile = Join-Path $AnalysisDir "analysis_log.md"
"# Migration Analysis Log`n`nStarted: $(Get-Date -Format 'yyyy-MM-dd HH:mm')`n" | Out-File $logFile

Write-Host "Analysis directory created at: $AnalysisDir"
```

### Step 1.6: Generate Compilation Database

clang-query requires `compile_commands.json`.

**For CMake projects:**
```powershell
cd $ProjectRoot
New-Item -ItemType Directory -Force -Path "build"
cd build

cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# Copy to project root
Copy-Item "compile_commands.json" -Destination $ProjectRoot
```

**For Visual Studio projects:**
```powershell
# Use clang-cl intercept or compiledb
# Install compiledb
pip install compiledb

# Generate from make/nmake
compiledb -n make  # For Makefile projects

# For MSBuild, use Clang Power Tools VS extension
# Or manually create compile_commands.json
```

**Manual compile_commands.json template:**
```powershell
$compileCommands = @"
[
  {
    "directory": "$($ProjectRoot -replace '\\', '/')",
    "command": "clang -I./include -c src/main.c -o main.o",
    "file": "src/main.c"
  },
  {
    "directory": "$($ProjectRoot -replace '\\', '/')",
    "command": "clang -I./include -c src/config.c -o config.o",
    "file": "src/config.c"
  }
]
"@

$compileCommands | Out-File -FilePath (Join-Path $ProjectRoot "compile_commands.json") -Encoding UTF8
```

**Common Issues:**

| Problem | Solution |
|---------|----------|
| "clang not found" | Add LLVM bin to PATH: `$env:PATH += ";C:\Program Files\LLVM\bin"` |
| CMake not generating JSON | Ensure cmake version 3.5+; check generator supports it |
| Path issues in JSON | Use forward slashes, escape backslashes |

---

## Phase 2: Initial Orientation with Doxygen

### Step 2.1: Generate Doxygen Configuration

```powershell
cd $ProjectRoot

# Generate default config
$doxygenDir = Join-Path $AnalysisDir "doxygen"
doxygen -g (Join-Path $doxygenDir "Doxyfile")
```

### Step 2.2: Configure for Analysis

```powershell
# Create optimized Doxyfile for analysis
$doxyfileContent = @"
# Project settings
PROJECT_NAME           = "Migration Analysis"
OUTPUT_DIRECTORY       = $($AnalysisDir -replace '\\', '/')/doxygen/output
INPUT                  = src include
RECURSIVE              = YES
FILE_PATTERNS          = *.c *.h

# Extract everything (not just documented code)
EXTRACT_ALL            = YES
EXTRACT_STATIC         = YES
EXTRACT_PRIVATE        = YES
EXTRACT_LOCAL_CLASSES  = YES

# Source browsing
SOURCE_BROWSER         = YES
INLINE_SOURCES         = NO
REFERENCED_BY_RELATION = YES
REFERENCES_RELATION    = YES

# Graphs (requires Graphviz)
HAVE_DOT               = YES
CALL_GRAPH             = YES
CALLER_GRAPH           = YES
GRAPHICAL_HIERARCHY    = YES
DIRECTORY_GRAPH        = YES
INCLUDE_GRAPH          = YES
INCLUDED_BY_GRAPH      = YES

# Graph settings
DOT_IMAGE_FORMAT       = svg
INTERACTIVE_SVG        = YES
DOT_GRAPH_MAX_NODES    = 100
MAX_DOT_GRAPH_DEPTH    = 5

# Windows-specific: path to dot
DOT_PATH               = "C:/Program Files/Graphviz/bin"

# Disable what we don't need
GENERATE_LATEX         = NO
"@

$doxyfileContent | Out-File -FilePath (Join-Path $doxygenDir "Doxyfile") -Encoding UTF8
```

### Step 2.3: Run Doxygen

```powershell
cd $ProjectRoot

# Run Doxygen
doxygen (Join-Path $AnalysisDir "doxygen\Doxyfile")

# Open results in default browser
$indexPath = Join-Path $AnalysisDir "doxygen\output\html\index.html"
Start-Process $indexPath
```

### Step 2.4: Analyze Doxygen Output

Navigate the generated HTML:

1. **Files** → Click any `.c` file → "Variables" section shows globals
2. **Functions** → Click function → "Call Graph" and "Caller Graph"
3. **Files** → "Include dependency graph" shows file relationships

**Document findings:**
```powershell
$findings = @"

## Phase 2: Doxygen Orientation

### Key Entry Points Identified
- main() in main.c
- library_init() in init.c

### High Fan-In Functions (many callers)
- get_config() - suspected global accessor
- log_message() - suspected global logger

### Suspected Global Accessors
- [ ] Investigate: get_config()
- [ ] Investigate: vfs_find()

"@

Add-Content -Path (Join-Path $AnalysisDir "analysis_log.md") -Value $findings
```

---

## Phase 3: Global Inventory with clang-query and ctags

### Step 3.1: Find All Globals with clang-query

```powershell
cd $ProjectRoot

# Create output directory
$clangDir = Join-Path $AnalysisDir "clang"

# Find all source files
$sourceFiles = Get-ChildItem -Path "src" -Filter "*.c" -Recurse

# Query each file for globals
$globalsOutput = Join-Path $clangDir "globals_raw.txt"
"" | Out-File $globalsOutput  # Clear file

foreach ($file in $sourceFiles) {
    "=== $($file.FullName) ===" | Add-Content $globalsOutput
    
    # Run clang-query
    $result = & clang-query -p $ProjectRoot `
        -c "match varDecl(hasGlobalStorage())" `
        $file.FullName 2>&1
    
    $result | Add-Content $globalsOutput
}

Write-Host "Globals output saved to: $globalsOutput"
```

### Step 3.2: Parse clang-query Output

```powershell
# Extract global declarations
$globalsRaw = Get-Content $globalsOutput -Raw
$globalsList = Join-Path $clangDir "globals_list.txt"

# Use regex to find declarations
$pattern = '(static\s+)?(\w+[\s\*]+)+\w+\s*(=|;)'
$matches = [regex]::Matches($globalsRaw, $pattern)

$matches | ForEach-Object { $_.Value } | 
    Sort-Object -Unique | 
    Out-File $globalsList

Write-Host "Unique globals saved to: $globalsList"
Get-Content $globalsList | Select-Object -First 20
```

### Step 3.3: Alternative/Supplement with ctags

```powershell
cd $ProjectRoot

$tagsFile = Join-Path $AnalysisDir "cscope\tags"

# Generate tags with full details
ctags --fields=+niazS --extras=+q `
    -R --languages=C `
    -f $tagsFile `
    src include

# Extract just variables (kind 'v')
$reportsDir = Join-Path $AnalysisDir "reports"
$tagsContent = Get-Content $tagsFile

# Parse tags file for variables
$variables = $tagsContent | Where-Object { $_ -match '\tv\t' }

# Separate static vs extern
$staticVars = $variables | Where-Object { $_ -match 'static:' }
$externVars = $variables | Where-Object { $_ -notmatch 'static:' }

$staticVars | Out-File (Join-Path $reportsDir "static_vars.txt")
$externVars | Out-File (Join-Path $reportsDir "extern_vars.txt")

Write-Host "Static variables: $($staticVars.Count)"
Write-Host "Extern variables: $($externVars.Count)"
```

### Step 3.4: Create Global Inventory Table

```powershell
$inventoryFile = Join-Path $AnalysisDir "reports\global_inventory.md"

# Create header
@"
# Global State Inventory

| Variable | File | Type | Linkage | Notes |
|----------|------|------|---------|-------|
"@ | Out-File $inventoryFile

# Parse tags and create table entries
$tagsContent = Get-Content (Join-Path $AnalysisDir "cscope\tags")
$variables = $tagsContent | Where-Object { $_ -match '\tv\t' }

foreach ($var in $variables) {
    # Parse ctags format: name<TAB>file<TAB>pattern<TAB>kind<TAB>extras
    $parts = $var -split '\t'
    if ($parts.Count -ge 4) {
        $name = $parts[0]
        $file = Split-Path $parts[1] -Leaf
        $linkage = if ($var -match 'static:') { "internal" } else { "external" }
        
        "| $name | $file | TODO | $linkage | |" | Add-Content $inventoryFile
    }
}

Write-Host "Inventory saved to: $inventoryFile"
Get-Content $inventoryFile | Select-Object -First 15
```

### Step 3.5: Identify Mutex-Protected Globals

```powershell
$mutexReport = Join-Path $AnalysisDir "reports\mutex_protected_globals.txt"
"" | Out-File $mutexReport

# Get list of globals
$globals = Get-Content (Join-Path $AnalysisDir "reports\static_vars.txt") |
    ForEach-Object { ($_ -split '\t')[0] } |
    Select-Object -First 30

foreach ($global in $globals) {
    if ([string]::IsNullOrWhiteSpace($global)) { continue }
    
    # Search for global near mutex calls
    $sourceFiles = Get-ChildItem -Path "src" -Filter "*.c" -Recurse
    
    foreach ($file in $sourceFiles) {
        $content = Get-Content $file.FullName -Raw
        
        # Check if global appears near mutex operations
        if ($content -match "mutex.*$global|$global.*mutex|lock.*$global|$global.*lock") {
            "${global}: MUTEX PROTECTED in $($file.Name)" | Add-Content $mutexReport
        }
    }
}

Write-Host "Mutex analysis saved to: $mutexReport"
```

---

## Phase 4: Cross-Reference Analysis with cscope

### Step 4.1: Build cscope Database

```powershell
cd $ProjectRoot

$cscopeDir = Join-Path $AnalysisDir "cscope"

# Create file list
$sourceFiles = Get-ChildItem -Path "src", "include" -Include "*.c", "*.h" -Recurse
$fileList = Join-Path $cscopeDir "cscope.files"
$sourceFiles.FullName | Out-File $fileList -Encoding ASCII

# Build database
# Note: cscope paths are relative, so we need to be in project root
$cscopeOut = Join-Path $cscopeDir "cscope.out"

# cscope on Windows may need Unix-style paths
$fileListUnix = $fileList -replace '\\', '/'

cscope -b -q -k -i $fileList -f $cscopeOut

Write-Host "cscope database built at: $cscopeOut"
```

### Step 4.2: Create Analysis Function

```powershell
# Create reusable analysis function
function Analyze-Global {
    param(
        [string]$Symbol,
        [string]$CscopeDb
    )
    
    Write-Host "`n=== Analysis of: $Symbol ===" -ForegroundColor Cyan
    
    # Find definition
    Write-Host "`n### Definition:" -ForegroundColor Yellow
    cscope -d -f $CscopeDb -L1 $Symbol 2>$null
    
    # Find all references
    Write-Host "`n### All References:" -ForegroundColor Yellow
    $refs = cscope -d -f $CscopeDb -L0 $Symbol 2>$null
    $refs
    
    # Count metrics
    $refCount = ($refs | Measure-Object).Count
    $fileCount = ($refs | ForEach-Object { ($_ -split ' ')[0] } | Sort-Object -Unique | Measure-Object).Count
    
    Write-Host "`n### Coupling Metrics:" -ForegroundColor Yellow
    Write-Host "Total references: $refCount"
    Write-Host "Files involved: $fileCount"
    
    if ($fileCount -le 3) {
        Write-Host "Assessment: LOW coupling - good migration candidate" -ForegroundColor Green
    }
    elseif ($fileCount -le 10) {
        Write-Host "Assessment: MEDIUM coupling - migration possible with care" -ForegroundColor Yellow
    }
    else {
        Write-Host "Assessment: HIGH coupling - migration will be complex" -ForegroundColor Red
    }
    
    return @{
        Symbol = $Symbol
        References = $refCount
        Files = $fileCount
    }
}

# Save function for reuse
$functionDef = @'
function Analyze-Global {
    param([string]$Symbol, [string]$CscopeDb)
    # ... (full function as above)
}
'@
$functionDef | Out-File (Join-Path $AnalysisDir "scripts\Analyze-Global.ps1")
```

### Step 4.3: Analyze Each Global

```powershell
$cscopeDb = Join-Path $AnalysisDir "cscope\cscope.out"
$summaryFile = Join-Path $AnalysisDir "reports\xref_summary.md"

# Header
@"
# Cross-Reference Summary

| Global | References | Files | Coupling |
|--------|------------|-------|----------|
"@ | Out-File $summaryFile

# Get globals from inventory
$inventoryFile = Join-Path $AnalysisDir "reports\global_inventory.md"
$globals = Get-Content $inventoryFile | 
    Where-Object { $_ -match '^\|' -and $_ -notmatch 'Variable' } |
    ForEach-Object { ($_ -split '\|')[1].Trim() }

$results = @()

foreach ($global in $globals) {
    if ([string]::IsNullOrWhiteSpace($global)) { continue }
    
    # Query cscope
    $refs = cscope -d -f $cscopeDb -L0 $global 2>$null
    $refCount = ($refs | Measure-Object).Count
    $fileCount = ($refs | ForEach-Object { ($_ -split ' ')[0] } | 
        Sort-Object -Unique | Measure-Object).Count
    
    $coupling = switch ($fileCount) {
        { $_ -le 3 } { "LOW" }
        { $_ -le 10 } { "MEDIUM" }
        default { "HIGH" }
    }
    
    "| $global | $refCount | $fileCount | $coupling |" | Add-Content $summaryFile
    
    $results += @{
        Global = $global
        References = $refCount
        Files = $fileCount
        Coupling = $coupling
    }
}

Write-Host "`nCross-reference summary saved to: $summaryFile"
Get-Content $summaryFile
```

### Step 4.4: Interactive Exploration

```powershell
# Launch cscope interactively
cd $ProjectRoot
cscope -d -f (Join-Path $AnalysisDir "cscope\cscope.out")

<#
Interactive commands:
Tab         - Switch between input and results
0-9         - Select query type
Enter       - Execute query
Ctrl+D/Esc  - Exit

Query types:
0 - Find this C symbol
1 - Find this global definition
2 - Find functions called by this function
3 - Find functions calling this function
4 - Find this text string
#>
```

---

## Phase 5: Thread Safety Analysis

### Windows Thread Analysis Options

ThreadSanitizer is not available on Windows. Alternatives:

| Tool | Type | Coverage | Setup Difficulty |
|------|------|----------|------------------|
| Dr. Memory | Runtime | Memory + basic threading | Medium |
| Application Verifier | Runtime | Windows-specific | Easy |
| Clang Static Analyzer | Static | Limited threading | Easy |
| Intel Inspector | Runtime/Static | Comprehensive | Medium (paid) |
| WSL + TSan | Runtime | Full TSan | Medium |

### Step 5.1: Static Analysis with Clang

```powershell
cd $ProjectRoot

# Create output directory
$staticAnalysisDir = Join-Path $AnalysisDir "static_analysis"
New-Item -ItemType Directory -Force -Path $staticAnalysisDir

# Run Clang Static Analyzer on each file
$sourceFiles = Get-ChildItem -Path "src" -Filter "*.c" -Recurse
$analysisResults = Join-Path $staticAnalysisDir "clang_analysis.txt"

"" | Out-File $analysisResults

foreach ($file in $sourceFiles) {
    "=== Analyzing: $($file.Name) ===" | Add-Content $analysisResults
    
    # Run analyzer
    $result = clang --analyze `
        -Xanalyzer -analyzer-output=text `
        -I"include" `
        $file.FullName 2>&1
    
    $result | Add-Content $analysisResults
}

Write-Host "Static analysis complete: $analysisResults"

# Extract warnings
$warnings = Get-Content $analysisResults | Where-Object { $_ -match 'warning:' }
Write-Host "Total warnings: $($warnings.Count)"
```

### Step 5.2: Install and Run Dr. Memory

Dr. Memory detects memory errors and some threading issues.

```powershell
# Download Dr. Memory
$drMemoryUrl = "https://github.com/DynamoRIO/drmemory/releases/download/release_2.5.0/DrMemory-Windows-2.5.0.zip"
$drMemoryDir = "C:\Tools\DrMemory"

New-Item -ItemType Directory -Force -Path $drMemoryDir
Invoke-WebRequest -Uri $drMemoryUrl -OutFile "$drMemoryDir\drmemory.zip"
Expand-Archive -Path "$drMemoryDir\drmemory.zip" -DestinationPath $drMemoryDir -Force

# Add to PATH
$env:PATH += ";$drMemoryDir\DrMemory-Windows-2.5.0\bin64"
```

### Step 5.3: Run Tests Under Dr. Memory

```powershell
# Build debug version of your project
cd $ProjectRoot
# Adjust build command for your project
cmake --build build --config Debug

# Run tests under Dr. Memory
$testExe = Join-Path $ProjectRoot "build\Debug\test_runner.exe"
$drMemoryOutput = Join-Path $AnalysisDir "reports\drmemory_output.txt"

drmemory -logdir (Join-Path $AnalysisDir "drmemory_logs") `
    -- $testExe 2>&1 | Tee-Object -FilePath $drMemoryOutput

Write-Host "Dr. Memory output: $drMemoryOutput"
```

### Step 5.4: Parse Results

```powershell
$drMemoryLog = Get-ChildItem -Path (Join-Path $AnalysisDir "drmemory_logs") `
    -Filter "results.txt" -Recurse | Select-Object -First 1

if ($drMemoryLog) {
    $results = Get-Content $drMemoryLog.FullName
    
    # Count issue types
    $errors = ($results | Where-Object { $_ -match '^Error' }).Count
    $warnings = ($results | Where-Object { $_ -match '^UNADDRESSABLE|^UNINITIALIZED|^LEAK' }).Count
    
    Write-Host "Dr. Memory Results:"
    Write-Host "  Errors: $errors"
    Write-Host "  Memory issues: $warnings"
    
    # Save summary
    @"
## Dr. Memory Analysis

- Errors: $errors
- Memory issues: $warnings

See full log: $($drMemoryLog.FullName)
"@ | Add-Content (Join-Path $AnalysisDir "analysis_log.md")
}
```

### Step 5.5: Application Verifier (Windows Built-in)

```powershell
# Application Verifier is a Windows SDK tool
# Open via: appverif.exe (if Windows SDK installed)

# Or use command line
$testExe = "C:\path\to\test.exe"

# Enable basic checks
appverif /enable Heaps Locks Handles /for $testExe

# Run your tests normally
& $testExe

# View results in Event Viewer > Application Verifier

# Disable when done
appverif /disable * /for $testExe
```

### Step 5.6: Manual Thread Safety Audit

Since automated tools are limited on Windows, supplement with manual analysis:

```powershell
# Find threading patterns in code
$sourceFiles = Get-ChildItem -Path "src" -Filter "*.c" -Recurse
$threadReport = Join-Path $AnalysisDir "reports\threading_patterns.txt"

"# Threading Patterns Analysis`n" | Out-File $threadReport

foreach ($file in $sourceFiles) {
    $content = Get-Content $file.FullName -Raw
    
    # Check for threading primitives
    $patterns = @{
        "CreateThread" = "Thread creation"
        "WaitForSingleObject" = "Thread synchronization"
        "InitializeCriticalSection" = "Critical section init"
        "EnterCriticalSection" = "Critical section enter"
        "pthread_create" = "POSIX thread creation"
        "pthread_mutex" = "POSIX mutex"
        "volatile" = "Volatile variable"
        "InterlockedIncrement" = "Atomic operation"
    }
    
    $found = @()
    foreach ($pattern in $patterns.GetEnumerator()) {
        if ($content -match $pattern.Key) {
            $found += $pattern.Value
        }
    }
    
    if ($found.Count -gt 0) {
        "`n## $($file.Name)" | Add-Content $threadReport
        $found | ForEach-Object { "- $_" } | Add-Content $threadReport
    }
}

Write-Host "Threading patterns saved to: $threadReport"
Get-Content $threadReport
```

### Step 5.7: Correlate with Global Inventory

```powershell
$threadSafetyFile = Join-Path $AnalysisDir "reports\thread_safety.md"

@"
# Thread Safety Assessment

| Global | Static Analysis | Manual Review | Assessment |
|--------|-----------------|---------------|------------|
"@ | Out-File $threadSafetyFile

# Get globals
$inventoryFile = Join-Path $AnalysisDir "reports\global_inventory.md"
$globals = Get-Content $inventoryFile | 
    Where-Object { $_ -match '^\|' -and $_ -notmatch 'Variable' } |
    ForEach-Object { ($_ -split '\|')[1].Trim() }

foreach ($global in $globals) {
    if ([string]::IsNullOrWhiteSpace($global)) { continue }
    
    # Check if global appears in threading patterns
    $threadingPatterns = Get-Content $threadReport -Raw
    $inThreading = if ($threadingPatterns -match $global) { "Yes" } else { "No" }
    
    # Check static analysis for issues
    $staticAnalysis = Get-Content $analysisResults -Raw
    $staticIssues = if ($staticAnalysis -match $global) { "Check" } else { "None" }
    
    $assessment = if ($inThreading -eq "Yes") { "Needs review" } else { "Likely safe" }
    
    "| $global | $staticIssues | $inThreading | $assessment |" | Add-Content $threadSafetyFile
}

Write-Host "Thread safety assessment saved to: $threadSafetyFile"
Get-Content $threadSafetyFile
```

---

## Phase 6: Synthesize Findings

### Step 6.1: Create Migration Decision Matrix

```powershell
$decisionFile = Join-Path $AnalysisDir "reports\migration_decisions.md"

@"
# Migration Decision Matrix

## Decision Criteria

| Factor | Weight | Description |
|--------|--------|-------------|
| Coupling | High | More files = harder migration |
| Thread Safety | High | Issues = must understand before migrating |
| Testability Benefit | Medium | Would migration enable better testing? |
| Performance Impact | Medium | Hot path = don't add indirection |
| API Stability | Low | Public API = need facade |

## Classification Guide

**MIGRATE** - Move to ServiceLocator
- Low coupling (≤3 files)
- Clear synchronization (or single-threaded)
- Testability benefit

**DO NOT MIGRATE** - Keep global
- OS-level coordination
- Bootstrap dependency
- Hot path

**PARTIAL** - Wrap but don't fully migrate
- High coupling but testability benefit

## Decisions

| Global | Coupling | Thread-Safe? | Decision | Rationale |
|--------|----------|--------------|----------|-----------|
"@ | Out-File $decisionFile

# Populate from previous analysis
$xrefSummary = Get-Content (Join-Path $AnalysisDir "reports\xref_summary.md")
$threadSafety = Get-Content (Join-Path $AnalysisDir "reports\thread_safety.md")

# Manual step: review and fill in decisions
Write-Host "Decision matrix template created at: $decisionFile"
Write-Host "Review and complete the 'Decision' and 'Rationale' columns manually."
```

### Step 6.2: Generate Final Report

```powershell
$finalReport = Join-Path $AnalysisDir "reports\FINAL_ANALYSIS.md"

@"
# Migration Analysis Final Report

**Project:** $((Get-Item $ProjectRoot).Name)
**Date:** $(Get-Date -Format 'yyyy-MM-dd')
**Platform:** Windows (PowerShell analysis)

## Executive Summary

[Fill in after reviewing all data]

- Total globals analyzed: XX
- Recommended for migration: XX  
- Keep as global: XX
- Needs further investigation: XX

## Analysis Artifacts

| Artifact | Location |
|----------|----------|
| Doxygen documentation | doxygen/output/html/index.html |
| Global inventory | reports/global_inventory.md |
| Cross-reference summary | reports/xref_summary.md |
| Thread safety assessment | reports/thread_safety.md |
| Migration decisions | reports/migration_decisions.md |

## Global State Inventory

$(Get-Content (Join-Path $AnalysisDir "reports\global_inventory.md") -Raw)

## Dependency Analysis

$(Get-Content (Join-Path $AnalysisDir "reports\xref_summary.md") -Raw)

## Thread Safety Assessment

$(Get-Content (Join-Path $AnalysisDir "reports\thread_safety.md") -Raw)

## Windows-Specific Notes

- ThreadSanitizer not available; used Dr. Memory and static analysis
- Consider WSL for comprehensive thread safety testing
- Application Verifier available for Windows-specific checks

## Next Steps

1. [ ] Review findings with team
2. [ ] Set up WSL for TSan testing (recommended)
3. [ ] Establish benchmark baseline
4. [ ] Begin migration of low-risk globals

"@ | Out-File $finalReport

Write-Host "Final report created at: $finalReport"
Start-Process $finalReport
```

---

## Phase 7: Ongoing Validation

### Step 7.1: PowerShell Analysis Script

Create reusable script for future runs:

```powershell
# Save as: Invoke-MigrationAnalysis.ps1

param(
    [Parameter(Mandatory=$true)]
    [string]$ProjectRoot,
    
    [switch]$SkipDoxygen,
    [switch]$SkipCscope,
    [switch]$Verbose
)

$ErrorActionPreference = "Stop"

# Setup
$AnalysisDir = Join-Path $ProjectRoot "migration_analysis"
New-Item -ItemType Directory -Force -Path $AnalysisDir | Out-Null

Write-Host "=== Migration Analysis ===" -ForegroundColor Cyan
Write-Host "Project: $ProjectRoot"
Write-Host "Output: $AnalysisDir"

# Phase 2: Doxygen
if (-not $SkipDoxygen) {
    Write-Host "`nPhase 2: Running Doxygen..." -ForegroundColor Yellow
    # ... doxygen commands
}

# Phase 3: Global inventory
Write-Host "`nPhase 3: Building global inventory..." -ForegroundColor Yellow
# ... ctags commands

# Phase 4: Cross-reference
if (-not $SkipCscope) {
    Write-Host "`nPhase 4: Building cross-references..." -ForegroundColor Yellow
    # ... cscope commands
}

# Phase 5: Static analysis
Write-Host "`nPhase 5: Running static analysis..." -ForegroundColor Yellow
# ... clang analysis

Write-Host "`n=== Analysis Complete ===" -ForegroundColor Green
Write-Host "Results in: $AnalysisDir"
```

### Step 7.2: GitHub Actions CI (Cross-Platform)

```yaml
# .github/workflows/thread-safety.yml
name: Thread Safety Analysis

on: [push, pull_request]

jobs:
  windows-static-analysis:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install LLVM
        run: choco install llvm -y
        
      - name: Run Clang Static Analyzer
        shell: pwsh
        run: |
          $files = Get-ChildItem -Path src -Filter "*.c" -Recurse
          foreach ($file in $files) {
            clang --analyze -I"include" $file.FullName
          }
          
  linux-tsan:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Build with TSan
        run: |
          export CC=clang
          export CFLAGS="-fsanitize=thread -g -O1"
          make
          
      - name: Run tests
        run: ./run_tests 2>&1 | tee tsan_output.txt
        
      - name: Check for races
        run: |
          if grep -q "WARNING: ThreadSanitizer" tsan_output.txt; then
            exit 1
          fi
```

### Step 7.3: Benchmark Script

```powershell
# benchmark_baseline.ps1

param(
    [Parameter(Mandatory=$true)]
    [string]$TestExecutable,
    
    [int]$Iterations = 5
)

$results = @()

Write-Host "=== Performance Baseline ===" -ForegroundColor Cyan
Write-Host "Executable: $TestExecutable"
Write-Host "Iterations: $Iterations"
Write-Host ""

for ($i = 1; $i -le $Iterations; $i++) {
    Write-Host "Run $i... " -NoNewline
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    & $TestExecutable | Out-Null
    $sw.Stop()
    
    $results += $sw.ElapsedMilliseconds
    Write-Host "$($sw.ElapsedMilliseconds) ms"
}

$avg = ($results | Measure-Object -Average).Average
$min = ($results | Measure-Object -Minimum).Minimum
$max = ($results | Measure-Object -Maximum).Maximum

Write-Host ""
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  Average: $([math]::Round($avg, 2)) ms"
Write-Host "  Min: $min ms"
Write-Host "  Max: $max ms"

# Save results
$output = @{
    Date = Get-Date -Format "yyyy-MM-dd HH:mm"
    Executable = $TestExecutable
    Iterations = $Iterations
    Results = $results
    Average = $avg
    Min = $min
    Max = $max
}

$output | ConvertTo-Json | Out-File "benchmark_results.json"
```

---

## Complete Example: Analyzing a Sample Project

```powershell
# Full analysis workflow

# 1. Setup
$ProjectRoot = "C:\Projects\myproject"
$AnalysisDir = "$ProjectRoot\migration_analysis"

# 2. Create directories
@("doxygen", "cscope", "clang", "reports", "scripts") | ForEach-Object {
    New-Item -ItemType Directory -Force -Path "$AnalysisDir\$_"
}

# 3. Generate compile_commands.json
cd $ProjectRoot
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
Copy-Item "build\compile_commands.json" -Destination $ProjectRoot

# 4. Run Doxygen
doxygen -g "$AnalysisDir\doxygen\Doxyfile"
# Edit Doxyfile...
doxygen "$AnalysisDir\doxygen\Doxyfile"

# 5. Generate tags
ctags --fields=+niazS -R -f "$AnalysisDir\cscope\tags" src include

# 6. Build cscope database  
Get-ChildItem -Path src, include -Include "*.c", "*.h" -Recurse | 
    Select-Object -ExpandProperty FullName |
    Out-File "$AnalysisDir\cscope\cscope.files" -Encoding ASCII
    
cscope -b -q -k -i "$AnalysisDir\cscope\cscope.files" `
    -f "$AnalysisDir\cscope\cscope.out"

# 7. Run static analysis
Get-ChildItem -Path src -Filter "*.c" -Recurse | ForEach-Object {
    clang --analyze -I"include" $_.FullName 2>&1
} | Out-File "$AnalysisDir\reports\static_analysis.txt"

# 8. Open results
Start-Process "$AnalysisDir\doxygen\output\html\index.html"
```

---

## Quick Reference

### PowerShell Commands Cheat Sheet

```powershell
# Tool installation (Chocolatey)
choco install doxygen.install graphviz llvm universal-ctags

# Doxygen
doxygen -g Doxyfile              # Generate config
doxygen Doxyfile                 # Generate docs

# cscope
cscope -b -q -k -i files.txt     # Build database
cscope -d -f cscope.out -L0 sym  # Find references
cscope -d -f cscope.out -L1 sym  # Find definition
cscope -d -f cscope.out -L3 func # Find callers

# ctags
ctags -R --fields=+niazS .       # Generate tags

# clang-query
clang-query -p . -c "match varDecl(hasGlobalStorage())" file.c

# Static analysis
clang --analyze -Xanalyzer -analyzer-output=text file.c
```

### Key PowerShell Patterns

```powershell
# Find files
Get-ChildItem -Path src -Filter "*.c" -Recurse

# Search in files
Select-String -Path "src\*.c" -Pattern "mutex"

# Process each file
Get-ChildItem -Filter "*.c" | ForEach-Object { 
    # Process $_.FullName 
}

# Create report table
@"
| Col1 | Col2 |
|------|------|
"@ | Out-File report.md

# Add to PATH temporarily
$env:PATH += ";C:\Tools\bin"

# Run external tool and capture output
$result = & tool.exe args 2>&1
```

### Decision Flowchart

```
Is it a mutex/allocator/bootstrap dependency?
  YES → DO NOT MIGRATE
  NO ↓

Is it used for OS-level coordination?
  YES → DO NOT MIGRATE  
  NO ↓

Is it on a hot path (>1M calls/sec)?
  YES → DO NOT MIGRATE
  NO ↓

Does static analysis show issues?
  YES → INVESTIGATE FIRST
  NO ↓

Is coupling LOW (≤3 files)?
  YES → MIGRATE (good candidate)
  NO ↓

Is coupling MEDIUM (≤10 files)?
  YES → MIGRATE with facade
  NO ↓

HIGH coupling?
  → PARTIAL migration or defer
```

---

## Appendix: WSL Fallback for Linux-Only Tools

For tools without good Windows support (TSan, Infer), use WSL:

### Setup WSL

```powershell
# Install WSL (run as Administrator)
wsl --install -d Ubuntu

# Restart, then in Ubuntu terminal:
sudo apt update
sudo apt install -y clang gcc make cmake
```

### Mount Windows Project in WSL

```bash
# In WSL, Windows drives are at /mnt/
cd /mnt/c/Projects/myproject

# Run TSan build
export CC=clang
export CFLAGS="-fsanitize=thread -g -O1"
make clean && make

# Run tests
./run_tests 2>&1 | tee tsan_output.txt
```

### PowerShell Integration

```powershell
# Run Linux commands from PowerShell
wsl -e bash -c "cd /mnt/c/Projects/myproject && make CC=clang CFLAGS='-fsanitize=thread -g'"

# Run TSan tests
wsl -e bash -c "cd /mnt/c/Projects/myproject && ./run_tests" 2>&1 | 
    Tee-Object -FilePath "$AnalysisDir\tsan_output.txt"

# Check for races
$tsanOutput = Get-Content "$AnalysisDir\tsan_output.txt" -Raw
if ($tsanOutput -match "WARNING: ThreadSanitizer") {
    Write-Host "Thread safety violations detected!" -ForegroundColor Red
}
```

### Install Infer in WSL

```bash
# In WSL Ubuntu
VERSION="v1.1.0"
curl -sSL "https://github.com/facebook/infer/releases/download/${VERSION}/infer-linux64-${VERSION}.tar.xz" \
    | sudo tar -C /opt -xJ
sudo ln -s /opt/infer-linux64-${VERSION}/bin/infer /usr/local/bin/infer

# Run Infer
cd /mnt/c/Projects/myproject
infer run -- make
```

---

*FAT-P Library — User Manual UM-MIGRATION-ANALYSIS-002*  
*Last updated: January 2025*
