param(
    [ValidateSet('x86', 'x64')][string]$Architecture = 'x86',
    [Parameter(Mandatory=$true)][string]$VisualStudioRoot
)
$ErrorActionPreference = 'Stop'
$setupName = if ($Architecture -eq 'x86') { 'vcvars32.bat' } else { 'vcvars64.bat' }
$setup = Join-Path $VisualStudioRoot ('VC\Auxiliary\Build\' + $setupName)
if (-not (Test-Path -LiteralPath $setup)) { throw 'Visual C++ build environment was not found.' }
Push-Location -LiteralPath $PSScriptRoot
try {
    $compilerCommand = '"' + $setup + '" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 budget_tests.cpp /Fe:budget_tests-' + $Architecture + '.exe /Fo:budget_tests-' + $Architecture + '.obj && budget_tests-' + $Architecture + '.exe'
    & $env:ComSpec /d /c $compilerCommand
    if ($LASTEXITCODE -ne 0) { throw ('Budget tests failed: ' + $LASTEXITCODE) }
}
finally { Pop-Location }
