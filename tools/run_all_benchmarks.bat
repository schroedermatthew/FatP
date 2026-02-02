@echo off
REM tools\run_all_benchmarks.bat - wrapper that bypasses PowerShell execution policy
powershell -ExecutionPolicy Bypass -File "%~dp0run_all_benchmarks.ps1" %*
