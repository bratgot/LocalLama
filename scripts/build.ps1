<#
.SYNOPSIS
    Configure and build LlamaChat with CMake, using a toolchain that matches
    your Qt kit.

.DESCRIPTION
    Run from anywhere; paths resolve relative to the repo root.

    IMPORTANT: the compiler must match how Qt was built.
      * An MSVC Qt kit  (..\msvc2019_64)  -> Visual Studio generator (cl.exe)
      * A  MinGW Qt kit (..\mingw_64)      -> Ninja + GCC
    Mixing them gives 'undefined reference to __imp_...' link errors. This script
    picks the right one automatically from the Qt kit path, and ignores any
    stray GCC on PATH (e.g. Strawberry Perl) when the kit is MSVC.

.PARAMETER QtDir
    Qt kit, e.g. C:\Qt\6.7.2\msvc2019_64. Defaults to $env:QT_DIR, then
    autodetects the newest kit under C:\Qt.

.PARAMETER Config
    Release (default), Debug, or RelWithDebInfo.

.PARAMETER Generator
    Force a specific CMake generator (overrides the auto choice).

.PARAMETER Clean
    Delete the build directory before configuring.

.EXAMPLE
    .\scripts\build.ps1 -Clean
#>
[CmdletBinding()]
param(
    [string]$QtDir = $env:QT_DIR,
    [ValidateSet('Release','Debug','RelWithDebInfo')]
    [string]$Config = 'Release',
    [string]$Generator,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

function Find-QtDir {
    if ($QtDir -and (Test-Path $QtDir)) { return (Resolve-Path $QtDir).Path }
    $versions = Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+' } | Sort-Object Name -Descending
    # Prefer an MSVC kit across all versions, so build and package agree even
    # when a MinGW kit is also installed. Only fall back to MinGW if no MSVC.
    foreach ($v in $versions) {
        $msvc = Get-ChildItem $v.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like 'msvc*_64' } | Select-Object -First 1
        if ($msvc) { return $msvc.FullName }
    }
    foreach ($v in $versions) {
        $mingw = Get-ChildItem $v.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like 'mingw*_64' } | Select-Object -First 1
        if ($mingw) { return $mingw.FullName }
    }
    return $null
}

function Get-VsGenerator {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) { return $null }
    $line = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property catalog_productLineVersion 2>$null
    switch ("$line".Trim()) {
        '2022' { 'Visual Studio 17 2022' }
        '2019' { 'Visual Studio 16 2019' }
        '2017' { 'Visual Studio 15 2017' }
        default { $null }
    }
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "cmake not found on PATH. Install CMake, or use 'Developer PowerShell for VS'."
}

$qt = Find-QtDir
if (-not $qt) { throw "Qt not found. Pass -QtDir C:\Qt\<version>\msvc<...>_64 or set `$env:QT_DIR." }

Write-Host "Repo : $repoRoot"
Write-Host "Qt   : $qt" -ForegroundColor Cyan

# --- choose a generator that matches the Qt kit's ABI ------------------------
$genArgs    = @()
$desiredGen = $null

if ($Generator) {
    $genArgs   += @('-G', $Generator)
    $desiredGen = $Generator
}
elseif ($qt -match 'msvc') {
    $vsGen = Get-VsGenerator
    if (-not $vsGen) {
        throw @"
Your Qt kit is MSVC:
  $qt
but no Visual Studio C++ toolchain was found. MSVC-built Qt cannot be linked
with MinGW/GCC -- that is the 'undefined reference to __imp_...' error you hit.

Fix one of these:
  * Install Visual Studio 2019/2022 (or the standalone Build Tools) with the
    'Desktop development with C++' workload, then re-run this script; OR
  * Switch to a MinGW Qt kit, e.g.  C:\Qt\6.7.2\mingw_64 , and set
    `$env:QT_DIR to it (then a GCC toolchain matching that kit is used).
"@
    }
    $genArgs   += @('-G', $vsGen, '-A', 'x64')
    $desiredGen = $vsGen
    Write-Host "Tool : $vsGen (MSVC x64)" -ForegroundColor Cyan
}
elseif ($qt -match 'mingw') {
    if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
        throw "MinGW Qt kit needs Ninja on PATH (Qt ships one under Tools\Ninja, or install it)."
    }
    $genArgs   += @('-G', 'Ninja')
    $desiredGen = 'Ninja'
    Write-Host "Tool : Ninja + GCC (MinGW)" -ForegroundColor Cyan
    Write-Warning "Ensure the GCC on PATH matches the MinGW that built this Qt kit."
}
else {
    Write-Warning "Could not tell MSVC vs MinGW from the kit path; letting CMake choose its default generator."
}

# A build dir configured with a different generator must be recreated.
$cachePath = Join-Path $buildDir 'CMakeCache.txt'
if (-not $Clean -and $desiredGen -and (Test-Path $cachePath)) {
    $m = Select-String -Path $cachePath -Pattern '^CMAKE_GENERATOR:INTERNAL=(.+)$' -ErrorAction SilentlyContinue |
         Select-Object -First 1
    $cached = if ($m) { $m.Matches.Groups[1].Value } else { $null }
    if ($cached -and $cached -ne $desiredGen) {
        Write-Host "Generator changed ($cached -> $desiredGen); cleaning build dir." -ForegroundColor Yellow
        $Clean = $true
    }
}

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Cleaning $buildDir" -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

# CMAKE_BUILD_TYPE only matters for single-config generators (Ninja); the
# Visual Studio generator is multi-config and uses --config at build time.
$cfgArgs = @("-DCMAKE_PREFIX_PATH=$qt")
if ($desiredGen -notlike 'Visual Studio*') { $cfgArgs += "-DCMAKE_BUILD_TYPE=$Config" }

Write-Host "Configuring..." -ForegroundColor Cyan
cmake -S $repoRoot -B $buildDir @genArgs @cfgArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

Write-Host "Building ($Config)..." -ForegroundColor Cyan
cmake --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "Build failed." }

$exe = Join-Path $buildDir "$Config\LlamaChat.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $buildDir 'LlamaChat.exe' }

if (Test-Path $exe) {
    Write-Host "`nBuilt: $exe" -ForegroundColor Green
    Write-Host "Next:  .\scripts\package.ps1   (assembles the portable folder)" -ForegroundColor Green
} else {
    Write-Warning "Build reported success but LlamaChat.exe was not found under $buildDir."
}
