@echo off
set shaders_path=%cd%/shaders
set win32_30=%cd%/data/update/common/shaders/win32_30
set win32_30_atidx10=%cd%/data/update/common/shaders/win32_30_atidx10
set win32_30_nv8=%cd%/data/update/common/shaders/win32_30_nv8

cd tools/RageShaderEditor
for /R "%shaders_path%" %%a in (*.xml) do (
    RageShaderEditor.exe "%%a"
)
cd ../..

FOR /F "delims=" %%i IN ('where /r shaders\\GTA-IV-Shaders mirror.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_cutout_fence.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection deferred_lighting.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_cubemap_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_cutout_fence.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_default.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_emissive.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_emissivenight.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_emissivestrong.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal_cubemap_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal_spec.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal_spec_cubemap_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_normal_spec_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_parallax.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_parallax_specmap.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_parallax_steep.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_rmptfx_gpurender.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_rmptfx_raindrops.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_spec.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_spec_reflect.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_trees.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_vehicle_paint1.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_vehicle_paint2.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection gta_vehicle_paint3.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection rage_billboard_nobump.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection rage_default.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection rage_postfx.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection water.fxc') DO call :copyShader "%%i"
FOR /F "delims=" %%i IN ('where /r shaders\\GTAIV.ShaderFixesCollection waterTex.fxc') DO call :copyShader "%%i"
goto :eof

:copyShader
echo D | xcopy "%~1" "%win32_30%" /K /H /Y
echo D | xcopy "%~1" "%win32_30_atidx10%" /K /H /Y
echo D | xcopy "%~1" "%win32_30_nv8%" /K /H /Y
EXIT /B %ERRORLEVEL%
