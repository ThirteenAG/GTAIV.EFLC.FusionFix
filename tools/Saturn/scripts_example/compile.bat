@echo off
cd /d "%~dp0"

set "SATURN="
for /f "tokens=2,*" %%a in ('reg query "HKCU\Software\ELO\Saturn" /v Path 2^>nul') do if exist "%%b\saturn.exe" set "SATURN=%%b\saturn.exe"

if not defined SATURN (
    echo Saturn is not installed.
    pause
    exit /b 1
)

"%SATURN%" --compile --src "src" --out "scripts\pc"
pause
