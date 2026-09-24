param([Parameter(Mandatory=$true)][string]$VisualStudioRoot)
$ErrorActionPreference='Stop'
$setup=Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
Push-Location -LiteralPath $PSScriptRoot
try {
    & $env:ComSpec /d /c ('"'+$setup+'" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 light_geometry_tests.cpp /Fe:light_geometry_tests-x86.exe /Fo:light_geometry_tests-x86.obj && light_geometry_tests-x86.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Light geometry tests failed.' }
} finally { Pop-Location }
