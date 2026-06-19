<#
.SYNOPSIS
    Reorganise a FLAT checkout (all files dumped in the repo root) into the
    standard layout used by this project. Only moves known files; safe to re-run.

.DESCRIPTION
    Use this if you already have a folder where main.cpp, MainWindow.cpp,
    config.json, etc. all sit at the top level. It creates the standard
    directories and moves files into them.

    After running, replace CMakeLists.txt with the updated one from this repo
    (config.json now lives in assets\), and copy in the scripts\ folder and
    CMakePresets.json if your old checkout didn't have them.

.PARAMETER Root
    The folder to reorganise. Defaults to the current directory.
#>
[CmdletBinding()]
param([string]$Root = (Get-Location).Path)

$ErrorActionPreference = 'Stop'
Push-Location $Root
try {
    Write-Host "Reorganising: $Root" -ForegroundColor Cyan

    foreach ($d in 'src','assets','scripts','cmake','docs','runtime\llama','runtime\models') {
        New-Item -ItemType Directory -Force -Path $d | Out-Null
    }

    # C++ sources -> src\
    Get-ChildItem -File -Filter *.cpp -ErrorAction SilentlyContinue | Move-Item -Destination src -Force
    Get-ChildItem -File -Filter *.h   -ErrorAction SilentlyContinue | Move-Item -Destination src -Force

    # runtime config -> assets\
    if (Test-Path config.json) { Move-Item config.json assets -Force }

    # stray placeholder README from the old bin/models folders
    if (Test-Path README.txt) { Move-Item README.txt 'runtime\llama\README.txt' -Force }

    # download artifact folder, if present
    if (Test-Path mnt) { Remove-Item mnt -Recurse -Force }

    Write-Host "`nDone. Top-level layout:" -ForegroundColor Green
    Get-ChildItem | Select-Object Mode, Name | Format-Table -AutoSize
    Write-Host "Reminder: use the updated CMakeLists.txt (config now in assets\)." -ForegroundColor Yellow
}
finally { Pop-Location }
