param([Parameter(Mandatory=$true)][string]$VisualStudioRoot,
      [Parameter(Mandatory=$true)][string]$GameExe)
$ErrorActionPreference='Stop'
$setup=Join-Path $VisualStudioRoot 'VC\Auxiliary\Build\vcvars32.bat'
Push-Location -LiteralPath $PSScriptRoot
try {
    # Link the same library objects used by the successful production build.
    $objects='allocator','easy','inline_hook','mid_hook','os.windows','utility','vmt_hook','Zydis'
    $links=($objects | ForEach-Object { '"..\..\build\obj\Release\GTAIV.EFLC.FusionFix\'+$_+'.obj"' }) -join ' '
    $compile='"'+$setup+'" >nul && cl /nologo /std:c++latest /W4 /WX /EHsc /O2 /MT /I"..\..\external\injector\safetyhook\include" hook_order_tests.cpp '+$links+' /Fe:hook_order_tests-x86.exe /Fo:hook_order_tests-x86.obj && hook_order_tests-x86.exe "'+$GameExe+'"'
    & $env:ComSpec /d /c $compile
    if($LASTEXITCODE -ne 0) { throw 'Hook order fixture failed.' }
} finally { Pop-Location }
