<#
.SYNOPSIS
    Assemble a self-contained, portable folder (dist\GrammarRefine) you can zip
    and move to the airgapped machine.

.DESCRIPTION
    Run scripts\build.ps1 first. This copies the exe + config, runs windeployqt
    to gather Qt DLLs, and pulls in anything you've staged under runtime\llama
    and runtime\models.

.PARAMETER QtDir
    Qt MSVC kit (for windeployqt). Defaults to $env:QT_DIR, then autodetect.

.PARAMETER Config
    Build configuration the exe was built as (default Release).
#>
[CmdletBinding()]
param(
    [string]$QtDir = $env:QT_DIR,
    [string]$Config = 'Release'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'
$distDir  = Join-Path $repoRoot 'dist\GrammarRefine'

$exe = Join-Path $buildDir "$Config\GrammarRefine.exe"
if (-not (Test-Path $exe)) { $exe = Join-Path $buildDir 'GrammarRefine.exe' }
if (-not (Test-Path $exe)) {
    throw "GrammarRefine.exe not found. Run scripts\build.ps1 first."
}

function Find-QtBin {
    if ($QtDir -and (Test-Path $QtDir)) { return (Join-Path $QtDir 'bin') }
    $versions = Get-ChildItem 'C:\Qt' -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -match '^\d+\.\d+' } | Sort-Object Name -Descending
    foreach ($v in $versions) {
        $kit = Get-ChildItem $v.FullName -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like 'msvc*_64' } | Select-Object -First 1
        if ($kit) { return (Join-Path $kit.FullName 'bin') }
    }
    return $null
}

# fresh dist
if (Test-Path $distDir) { Remove-Item $distDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $distDir            | Out-Null
New-Item -ItemType Directory -Force -Path "$distDir\llama"    | Out-Null
New-Item -ItemType Directory -Force -Path "$distDir\models"   | Out-Null

Copy-Item $exe $distDir
Copy-Item (Join-Path $repoRoot 'assets\config.json') $distDir

# Qt runtime
$qtBin = Find-QtBin
$windeploy = if ($qtBin) { Join-Path $qtBin 'windeployqt.exe' } else { $null }
if ($windeploy -and (Test-Path $windeploy)) {
    Write-Host "Running windeployqt (lean)..." -ForegroundColor Cyan
    & $windeploy (Join-Path $distDir 'GrammarRefine.exe') `
        --release --no-translations --no-system-d3d-compiler --no-opengl-sw --compiler-runtime
} else {
    Write-Warning "windeployqt not found - copy the Qt DLLs into $distDir manually."
}

# --- Bundle the MSVC runtime (reliable: copy straight from System32) ----------
# windeployqt --compiler-runtime only works from a Developer prompt; this is the
# fallback so the app runs on a clean / airgapped box without the VC++ redist.
$sys = Join-Path $env:WINDIR 'System32'
foreach ($d in 'vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll') {
    $src = Join-Path $sys $d
    if (Test-Path $src) { Copy-Item $src $distDir -Force }
    else { Write-Warning "MSVC runtime $d not found in System32 - bundle it manually for the target." }
}

# --- Trim plugins a plain text app does not use ------------------------------
foreach ($p in 'imageformats','iconengines','generic') {
    $dir = Join-Path $distDir $p
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
}
foreach ($d in 'Qt6Pdf.dll','Qt6Svg.dll') {
    $f = Join-Path $distDir $d
    if (Test-Path $f) { Remove-Item $f -Force }
}

# staged llama-server + model (if you've put them under runtime\)
Get-ChildItem (Join-Path $repoRoot 'runtime\llama')  -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne 'README.txt' } | Copy-Item -Destination "$distDir\llama"
Get-ChildItem (Join-Path $repoRoot 'runtime\models') -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne 'README.txt' } | Copy-Item -Destination "$distDir\models"

Write-Host "`nPortable folder ready: $distDir" -ForegroundColor Green
$haveLlama  = (Get-ChildItem "$distDir\llama"  -Filter *.exe -ErrorAction SilentlyContinue).Count -gt 0
$haveModel  = (Get-ChildItem "$distDir\models" -Filter *.gguf -ErrorAction SilentlyContinue).Count -gt 0
$haveVcrt   = Test-Path (Join-Path $distDir 'vcruntime140.dll')
$haveCuda   = (Get-ChildItem "$distDir\llama" -Filter 'cudart64_*.dll' -ErrorAction SilentlyContinue).Count -gt 0
$sizeGB     = [math]::Round((Get-ChildItem $distDir -Recurse -File | Measure-Object Length -Sum).Sum / 1GB, 2)

Write-Host ("Mode: {0}   VC runtime: {1}   Size: {2} GB" -f `
    ($(if ($haveCuda) {'GPU (CUDA libs bundled)'} else {'CPU-only (no CUDA libs)'})), `
    ($(if ($haveVcrt) {'bundled'} else {'MISSING'})), $sizeGB) -ForegroundColor Cyan

if (-not $haveLlama)  { Write-Warning "No llama-server.exe in dist\llama - stage it under runtime\llama and re-run." }
if (-not $haveModel)  { Write-Warning "No .gguf in dist\models - stage it under runtime\models and re-run." }
if (-not $haveVcrt)   { Write-Warning "vcruntime140.dll not bundled - the app may not start on a clean machine." }
if ($haveLlama -and $haveModel) {
    Write-Host "Looks complete. Zip $distDir and transfer it to the airgapped machine." -ForegroundColor Green
}
