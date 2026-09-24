param(
    [string]$GameExecutable = 'D:\SteamLibrary\steamapps\common\Grand Theft Auto IV\GTAIV\GTAIV.exe',
    [Parameter(Mandatory=$true)][string]$VisualStudioRoot
)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $GameExecutable -PathType Leaf)) { throw 'GTA IV executable was not found.' }
Push-Location -LiteralPath $PSScriptRoot
try {
    foreach ($architecture in @('x86', 'x64')) {
        $setupName = if ($architecture -eq 'x86') { 'vcvars32.bat' } else { 'vcvars64.bat' }
        $setup = Join-Path $VisualStudioRoot ('VC\Auxiliary\Build\' + $setupName)
        if (-not (Test-Path -LiteralPath $setup)) { throw ('Visual C++ environment was not found: ' + $setup) }
        $program = 'allocation_adapter_tests-' + $architecture + '.exe'
        $object = 'allocation_adapter_tests-' + $architecture + '.obj'
        $compilerCommand = '"' + $setup + '" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 allocation_adapter_tests.cpp /Fe:' + $program + ' /Fo:' + $object
        & $env:ComSpec /d /c $compilerCommand
        if ($LASTEXITCODE -ne 0) { throw ('Allocation adapter tests failed to compile for ' + $architecture + ': ' + $LASTEXITCODE) }
        & (Join-Path $PSScriptRoot $program) $GameExecutable
        if ($LASTEXITCODE -ne 0) { throw ('Allocation adapter tests failed for ' + $architecture + ': ' + $LASTEXITCODE) }
    }
}
finally { Pop-Location }
