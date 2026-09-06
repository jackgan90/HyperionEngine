@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\GenerateSolution.ps1" %*
set "GENERATE_RESULT=%ERRORLEVEL%"
echo.
if not "%GENERATE_RESULT%"=="0" echo Solution generation failed. See the error above.
pause
exit /b %GENERATE_RESULT%
