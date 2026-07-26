$ErrorActionPreference = 'Stop'

$desktopCandidates = @(
    [Environment]::GetFolderPath('Desktop'),
    (Join-Path $env:USERPROFILE 'Desktop'),
    (Join-Path $env:USERPROFILE 'OneDrive\Desktop'),
    (Join-Path $env:USERPROFILE 'OneDrive\Bureau')
) | Select-Object -Unique

foreach ($directory in $desktopCandidates) {
    "DESKTOP|$directory|exists=$(Test-Path -LiteralPath $directory)"
    if (Test-Path -LiteralPath $directory) {
        Get-ChildItem -LiteralPath $directory -File -Filter '*.ogg' |
            ForEach-Object { "OGG|$($_.FullName)|$($_.Length)" }
    }
}
