@echo off
setlocal EnableDelayedExpansion
set src=%cd%\textures\
set dst=%cd%\data\update\

cd tools
cd ResourceBuilder
for /f "delims=" %%i in ('dir %src% /a:d/s/b') do (
  set "foldername=%%i"
  if /I "!foldername:~-4!"==".wtd" (
    set "string=%%~dpi"
    set "modified=!string:%src%=%dst%!"
    if not exist !modified! mkdir !modified!
    set "string=%%i"
    set "modified=!string:%src%=%dst%!"
    fftdc.exe -c_wtd_v8 !modified! -f %%i
  )
)
cd ..
cd ..
