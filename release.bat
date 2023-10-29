copy bin\GTAIV.EFLC.FusionFix.asi data\plugins\GTAIV.EFLC.FusionFix.asi

call buildimg.bat
call buildshaders.bat

7z a "GTAIV.EFLC.FusionFix.zip" ".\data\*" ^
-xr!update\GTAIV.EFLC.FusionFix\GTAIV.EFLC.FusionFix ^
-xr!update\GTAIV.EFLC.FusionFix\GTAIV.FusionFix ^
-xr!update\GTAIV.EFLC.FusionFix\TLAD.FusionFix ^
-xr!update\GTAIV.EFLC.FusionFix\TBOGT.FusionFix ^
-xr!update\GTAIV.EFLC.FusionFix\FusionTrees ^
-xr!update\GTAIV.EFLC.FusionFix\FusionLights ^
-x!update\GTAIV.EFLC.FusionFix\cdimagemake.ims ^
-x!*.gitkeep
