param(
    [string]$BuildPreset = "ua-io48-lin",
    [string]$ConfigurePreset = "rel_app_xvf3800_windows",
    [string]$SubstDrive = "X",
    [switch]$NoConfigure
)

$ErrorActionPreference = "Stop"

$PackageRoot = (Resolve-Path $PSScriptRoot).Path.TrimEnd("\")
$EnvScript = Join-Path $PackageRoot "xvf3800_env.ps1"
. $EnvScript

$drive = $SubstDrive.TrimEnd(":", "\").ToUpperInvariant()
$driveRoot = "$drive`:"
$existingMapping = (& subst) | Where-Object { $_.StartsWith("${driveRoot}\:") } | Select-Object -First 1

if ($existingMapping) {
    $mappedPath = (($existingMapping -split "=>", 2)[1]).Trim().TrimEnd("\")
    if ([string]::Compare($mappedPath, $PackageRoot, $true) -ne 0) {
        throw "$driveRoot is already mapped to '$mappedPath'. Re-run with -SubstDrive using a free drive letter."
    }
} else {
    & subst $driveRoot $PackageRoot
}

$ShortSources = "$driveRoot\sources"
$CMake = "C:\Program Files\CMake\bin\cmake.exe"
if (-not (Test-Path $CMake)) {
    $CMake = (Get-Command cmake.exe -ErrorAction Stop).Source
}

Push-Location $ShortSources
try {
    if (-not $NoConfigure) {
        & $CMake --fresh "--preset=$ConfigurePreset" "-DPython3_EXECUTABLE=$env:Python3_EXECUTABLE" -Wno-dev
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configure failed with exit code $LASTEXITCODE"
        }
    }

    & $CMake --build "--preset=$BuildPreset"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}
