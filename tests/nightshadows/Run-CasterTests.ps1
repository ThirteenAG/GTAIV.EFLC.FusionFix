param([Parameter(Mandatory=$true)][string]$VisualStudioRoot,
      [Parameter(Mandatory=$true)][string]$GameExe)
$ErrorActionPreference='Stop'
$setup=Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
Push-Location -LiteralPath $PSScriptRoot
try {
    & $env:ComSpec /d /c ('"'+$setup+'" >nul && cl /nologo /std:c++17 /W4 /WX /EHsc /O2 caster_policy_tests.cpp /Fe:caster_policy_tests-x86.exe /Fo:caster_policy_tests-x86.obj && caster_policy_tests-x86.exe "'+$GameExe+'"')
    if ($LASTEXITCODE -ne 0) { throw 'Caster tests failed.' }
} finally { Pop-Location }
