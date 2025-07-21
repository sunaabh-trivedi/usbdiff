@echo off
setlocal

:: Get absolute folder path (no trailing backslash)
set "FOLDER=%~dp0"
if "%FOLDER:~-1%"=="\" set "FOLDER=%FOLDER:~0,-1%"

:: Get current PATH
for /f "tokens=2*" %%A in ('reg query HKCU\Environment /v PATH 2^>nul') do set "OLD_PATH=%%B"

:: If no old path, set a default
if not defined OLD_PATH set "OLD_PATH="

:: Check if already added
echo %OLD_PATH% | find /i "%FOLDER%" >nul
if not errorlevel 1 (
    echo Folder already in PATH.
    goto :eof
)

:: Add to PATH
setx PATH "%OLD_PATH%;%FOLDER%"
echo.
echo Added "%FOLDER%" to your user PATH.
echo Open a new terminal and run: usbdiff <directory>
