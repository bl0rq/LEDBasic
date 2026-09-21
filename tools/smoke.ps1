$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exe = Join-Path $root "tools\build\ledbasic.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $root "tools\build\ledbasic"
}
if (-not (Test-Path $exe)) {
    Write-Error "Build the host tools first: cmake -B tools/build -S tools && cmake --build tools/build"
}
$bas = Join-Path $root "examples\bas\Rainbow.bas"
& $exe run $bas --leds 60 --frames 3 --dump-ascii
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host "smoke ok"
