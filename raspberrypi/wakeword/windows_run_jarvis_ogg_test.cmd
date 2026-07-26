@echo off
set "TASK=VoiceDSP-Jarvis-Ogg-Test"
set "SCRIPT=%USERPROFILE%\VoiceDSP-Jarvis-Ogg-Test.ps1"
set "PLAY_VOLUME=50"
schtasks /Create /TN "%TASK%" /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File %SCRIPT% -Volume %PLAY_VOLUME%" /SC ONCE /ST 23:59 /RU "%USERNAME%" /IT /F
if errorlevel 1 exit /b %errorlevel%
schtasks /Run /TN "%TASK%"
