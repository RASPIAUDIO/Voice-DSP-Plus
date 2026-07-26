param(
    [int]$Repeats = 6,
    [int]$Volume = 75,
    [int]$PauseMs = 2500
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Speech

$logPath = Join-Path $env:USERPROFILE 'VoiceDSP-HeyJarvis-Test.log'
$speaker = [System.Speech.Synthesis.SpeechSynthesizer]::new()
$speaker.Volume = [Math]::Max(0, [Math]::Min(100, $Volume))
$speaker.Rate = -1

try {
    "START $(Get-Date -Format o) repeats=$Repeats volume=$Volume pause_ms=$PauseMs" |
        Set-Content -Path $logPath -Encoding ascii
    for ($index = 1; $index -le $Repeats; $index++) {
        "SAY $index $(Get-Date -Format o)" | Add-Content -Path $logPath -Encoding ascii
        $speaker.Speak('Hey Jarvis')
        Start-Sleep -Milliseconds $PauseMs
    }
    "DONE $(Get-Date -Format o)" | Add-Content -Path $logPath -Encoding ascii
}
finally {
    $speaker.Dispose()
}
