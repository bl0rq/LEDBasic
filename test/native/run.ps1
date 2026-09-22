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

Write-Host "Compiling LEDBASIC_NO_FASTLED object"
$obj = Join-Path $native "BasicInterpreter_nofastled.o"
& $cxx -std=c++17 -c -O0 -g -DLEDBASIC_NO_FASTLED `
    "-I$native" `
    "-I$(Join-Path $root 'include')" `
    $src `
    -o $obj
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Remove-Item $obj -ErrorAction SilentlyContinue

Write-Host "Compiling WLED runtime (native)"
$runtimeOut = Join-Path $native "test_wled_runtime.exe"
$wled = Join-Path $root "wled"
& $cxx -std=c++17 -O0 -g -DLEDBASIC_NO_FASTLED `
    "-I$native" `
    "-I$(Join-Path $root 'include')" `
    "-I$wled" `
    (Join-Path $wled "ledbasic_interpreter_build.cpp") `
    (Join-Path $wled "ledbasic_programs.cpp") `
    (Join-Path $wled "ledbasic_runtime.cpp") `
    (Join-Path $native "test_wled_runtime.cpp") `
    -o $runtimeOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $runtimeOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "Compiling WLED client JSON"
$clientOut = Join-Path $native "test_wled_client.exe"
& $cxx -std=c++17 -O0 -g `
    "-I$(Join-Path $root 'tools\host')" `
    (Join-Path $root "tools\host\WledJson.cpp") `
    (Join-Path $native "test_wled_client.cpp") `
    -o $clientOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $clientOut
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& $out
exit $LASTEXITCODE
