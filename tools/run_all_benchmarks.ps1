# FATP_META:
#   meta_version: 1
#   component: Tooling
#   file_role: tooling
#   path: tools/run_all_benchmarks.ps1
#   summary: "Windows PowerShell script to execute all Fat-P benchmark suites."
#   api_stability: in_work
#   layer: Infrastructure
#   related:
#     docs_search: ""
#     tests: []
#   hygiene:
#     pragma_once: false
#     include_guard: false
#     defines_total: 0
#     defines_unprefixed: 0
#     undefs_total: 0
#     includes_windows_h: false
# ==============================================================================
# run_all_benchmarks.ps1 - Execute all Fat-P benchmark suites (Windows)
# ==============================================================================
# Location: tools\ (run from project root)
#
# Usage:
#   .\tools\run_all_benchmarks.ps1 [OPTIONS]
#
# Options:
#   -Dir DIR            Benchmark directory (default: build\release)
#   -Output DIR         Output directory for results (default: benchmark_results)
#   -Filter PATTERN     Only run benchmarks matching pattern (e.g., "*Hash*")
#   -Quick              Quick mode: reduced iterations (warmup=1, measured=5)
#   -Verbose            Verbose output during execution
#   -ContinueOnError    Continue on error (don't stop on first failure)
#   -DryRun             Show what would be run without executing
#   -Help               Show this help message
#
# ==============================================================================

param(
    [string]$Dir = "build\release",
    [string]$Output = "benchmark_results",
    [string]$Filter = "",
    [switch]$Quick,
    [switch]$Verbose,
    [switch]$ContinueOnError,
    [switch]$DryRun,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# ------------------------------------------------------------------------------
# Verify we're in project root
# ------------------------------------------------------------------------------
if (-not (Test-Path "CMakeLists.txt")) {
    Write-Host "Error: Run from project root (where CMakeLists.txt is located)" -ForegroundColor Red
    exit 1
}

# ------------------------------------------------------------------------------
# Help
# ------------------------------------------------------------------------------
if ($Help) {
    Get-Help $MyInvocation.MyCommand.Path -Detailed
    exit 0
}

# ------------------------------------------------------------------------------
# Configuration
# ------------------------------------------------------------------------------
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$ResultsDir = Join-Path $Output $Timestamp

# Quick mode environment
if ($Quick) {
    $env:FATP_BENCH_WARMUP_RUNS = "1"
    $env:FATP_BENCH_BATCHES = "5"
    $env:FATP_BENCH_NO_STABILIZE = "1"
}

# Find benchmarks
$Pattern = if ($Filter) { "benchmark_*$Filter*.exe" } else { "benchmark_*.exe" }
$Benchmarks = Get-ChildItem -Path $Dir -Filter $Pattern -File -ErrorAction SilentlyContinue | Sort-Object Name

if ($Benchmarks.Count -eq 0) {
    Write-Host "Error: No benchmarks found in $Dir" -ForegroundColor Red
    Write-Host "Looking for: $Pattern"
    exit 1
}

# ------------------------------------------------------------------------------
# Display Plan
# ------------------------------------------------------------------------------
Write-Host "================================================================================" -ForegroundColor Blue
Write-Host "  Fat-P Benchmark Suite Runner" -ForegroundColor Blue
Write-Host "================================================================================" -ForegroundColor Blue
Write-Host ""
Write-Host "Directory:    " -NoNewline; Write-Host $Dir -ForegroundColor Yellow
Write-Host "Output:       " -NoNewline; Write-Host $ResultsDir -ForegroundColor Yellow
Write-Host "Benchmarks:   " -NoNewline; Write-Host $Benchmarks.Count -ForegroundColor Yellow
Write-Host "Quick mode:   " -NoNewline; Write-Host $Quick -ForegroundColor Yellow
Write-Host "Continue:     " -NoNewline; Write-Host $ContinueOnError -ForegroundColor Yellow
Write-Host ""
Write-Host "Benchmarks to run:"
foreach ($bench in $Benchmarks) {
    Write-Host "  - $($bench.Name)"
}
Write-Host ""

if ($DryRun) {
    Write-Host "[DRY RUN] Would execute the above benchmarks" -ForegroundColor Yellow
    exit 0
}

# Create output directory
New-Item -ItemType Directory -Force -Path $ResultsDir | Out-Null

# ------------------------------------------------------------------------------
# Run Benchmarks
# ------------------------------------------------------------------------------
Write-Host "Starting benchmark execution at $(Get-Date)" -ForegroundColor Blue
Write-Host ""

$Passed = 0
$Failed = 0
$FailedBenchmarks = @()
$Timings = [System.Collections.ArrayList]@()

function Run-Benchmark {
    param([System.IO.FileInfo]$Bench)
    
    $Name = $Bench.BaseName
    $ResultFile = Join-Path $ResultsDir "$Name.txt"
    $CsvFile = Join-Path $ResultsDir "$Name.csv"
    
    Write-Host ("=" * 80) -ForegroundColor Blue
    Write-Host "Running: $Name" -ForegroundColor Yellow
    Write-Host ("=" * 80) -ForegroundColor Blue
    
    # Set per-benchmark CSV output
    $env:FATP_BENCH_OUTPUT_CSV = $CsvFile
    
    $Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        if ($Verbose) {
            & $Bench.FullName 2>&1 | Tee-Object -FilePath $ResultFile
        } else {
            $output = & $Bench.FullName 2>&1
            $output | Out-File -FilePath $ResultFile -Encoding UTF8
            # Show header
            $output | Select-Object -First 30
            Write-Host "..."
            Write-Host "[Full output saved to $ResultFile]"
        }
        
        $Stopwatch.Stop()
        $Duration = [math]::Round($Stopwatch.Elapsed.TotalSeconds)
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "[OK] $Name completed in ${Duration}s" -ForegroundColor Green
            return [PSCustomObject]@{ Success = $true; Duration = $Duration; Name = $Name }
        } else {
            Write-Host "[FAIL] $Name failed (exit code: $LASTEXITCODE)" -ForegroundColor Red
            return [PSCustomObject]@{ Success = $false; Duration = $Duration; Name = $Name }
        }
    }
    catch {
        $Stopwatch.Stop()
        $Duration = [math]::Round($Stopwatch.Elapsed.TotalSeconds)
        Write-Host "[FAIL] $Name failed: $_" -ForegroundColor Red
        return [PSCustomObject]@{ Success = $false; Duration = $Duration; Name = $Name }
    }
}

foreach ($bench in $Benchmarks) {
    $result = Run-Benchmark -Bench $bench
    if ($null -ne $result) {
        $null = $Timings.Add($result)
    }
    
    if ($null -ne $result -and $result.Success) {
        $Passed++
    } else {
        $Failed++
        if ($null -ne $result) {
            $FailedBenchmarks += $result.Name
        }
        if (-not $ContinueOnError) {
            Write-Host "Stopping due to failure (use -ContinueOnError to continue)" -ForegroundColor Red
            break
        }
    }
    Write-Host ""
}

# ------------------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------------------
Write-Host "================================================================================" -ForegroundColor Blue
Write-Host "  Summary" -ForegroundColor Blue
Write-Host "================================================================================" -ForegroundColor Blue
Write-Host ""
Write-Host "Total:    $($Benchmarks.Count)"
Write-Host "Passed:   " -NoNewline; Write-Host $Passed -ForegroundColor Green
Write-Host "Failed:   " -NoNewline; Write-Host $Failed -ForegroundColor Red
Write-Host ""

if ($Failed -gt 0) {
    Write-Host "Failed benchmarks:" -ForegroundColor Red
    foreach ($name in $FailedBenchmarks) {
        Write-Host "  - $name"
    }
    Write-Host ""
}

Write-Host "Timing breakdown:"
foreach ($timing in $Timings) {
    if ($null -ne $timing) {
        Write-Host ("  {0,-40} {1,4}s" -f $timing.Name, $timing.Duration)
    }
}
Write-Host ""

$TotalTime = 0
foreach ($t in $Timings) { 
    if ($null -ne $t -and $null -ne $t.Duration) { 
        $TotalTime += $t.Duration 
    } 
}
$Minutes = [int][math]::Floor($TotalTime / 60)
$Seconds = [int]($TotalTime % 60)
$SecondsStr = $Seconds.ToString("00")
Write-Host "Total time: ${TotalTime}s (${Minutes}:${SecondsStr})" -ForegroundColor Yellow
Write-Host ""

Write-Host "Results saved to: $ResultsDir"
Write-Host ""
Write-Host "================================================================================" -ForegroundColor Blue
Write-Host "  Completed at $(Get-Date)" -ForegroundColor Blue
Write-Host "================================================================================" -ForegroundColor Blue

# Exit with failure if any benchmark failed
if ($Failed -gt 0) {
    exit 1
}
