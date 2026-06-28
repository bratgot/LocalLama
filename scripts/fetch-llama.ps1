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

# Local-only app: loopback HTTP server, models downloaded manually. Disable both
# network features so the build drops their runtime deps:
#   LLAMA_CURL=OFF    - no libcurl (model URL downloads)
#   LLAMA_OPENSSL=OFF - no HTTPS in the server, so no libssl-3-x64__.dll /
#                       libcrypto-3-x64__.dll dependency (the missing-DLL error)
# [string[]] cast is required: a single-element @(...) from if/else unwraps to a
# scalar string, which would then string-concat under += / mis-splat to cmake.
# For a redistributable GPU build, compile kernels for a broad range of NVIDIA
# architectures instead of just this machine's native one. Without this, ggml
# defaults to the build host's arch (e.g. Blackwell sm_120) and llama-server
# crashes with "no kernel image available" on any other GPU. Covered:
#   75 Turing · 80 A100 · 86 Ampere(RTX30) · 89 Ada(RTX40 / RTX Ada) ·
#   90 Hopper · 120 Blackwell(RTX50)  + 120-virtual PTX as a forward-compat JIT.
$cudaArch = '75-real;80-real;86-real;89-real;90-real;120-real;120-virtual'
[string[]]$cudaArgs = if ($Cpu) {
    '-DLLAMA_CURL=OFF','-DLLAMA_OPENSSL=OFF'
} else {
    # GGML_CUDA_NCCL=OFF: skip the multi-GPU NCCL dependency (unused single-GPU app).
    '-DGGML_CUDA=ON','-DGGML_CUDA_NCCL=OFF',"-DCMAKE_CUDA_ARCHITECTURES=$cudaArch",'-DLLAMA_CURL=OFF','-DLLAMA_OPENSSL=OFF'
}
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

# 4. CUDA runtime DLLs (GPU build only) so it runs on a box without CUDA installed.
#    Bundle the version ggml-cuda.dll was actually built against (e.g. cudart64_12)
#    by reading its import table - NOT the highest-numbered toolkit, which may be a
#    newer major version (v13.x) whose DLLs ggml-cuda.dll does not load. cublas64
#    pulls in cublasLt64 transitively, so include it too.
if (-not $Cpu) {
    $ggmlCuda = Join-Path $stageLlama 'ggml-cuda.dll'

    # locate dumpbin from the VS install to read ggml-cuda.dll's CUDA imports
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vsPath  = if (Test-Path $vswhere) { & $vswhere -latest -property installationPath } else { $null }
    $dumpbin = if ($vsPath) {
        Get-ChildItem "$vsPath\VC\Tools\MSVC\*\bin\Hostx64\x64\dumpbin.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    } else { $null }

    # determine the CUDA major version from the cudart import (e.g. "12")
    $ver = $null
    if ($dumpbin -and (Test-Path $ggmlCuda)) {
        $m = & $dumpbin.FullName /dependents $ggmlCuda |
            Select-String -Pattern 'cudart64_(\d+)\.dll' | Select-Object -First 1
        if ($m) { $ver = $m.Matches[0].Groups[1].Value }
    }

    if ($ver) {
        $needed = @("cudart64_$ver.dll", "cublas64_$ver.dll", "cublasLt64_$ver.dll")
    } else {
        Write-Warning "Could not read CUDA version from ggml-cuda.dll - falling back to newest-toolkit wildcard copy."
        $needed = @('cudart64_*.dll', 'cublas64_*.dll', 'cublasLt64_*.dll')
    }

    # all installed toolkit bin dirs, newest first; search each needed DLL across them
    $cudaBins = Get-ChildItem 'C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA' -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'bin' } | Where-Object { Test-Path $_ }

    $missing = @()
    foreach ($dll in $needed) {
        $hit = $null
        foreach ($bin in $cudaBins) {
            $hit = Get-ChildItem (Join-Path $bin $dll) -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($hit) { break }
        }
        if ($hit) {
            Copy-Item $hit.FullName $stageLlama -Force
            Write-Host "  bundled CUDA runtime: $($hit.Name)  ($($hit.DirectoryName))" -ForegroundColor DarkGray
        } else {
            $missing += $dll
        }
    }
    if ($missing) {
        Write-Warning ("CUDA runtime NOT bundled: {0}. The packaged app will fail with a missing-DLL error on any machine without CUDA installed. Install the matching CUDA Toolkit or copy these next to llama-server.exe manually." -f ($missing -join ', '))
    }
}

Write-Host "`nStaged into $stageLlama :" -ForegroundColor Green
Get-ChildItem $stageLlama -File | Select-Object Name
Write-Host "`nNext: download a model to runtime\models\model.gguf (if needed), then .\scripts\package.bat" -ForegroundColor Green
