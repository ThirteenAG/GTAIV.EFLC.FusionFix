@echo off
setlocal

rem Binaries of other projects that go into the release package:
rem - DXVK: its d3d9.dll becomes vulkan.dll, which d3d9.dll of Fusion Fix loads for the Vulkan graphics API
rem - NVIDIA DLSS and AMD FidelityFX runtimes for DLAA and FSR, loaded by plugins\GTAIV.EFLC.FusionFix.exe.
rem   Their versions match the SDK headers the helper is built with, see external\README.md.
set "DXVK_VERSION=v3.1.1"
set "DLSS_VERSION=v310.9.1"
set "FIDELITYFX_VERSION=v2.3.0"

cd /d "%~dp0"
set "TAR=%SystemRoot%\System32\tar.exe"

rem DXVK
rem note: %%B keeps the quotes from the json, cmd strips them when the url reaches curl
set "DXVK_URL="
for /f "tokens=1,* delims=:" %%A in ('curl -fsS https://api.github.com/repos/doitsujin/dxvk/releases/tags/%DXVK_VERSION% ^| findstr /c:"browser_download_url"') do (
  if not defined DXVK_URL (
    echo.%%B | findstr /i /c:"/dxvk-%DXVK_VERSION:~1%.tar.gz" >nul && set "DXVK_URL=%%B"
  )
)
if not defined DXVK_URL set "DXVK_URL=https://github.com/doitsujin/dxvk/releases/download/%DXVK_VERSION%/dxvk-%DXVK_VERSION:~1%.tar.gz"

del data\vulkan.dll dxvk.tar.gz 2>nul
rmdir /s /q dxvk-tmp 2>nul
mkdir dxvk-tmp
curl -fsSL -o dxvk.tar.gz %DXVK_URL% || (
  echo Failed to download DXVK & exit /b 1
)
"%TAR%" -xzf dxvk.tar.gz -C dxvk-tmp
for /d %%D in (dxvk-tmp\dxvk-*) do if exist "%%D\x32\d3d9.dll" copy /y "%%D\x32\d3d9.dll" data\vulkan.dll >nul
del dxvk.tar.gz
rmdir /s /q dxvk-tmp
if not exist data\vulkan.dll (
  echo Failed to extract DXVK & exit /b 1
)

rem NVIDIA DLSS
del data\plugins\nvngx_dlss.dll 2>nul
curl -fsSL -o data\plugins\nvngx_dlss.dll https://raw.githubusercontent.com/NVIDIA/DLSS/%DLSS_VERSION%/lib/Windows_x86_64/rel/nvngx_dlss.dll || (
  del data\plugins\nvngx_dlss.dll 2>nul
  echo Failed to download DLSS & exit /b 1
)

rem AMD FidelityFX: the loader and the upscaler it loads from its own folder
for %%F in (amd_fidelityfx_loader_dx12.dll amd_fidelityfx_upscaler_dx12.dll) do (
  del "data\plugins\%%F" 2>nul
  curl -fsSL -o "data\plugins\%%F" https://raw.githubusercontent.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/%FIDELITYFX_VERSION%/Kits/FidelityFX/signedbin/%%F || (
    del "data\plugins\%%F" 2>nul
    echo Failed to download %%F & exit /b 1
  )
)

exit /b 0
