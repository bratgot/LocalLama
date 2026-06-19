@echo off
REM Thin wrapper around package.ps1 for cmd.exe users.
REM Usage:  scripts\package.bat [-QtDir C:\Qt\6.7.2\msvc2019_64] [-Config Release]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0package.ps1" %*
