@echo off
REM tools\build.bat - wrapper that bypasses PowerShell execution policy
powershell -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
