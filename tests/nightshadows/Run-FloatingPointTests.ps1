param([Parameter(Mandatory=$true)][string]$VisualStudioRoot)
$ErrorActionPreference = 'Stop'
$setup = Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
if (-not (Test-Path -LiteralPath $setup)) { throw 'Visual C++ build environment was not found.' }
Push-Location -LiteralPath $PSScriptRoot
try {
    $compilerCommand = '"' + $setup + '" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 /arch:SSE2 /fp:precise /FAs /Fafp_state_tests-x86.asm fp_state_tests.cpp /Fe:fp_state_tests-x86.exe /Fo:fp_state_tests-x86.obj && fp_state_tests-x86.exe'
    & $env:ComSpec /d /c $compilerCommand
    if ($LASTEXITCODE -ne 0) { throw ('Floating point tests failed: ' + $LASTEXITCODE) }
}
finally { Pop-Location }
