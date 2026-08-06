# TiDaoji Build & Deploy Script
# Run from the TitanHide repo root on Windows
# Requires: Visual Studio 2022 with WDK, x64dbg installed

param(
    [string]$X64DbgDir = "C:\Users\Administrator\Downloads\ceAndXdbg\x96Dbg\x64",
    [string]$DriverServiceName = "TiDaoji",
    [switch]$SkipDriver,
    [switch]$SkipPlugin
)

$ErrorActionPreference = "Stop"
$RepoRoot = $PSScriptRoot

# --- Locate MSBuild ---
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    Write-Error "vswhere not found. Install Visual Studio 2022."
}
$vsPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath
$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
    $msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
}
Write-Host "[BUILD] MSBuild: $msbuild"

# --- Build x64dbg Plugin (Release|x64) ---
if (-not $SkipPlugin) {
    Write-Host "`n=== Building TiDaoji x64dbg Plugin ==="
    & $msbuild "$RepoRoot\TiDaoji_x64dbg\TiDaoji_x64dbg.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Rebuild /v:minimal
    if ($LASTEXITCODE -ne 0) { Write-Error "Plugin build failed" }

    $pluginSrc = "$RepoRoot\x64\Release\plugins\TiDaoji.dp64"
    if (-not (Test-Path $pluginSrc)) { Write-Error "Plugin output not found: $pluginSrc" }

    $pluginDst = "$X64DbgDir\plugins\TiDaoji.dp64"
    Write-Host "[DEPLOY] Plugin: $pluginSrc -> $pluginDst"
    Copy-Item $pluginSrc $pluginDst -Force
    Write-Host "[OK] Plugin deployed"
}

# --- Build Kernel Driver (Release|x64) ---
if (-not $SkipDriver) {
    Write-Host "`n=== Building TiDaoji Kernel Driver ==="
    & $msbuild "$RepoRoot\TiDaoji\TiDaoji.vcxproj" /p:Configuration=Release /p:Platform=x64 /t:Rebuild /v:minimal
    if ($LASTEXITCODE -ne 0) { Write-Error "Driver build failed" }

    $driverSrc = Get-ChildItem "$RepoRoot" -Recurse -Filter "TiDaoji.sys" |
                 Where-Object { $_.FullName -match "Release" -and $_.FullName -match "x64" } |
                 Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if (-not $driverSrc) { Write-Error "Driver .sys not found in build output" }

    Write-Host "[DEPLOY] Driver: $($driverSrc.FullName)"

    # Stop existing driver
    Write-Host "[DEPLOY] Stopping existing driver..."
    sc.exe stop $DriverServiceName 2>$null
    Start-Sleep -Seconds 2

    # Find driver path from service config, or use default
    $svcQuery = sc.exe qc $DriverServiceName 2>$null
    $existingPath = ""
    if ($svcQuery) {
        $binLine = $svcQuery | Where-Object { $_ -match "BINARY_PATH_NAME" }
        if ($binLine -match ":\s+(.+)$") { $existingPath = $Matches[1].Trim() }
    }

    if ($existingPath -and (Test-Path $existingPath)) {
        $driverDst = $existingPath
    } else {
        $driverDst = "C:\Windows\System32\drivers\TiDaoji.sys"
    }

    Write-Host "[DEPLOY] Copy driver to $driverDst"
    Copy-Item $driverSrc.FullName $driverDst -Force

    # Start driver
    Write-Host "[DEPLOY] Starting driver..."
    sc.exe start $DriverServiceName
    if ($LASTEXITCODE -ne 0) {
        Write-Warning "sc start failed. Driver may need manual loading (DSE/kdmapper)."
    } else {
        Write-Host "[OK] Driver started"
    }
}

Write-Host "`n=== Build & Deploy Complete ==="
Write-Host "Next: Restart x64dbg, run TiDaojiStatus to verify."
Write-Host "GUI: Plugins menu -> TiDaoji -> Settings..."
