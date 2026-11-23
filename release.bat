call tools\EmbedPDB\EmbedPDB.exe bin\GTAIV.EFLC.FusionFix.asi
call tools\EmbedPDB\EmbedPDB.exe bin\d3d9.dll

powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" -SearchPaths ".\bin\GTAIV.EFLC.FusionFix.asi .\bin\d3d9.dll"

if not exist data\plugins mkdir data\plugins
copy bin\GTAIV.EFLC.FusionFix.asi data\plugins\GTAIV.EFLC.FusionFix.asi || exit /b 1
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
-x!FusionFixHub.exe ^
-xr!*\.gitkeep

exit /b 0
