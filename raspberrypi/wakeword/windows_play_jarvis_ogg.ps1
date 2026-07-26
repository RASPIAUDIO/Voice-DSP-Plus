param(
    [string]$AudioPath = (Join-Path $env:USERPROFILE 'Desktop\jarvis.ogg'),
    [int]$Volume = 75
)

$ErrorActionPreference = 'Stop'
$python = Get-ChildItem -Path "$env:LOCALAPPDATA\Programs\Python\Python*\python.exe" `
    -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $python) {
    $python = (Get-Command python.exe -ErrorAction SilentlyContinue).Source
}
$player = Join-Path $env:USERPROFILE 'VoiceDSP-Jarvis-WASAPI.py'
$logPath = Join-Path $env:USERPROFILE 'VoiceDSP-Jarvis-Ogg-Test.log'

if (-not (Test-Path -LiteralPath $AudioPath)) {
    throw "Audio file not found: $AudioPath"
}
if (-not $python -or -not (Test-Path -LiteralPath $python)) {
    throw 'Python not found. Install Python 3 and ensure python.exe is available.'
}
if (-not (Test-Path -LiteralPath $player)) {
    throw "WASAPI player not found: $player"
}

$safeVolume = [Math]::Max(0, [Math]::Min(100, $Volume))
"START $(Get-Date -Format o) path=$AudioPath volume=$safeVolume" |
    Add-Content -Path $logPath -Encoding ascii

$level = $safeVolume / 100.0
$output = & $python $player $AudioPath --device Conexant --level $level 2>&1
$output | Add-Content -Path $logPath -Encoding ascii
if ($LASTEXITCODE -ne 0) {
    throw "WASAPI player failed with exit code $LASTEXITCODE"
}

"DONE $(Get-Date -Format o)" | Add-Content -Path $logPath -Encoding ascii
