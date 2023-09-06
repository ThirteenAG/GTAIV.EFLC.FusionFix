@echo on
set shaders_path=%cd%/shaders
set win32_30=%cd%/data/update/common/shaders/win32_30
set win32_30_atidx10=%cd%/data/update/common/shaders/win32_30_atidx10
set win32_30_nv8=%cd%/data/update/common/shaders/win32_30_nv8

cd tools/RageShaderEditor
for /R "%shaders_path%" %%a in (*.xml) do (
    RageShaderEditor.exe "%%a"
)
cd ../..

for /R "%shaders_path%" %%i in (*.fxc) do (
echo D | xcopy "%%i" "%win32_30%" /K /H /Y
echo D | xcopy "%%i" "%win32_30_atidx10%" /K /H /Y
echo D | xcopy "%%i" "%win32_30_nv8%" /K /H /Y
)

echo D | xcopy "%cd%/shaders/GTAIV.ShaderFixesCollection/other_assets/Improved Stipple" "%cd%/data/update" /K /H /Y /S
echo D | xcopy "%cd%/shaders/GTAIV.ShaderFixesCollection/other_assets/Opaque Wires" "%cd%/data/update" /K /H /Y /S
echo D | xcopy "%cd%/shaders/GTAIV.ShaderFixesCollection/other_assets/gta_trees_new dependencies" "%cd%/data/update" /K /H /Y /S