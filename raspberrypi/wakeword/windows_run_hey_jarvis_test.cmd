@echo off
set "TASK=VoiceDSP-HeyJarvis-Test"
set "SCRIPT=%USERPROFILE%\VoiceDSP-HeyJarvis-Test.ps1"
schtasks /Create /TN "%TASK%" /TR "powershell.exe -NoProfile -ExecutionPolicy Bypass -File %SCRIPT% -Repeats 6 -Volume 75 -PauseMs 2500" /SC ONCE /ST 23:59 /RU "%USERNAME%" /IT /F
if errorlevel 1 exit /b %errorlevel%
schtasks /Run /TN "%TASK%"
