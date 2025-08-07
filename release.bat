copy bin\GTAIV.EFLC.FusionFix.asi data\plugins\GTAIV.EFLC.FusionFix.asi
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

copy /Y bin\Release\GTAIV.EFLC.FusionFixInstaller.exe GTAIV.EFLC.FusionFixWebInstaller.exe
start /B /WAIT GTAIV.EFLC.FusionFixWebInstaller.exe GTAIV.EFLC.FusionFix.zip
:RETRY
move /Y GTAIV.EFLC.FusionFixWebInstaller_with_GTAIV.EFLC.FusionFix.exe GTAIV.EFLC.FusionFixOfflineInstaller.exe
if ERRORLEVEL 1 (
    echo File locked, retrying...
    timeout /t 2 > nul
    goto RETRY
)
