param(
    [Parameter(Mandatory = $true)]
    [string]$Path,
    [int]$VolumePercent = 100,
    [double]$StartSeconds = 0,
    [double]$DurationSeconds = 0
)

$ErrorActionPreference = 'Stop'
$taskName = 'VoiceDSPPlusAudioPlayback'
$runner = "$env:USERPROFILE\voice-dsp-plus-audio-runner.ps1"

if (-not (Test-Path -LiteralPath $Path)) {
    throw "Audio file not found: $Path"
}

$escapedPath = $Path.Replace("'", "''")
@"
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class AudioKeys {
    [DllImport("user32.dll")]
    public static extern void keybd_event(byte key, byte scan, uint flags, UIntPtr extraInfo);
}
'@

# Reset master volume to zero, then raise it to the requested percentage.
for (`$i = 0; `$i -lt 60; `$i++) {
    [AudioKeys]::keybd_event(0xAE, 0, 0, [UIntPtr]::Zero)
    [AudioKeys]::keybd_event(0xAE, 0, 2, [UIntPtr]::Zero)
}
`$steps = [Math]::Ceiling($VolumePercent / 2.0)
for (`$i = 0; `$i -lt `$steps; `$i++) {
    [AudioKeys]::keybd_event(0xAF, 0, 0, [UIntPtr]::Zero)
    [AudioKeys]::keybd_event(0xAF, 0, 2, [UIntPtr]::Zero)
}

`$ffplay = 'C:\ProgramData\chocolatey\bin\ffplay.exe'
`$arguments = @('-nodisp', '-autoexit', '-loglevel', 'error', '-volume', '$VolumePercent')
if ($StartSeconds -gt 0) { `$arguments += @('-ss', '$StartSeconds') }
if ($DurationSeconds -gt 0) { `$arguments += @('-t', '$DurationSeconds') }
`$arguments += '$escapedPath'
& `$ffplay @arguments
"@ | Set-Content -LiteralPath $runner -Encoding UTF8

$action = New-ScheduledTaskAction `
    -Execute 'powershell.exe' `
    -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$runner`""
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(5)
$principal = New-ScheduledTaskPrincipal `
    -UserId "$env:USERDOMAIN\$env:USERNAME" `
    -LogonType Interactive `
    -RunLevel Limited

Register-ScheduledTask `
    -TaskName $taskName `
    -Action $action `
    -Trigger $trigger `
    -Principal $principal `
    -Force | Out-Null
Start-ScheduledTask -TaskName $taskName
Write-Output "Started playback at $VolumePercent%: $Path"
