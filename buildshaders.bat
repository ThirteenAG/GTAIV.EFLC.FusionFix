@echo on
set shaders_path=%cd%/shaders
set win32_30=%cd%/data/update/common/shaders/win32_30

rem The shaders are built from the -temp copy until the submodule has the matching changes
set fusion_shaders=%shaders_path%/GTAIV.EFLC.FusionShaders-temp

cd tools/RageShaderEditor
for /R "%fusion_shaders%" %%a in (*.xml) do .\RageShaderEditor.exe "%%a"
for /R "%shaders_path%/external" %%a in (*.xml) do .\RageShaderEditor.exe "%%a"
cd ../..

for /R "%fusion_shaders%" %%i in (*.fxc) do echo D | xcopy "%%i" "%win32_30%" /K /H /Y
for /R "%shaders_path%/external" %%i in (*.fxc) do echo D | xcopy "%%i" "%win32_30%" /K /H /Y

echo D | xcopy "%fusion_shaders%/resources" "%cd%/data/update" /K /H /Y /S

del /F /Q "%cd%\data\update\common\shaders\preload.list" 2>nul