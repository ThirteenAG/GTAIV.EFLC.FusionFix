copy bin\GTAIV.EFLC.FusionFix.asi data\plugins\GTAIV.EFLC.FusionFix.asi

call buildimg.bat
call buildwtd.bat
call buildshaders.bat

7z a "GTAIV.EFLC.FusionFix.zip" ".\data\*" ^
-x!update\GTAIV.EFLC.FusionFix\GTAIV.EFLC.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\GTAIV.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\TLAD.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\TBOGT.FusionFix ^
-x!update\GTAIV.EFLC.FusionFix\FusionTrees ^
-x!update\GTAIV.EFLC.FusionFix\FusionLights ^
-x!update\GTAIV.EFLC.FusionFix\FusionLightsShadowcast ^
-x!update\GTAIV.EFLC.FusionFix\cdimagemake.ims ^
-xr!*\.gitkeep
