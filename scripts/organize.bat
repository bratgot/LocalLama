@echo off
REM Thin wrapper around organize.ps1 for cmd.exe users.
REM Usage:  scripts\organize.bat
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0organize.ps1" %*
