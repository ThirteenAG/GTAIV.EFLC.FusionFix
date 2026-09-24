param(
    [string]$GameExecutable = 'D:\SteamLibrary\steamapps\common\Grand Theft Auto IV\GTAIV\GTAIV.exe',
    [Parameter(Mandatory=$true)][string]$VisualStudioRoot
)
$ErrorActionPreference = 'Stop'
$setup = Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
if (-not (Test-Path -LiteralPath $setup)) { throw 'Visual C++ build environment was not found.' }
if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) { throw 'GTA IV executable was not found.' }
Push-Location -LiteralPath $PSScriptRoot
try {
    $compilerCommand = '"' + $setup + '" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 adapter_guard_tests.cpp /Fe:adapter_guard_tests-x86.exe /Fo:adapter_guard_tests-x86.obj'
    & $env:ComSpec /d /c $compilerCommand
    if ($LASTEXITCODE -ne 0) { throw ('Adapter guard tests failed to compile: ' + $LASTEXITCODE) }
    & '.\adapter_guard_tests-x86.exe' $GameExecutable
    if ($LASTEXITCODE -ne 0) { throw ('Adapter guard tests failed: ' + $LASTEXITCODE) }
}
finally { Pop-Location }
