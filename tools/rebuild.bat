@echo off
REM tools\rebuild.bat - wrapper that bypasses PowerShell execution policy
powershell -ExecutionPolicy Bypass -File "%~dp0rebuild.ps1" %*
