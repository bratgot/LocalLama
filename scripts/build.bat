@echo off
REM Thin wrapper around build.ps1 for cmd.exe users.
REM Usage:  scripts\build.bat [-QtDir C:\Qt\6.7.2\msvc2019_64] [-Clean]
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
