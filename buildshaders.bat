@echo on
set shaders_path=%cd%/shaders
set win32_30=%cd%/data/update/common/shaders/win32_30

cd tools/RageShaderEditor
for /R "%shaders_path%" %%a in (*.xml) do (
    RageShaderEditor.exe "%%a"
)
cd ../..

for /R "%shaders_path%" %%i in (*.fxc) do (
echo D | xcopy "%%i" "%win32_30%" /K /H /Y
)

echo D | xcopy "%cd%/shaders/GTAIV.EFLC.FusionShaders/resources" "%cd%/data/update" /K /H /Y /S
