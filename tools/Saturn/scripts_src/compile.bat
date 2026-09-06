@echo off

if not defined SATURN (
    echo Error: Saturn is not installed.
    exit
)

cd /d "%~dp0"

if not exist "src\." (
    echo Error: Source directory not found in "%CD%".
    exit
)

"%SATURN%" --compile --src "src" --out "scripts\pc"

pause
exit
