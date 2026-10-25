call tools\EmbedPDB\EmbedPDB.exe bin\GTAIV.EFLC.FusionFix.asi
call tools\EmbedPDB\EmbedPDB.exe bin\d3d9.dll

powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" -SearchPaths ".\bin\GTAIV.EFLC.FusionFix.asi .\bin\d3d9.dll"

if not exist data\plugins mkdir data\plugins
copy bin\GTAIV.EFLC.FusionFix.asi data\plugins\GTAIV.EFLC.FusionFix.asi || exit /b 1
rem FusionFixHub.exe ships with the fix, at the game's root (updates, settings editor, DLAA/FSR). HUB_EXE picks it;
rem the workflow downloads the signed release next to this script, a local release takes the hub's own build.
if not defined HUB_EXE if exist FusionFixHub.exe set "HUB_EXE=FusionFixHub.exe"
if not defined HUB_EXE set "HUB_EXE=%~dp0..\FusionFixHub-App\bin\Release\FusionFixHub.exe"
if exist "%HUB_EXE%" (
    copy /y "%HUB_EXE%" data\FusionFixHub.exe
) else if "%GITHUB_ACTIONS%"=="true" (
    echo WARNING: no FusionFixHub.exe ^(HUB_RELEASE_TOKEN not set^): the package is built without the hub
) else (
    echo ERROR: FusionFixHub.exe not found at "%HUB_EXE%". Build FusionFixHub-App ^(Release^) or set HUB_EXE to the signed FusionFixHub.exe.
    exit /b 1
)
rem d3d9.dll is only a pass-through to the system's d3d9.dll (source/d3d9)
copy bin\d3d9.dll data\d3d9.dll

call buildimg.bat
call buildwtd.bat
call buildshaders.bat
call buildgxt.bat

7z a "GTAIV.EFLC.FusionFix.zip" ".\data\*" ^
-x!update\GTAIV.EFLC.FusionFix\GTAIV.EFLC.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\GTAIV.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\TLAD.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\TBOGT.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\FusionTrees ^
-x!update\GTAIV.EFLC.FusionFix\FusionLights ^
-x!update\GTAIV.EFLC.FusionFix\cdimagemake.ims ^
-xr!*\.gitkeep

exit /b 0
