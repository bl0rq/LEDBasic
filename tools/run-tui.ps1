# Open the TUI in a new console window. Do not invoke `wt.exe` ourselves:
# `--title LEDBasic TUI` is split so Windows Terminal tries to launch a
# command named `TUI` and fails with 0x80070002.
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$TuiArgs
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$exe = Join-Path $root "tools\build\tui\ledbasic-tui.exe"
if (-not (Test-Path $exe)) {
    $exe = Join-Path $root "tools\build\tui\ledbasic-tui"
}
if (-not (Test-Path $exe)) {
    Write-Error "ledbasic-tui not found. Build first (Ctrl+Shift+B)."
}

# Inherited by the child if the exe is still dynamically linked.
if (Test-Path "C:\msys64\ucrt64\bin") {
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
}

$start = @{
    FilePath         = $exe
    WorkingDirectory = $root
}
if ($TuiArgs) { $start.ArgumentList = $TuiArgs }
Start-Process @start | Out-Null
Write-Host "Opened $exe"
