# TiDaoji anti-debug A/B helper (partial automation).
# A0: run probe outside debugger
# A1/B1: use x64dbg MCP or GUI per protocol doc
# Requires: antidebug_probe.exe built next to this script or TOOLS dir

param(
  [string]$Probe = "",
  [string]$OutDir = "C:\"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $Probe) {
  $candidates = @(
    (Join-Path $here "antidebug_probe.exe"),
    "D:\src\TiDaoji\tools\antidebug_probe.exe"
  )
  foreach ($c in $candidates) { if (Test-Path $c) { $Probe = $c; break } }
}
if (-not (Test-Path $Probe)) {
  Write-Host "[!] antidebug_probe.exe missing. cl /O2 /EHsc /Fe:antidebug_probe.exe antidebug_probe.cpp"
  exit 1
}

$a0 = Join-Path $OutDir "TiDaoji_probe_A0.txt"
Write-Host "=== A0: no debugger ==="
& $Probe -o $a0
$ec = $LASTEXITCODE
Write-Host "exit=$ec"
Get-Content $a0
Write-Host ""
Write-Host "=== Next: A1/B1 via x64dbg ==="
Write-Host "  init `"$Probe`""
Write-Host "  TiDaojiUnhide  then run once -> save as A1"
Write-Host "  TiDaoji        then re-init/run -> B1"
Write-Host "Protocol: docs\2026-08-07-tidaoji-vmp-antidebug-live-protocol.md"
exit $ec
