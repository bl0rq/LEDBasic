# Configure and build host CLI + TUI. Prepends MSYS2 if present (same as test/native/run.ps1).
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $root

if (Test-Path "C:\msys64\ucrt64\bin") {
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Error "cmake not found. Install CMake, or MSYS2 package mingw-w64-ucrt-x86_64-cmake."
}

Write-Host "Configuring host tools..."
cmake -B tools/build -S tools -DLEDBASIC_BUILD_TUI=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Building..."
cmake --build tools/build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$tui = Join-Path $root "tools\build\tui\ledbasic-tui.exe"
if (-not (Test-Path $tui)) {
    $tui = Join-Path $root "tools\build\tui\ledbasic-tui"
}
if (Test-Path $tui) {
    Write-Host "TUI: $tui"
}
exit 0
