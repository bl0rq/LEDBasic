$ErrorActionPreference = "Stop"
$native = $PSScriptRoot
$root = (Resolve-Path (Join-Path $native "..\..")).Path
$src = Join-Path $root "src\BasicInterpreter.cpp"
$test = Join-Path $native "test_interpreter.cpp"
$out = Join-Path $native "test_interpreter.exe"

if (Test-Path "C:\msys64\ucrt64\bin") {
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
}
$cxx = $null
foreach ($cand in @("g++", "clang++")) {
    if (Get-Command $cand -ErrorAction SilentlyContinue) { $cxx = $cand; break }
}
if (-not $cxx) {
    Write-Error "Need g++ or clang++ on PATH to run native tests."
}

Write-Host "Compiling with $cxx"
& $cxx -std=c++17 -O0 -g `
    "-I$native" `
    "-I$(Join-Path $root 'include')" `
    $src $test `
    -o $out

if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $out
exit $LASTEXITCODE
