<#
.SYNOPSIS
    Clone (if needed), build llama.cpp, and stage llama-server + its DLLs into
    runtime\llama so package.ps1 can bundle them. Re-run this whenever
    runtime\llama gets emptied.

.PARAMETER Cpu
    Build CPU-only (no CUDA). Smaller, zero VRAM, slower. Omit for a GPU/CUDA build.

.PARAMETER Dir
    Where llama.cpp lives/clones to. Default: <repo>\llama.cpp

.PARAMETER Generator
    Override the CMake generator (else the latest installed Visual Studio).

.EXAMPLE
    .\scripts\fetch-llama.ps1            # GPU/CUDA build + stage
    .\scripts\fetch-llama.ps1 -Cpu       # CPU-only build + stage
#>
[CmdletBinding()]
param(
    [switch]$Cpu,
    [string]$Dir,
    [string]$Generator
)

$ErrorActionPreference = 'Stop'
$repoRoot   = Split-Path -Parent $PSScriptRoot
if (-not $Dir) { $Dir = Join-Path $repoRoot 'llama.cpp' }
$stageLlama = Join-Path $repoRoot 'runtime\llama'

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
    throw "cmake not found on PATH."
}

# 1. clone if missing
if (-not (Test-Path (Join-Path $Dir 'CMakeLists.txt'))) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw "git not found, and $Dir has no llama.cpp source." }
    Write-Host "Cloning llama.cpp into $Dir ..." -ForegroundColor Cyan
    git clone https://github.com/ggml-org/llama.cpp $Dir
    if ($LASTEXITCODE -ne 0) { throw "git clone failed." }
}

# 2. configure + build (force MSVC; nvcc needs cl, and it matches the Qt app)
$gen = if ($Generator) { $Generator } else { Get-VsGenerator }
if (-not $gen) { throw "No Visual Studio C++ toolchain found. Install VS 2019/2022 or the Build Tools (Desktop C++)." }

$cudaArgs = if ($Cpu) { @() } else { @('-DGGML_CUDA=ON') }
$mode = if ($Cpu) { 'CPU-only' } else { 'CUDA/GPU' }
Write-Host "Building llama.cpp ($mode) with $gen ..." -ForegroundColor Cyan

$build = Join-Path $Dir 'build'
cmake -S $Dir -B $build -G $gen -A x64 @cudaArgs
if ($LASTEXITCODE -ne 0) { throw "llama.cpp configure failed." }
cmake --build $build --config Release -j
if ($LASTEXITCODE -ne 0) { throw "llama.cpp build failed." }

# 3. stage llama-server + DLLs into runtime\llama (clear old, keep README)
$rel = Join-Path $build 'bin\Release'
if (-not (Test-Path (Join-Path $rel 'llama-server.exe'))) { throw "llama-server.exe not found in $rel" }
New-Item -ItemType Directory -Force -Path $stageLlama | Out-Null
Get-ChildItem $stageLlama -File | Where-Object { $_.Name -ne 'README.txt' } | Remove-Item -Force
Copy-Item (Join-Path $rel 'llama-server.exe') $stageLlama
Copy-Item (Join-Path $rel '*.dll') $stageLlama

# 4. CUDA runtime DLLs (GPU build only) so it runs on a box without CUDA installed
if (-not $Cpu) {
    $cudaRoot = Get-ChildItem 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($cudaRoot) {
        Copy-Item "$($cudaRoot.FullName)\bin\cudart64_*.dll",`
                  "$($cudaRoot.FullName)\bin\cublas64_*.dll",`
                  "$($cudaRoot.FullName)\bin\cublasLt64_*.dll" $stageLlama -ErrorAction SilentlyContinue
    } else {
        Write-Warning "CUDA Toolkit not found - bundle the CUDA runtime DLLs manually for the airgapped box."
    }
}

Write-Host "`nStaged into $stageLlama :" -ForegroundColor Green
Get-ChildItem $stageLlama -File | Select-Object Name
Write-Host "`nNext: download a model to runtime\models\model.gguf (if needed), then .\scripts\package.bat" -ForegroundColor Green
