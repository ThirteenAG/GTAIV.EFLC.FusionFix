# Compiles the plugin's HLSL shaders with fxc before the build (premake5.lua). The compiled shaders are committed, with
# source/resources/shaders.lock: per shader, a hash of its source, the files it includes, the fxc arguments and fxc
# itself, and a hash of the compiled file. A shader compiles only when that input hash changed or the compiled file is
# missing or differs, so a fresh clone compiles nothing; the ones that need it compile in parallel and a failed compile
# fails the build with fxc's messages. After changing a shader, build and commit its compiled file with shaders.lock.
# Shaders compiled into a submodule are not committed: their hashes go to -LocalLock instead.
# -Force compiles every shader; -Verify then fails when a committed file differs from git (CI).
param(
    [Parameter(Mandatory = $true)][string]$LocalLock,
    [switch]$Force,
    [switch]$Verify
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$LocalLock = [IO.Path]::GetFullPath((Join-Path (Get-Location) $LocalLock))
$lock = Join-Path $root "source\resources\shaders.lock"
$fxcDir = Join-Path $root "source\dxsdk\lib\x86"
$fxc = Join-Path $fxcDir "fxc.exe"

# Src: the source, relative to the repository root; a wildcard compiles every match, {0} in the outputs is the matched
# file's name without extension. T: profile, E: entry point, Fo: compiled file, Fc: assembly listing (only for
# convenience, not committed), Flags: more fxc options, Local: not committed.
$shaders = @(
    # ConsoleGamma
    @{ Src = "shaders/external/gamma/fx/ConsoleGamma.fx"; T = "fx_2_0"; Fo = "source/resources/shaders/win32_30/ConsoleGamma.fxo"; Fc = "shaders/external/gamma/asm/ConsoleGamma.asm" }
    @{ Src = "shaders/external/gamma/fx/ConsoleGamma_Dither.fx"; T = "fx_2_0"; Fo = "source/resources/shaders/win32_30/ConsoleGamma_Dither.fxo"; Fc = "shaders/external/gamma/asm/ConsoleGamma_Dither.asm" }

    # Snow
    @{ Src = "source/resources/snow/*.ps"; T = "ps_3_0"; E = "main"; Fo = "source/resources/snow/{0}ps.pso" }
    @{ Src = "source/resources/snow/*.vs"; T = "vs_3_0"; E = "main"; Fo = "source/resources/snow/{0}vs.vso" }

    # Sun shafts, FXAA, SMAA
    @{ Src = "source/resources/SunShafts_PS.hlsl"; T = "ps_3_0"; E = "SSDraw"; Fo = "source/resources/SSDraw_PS.pso" }
    @{ Src = "source/resources/SunShafts_PS.hlsl"; T = "ps_3_0"; E = "SSPrepass"; Fo = "source/resources/SSPrepass_PS.pso" }
    @{ Src = "source/resources/SunShafts_PS.hlsl"; T = "ps_3_0"; E = "SSAdd"; Fo = "source/resources/SSAdd_PS.pso" }
    @{ Src = "source/resources/FXAA3_11.h"; T = "ps_3_0"; E = "ApplyFXAA"; Fo = "source/resources/FxaaPS.pso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "ps_3_0"; E = "DX9_SMAALumaEdgeDetectionPS"; Fo = "source/resources/SMAA_EdgeDetection.pso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "ps_3_0"; E = "DX9_SMAABlendingWeightCalculationPS"; Fo = "source/resources/SMAA_BlendingWeightsCalculation.pso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "ps_3_0"; E = "DX9_SMAANeighborhoodBlendingPS"; Fo = "source/resources/SMAA_NeighborhoodBlending.pso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "vs_3_0"; E = "DX9_SMAAEdgeDetectionVS"; Fo = "source/resources/SMAA_EdgeDetectionVS.vso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "vs_3_0"; E = "DX9_SMAABlendingWeightCalculationVS"; Fo = "source/resources/SMAA_BlendingWeightsCalculationVS.vso" }
    @{ Src = "source/resources/SMAA.hlsl"; T = "vs_3_0"; E = "DX9_SMAANeighborhoodBlendingVS"; Fo = "source/resources/SMAA_NeighborhoodBlendingVS.vso" }

    # Temporal anti-aliasing
    @{ Src = "source/resources/temporal/Temporal.hlsl"; T = "vs_3_0"; E = "VS_VelocityRigid"; Fo = "source/resources/temporal/VelocityRigid.vso" }
    @{ Src = "source/resources/temporal/Temporal.hlsl"; T = "vs_3_0"; E = "VS_VelocitySkinned"; Fo = "source/resources/temporal/VelocitySkinned.vso" }
    @{ Src = "source/resources/temporal/Temporal.hlsl"; T = "ps_3_0"; E = "PS_Velocity"; Fo = "source/resources/temporal/Velocity.pso" }
    @{ Src = "source/resources/temporal/Temporal.hlsl"; T = "vs_3_0"; E = "VS_BoneWrite"; Fo = "source/resources/temporal/BoneWrite.vso" }
    @{ Src = "source/resources/temporal/Temporal.hlsl"; T = "ps_3_0"; E = "PS_BoneWrite"; Fo = "source/resources/temporal/BoneWrite.pso" }

    # The shared module of FusionFixUtils (aa/AntiAliasing.hpp): its pixel shaders, embedded by aa/AntiAliasing.rc
    @{ Src = "external/FusionFixUtils/aa/shaders/*.hlsl"; T = "ps_3_0"; E = "main"; Fo = "external/FusionFixUtils/aa/shaders/{0}.pso"; Local = $true }

    # HDR output
    @{ Src = "source/resources/hdr/HDR.hlsl"; T = "ps_3_0"; E = "PS_HDROutput"; Fo = "source/resources/hdr/HDROutput.pso" }
    @{ Src = "source/resources/hdr/HDR.hlsl"; T = "ps_3_0"; E = "PS_HDRCalibration"; Fo = "source/resources/hdr/HDRCalibration.pso" }
)

$sha = [Security.Cryptography.SHA256]::Create()
$latin1 = [Text.Encoding]::GetEncoding(28591)
function Get-Hash([byte[]]$bytes) { return [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant() }
function Get-RelativePath([string]$path) { return $path.Substring($root.Length + 1).Replace('\', '/') }

# A source and everything it includes, recursively, as full paths; an include is looked up next to the file that
# includes it, then next to the shader's source
function Get-Inputs([string]$file, [string]$srcDir, [hashtable]$seen)
{
    $file = [IO.Path]::GetFullPath($file)
    if ($seen.ContainsKey($file)) { return }
    $seen[$file] = $true
    foreach ($m in [regex]::Matches([IO.File]::ReadAllText($file), '(?m)^[ \t]*#[ \t]*include[ \t]*["<]([^">]+)[">]'))
    {
        foreach ($dir in @([IO.Path]::GetDirectoryName($file), $srcDir))
        {
            $inc = Join-Path $dir $m.Groups[1].Value
            if ([IO.File]::Exists($inc)) { Get-Inputs $inc $srcDir $seen; break }
        }
    }
}

# Line endings don't count: checkouts with either compile nothing
$contentHashes = @{}
function Get-ContentHash([string]$file)
{
    if (-not $contentHashes.ContainsKey($file))
    {
        $text = $latin1.GetString([IO.File]::ReadAllBytes($file)).Replace("`r`n", "`n")
        $contentHashes[$file] = Get-Hash $latin1.GetBytes($text)
    }
    return $contentHashes[$file]
}

# Compiled file -> "<inputs hash> <compiled file hash>"
function Read-Lock([string]$path)
{
    $entries = @{}
    if (Test-Path $path)
    {
        foreach ($line in [IO.File]::ReadAllLines($path))
        {
            $f = @($line.Trim() -split '\s+')
            if ($f.Count -eq 3 -and -not $f[0].StartsWith("#")) { $entries[$f[0]] = "$($f[1]) $($f[2])" }
        }
    }
    return $entries
}

# Rewritten only when an entry changed
function Write-Lock([string]$path, [hashtable]$entries, [string]$header)
{
    $lines = @($header) + @($entries.Keys | Sort-Object | ForEach-Object { "$_ $($entries[$_])" })
    $text = ($lines -join "`r`n") + "`r`n"
    if ((Test-Path $path) -and ([IO.File]::ReadAllText($path).Replace("`r`n", "`n") -eq $text.Replace("`r`n", "`n"))) { return }
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $path) | Out-Null
    [IO.File]::WriteAllText($path, $text)
}

$fxcHash = Get-Hash $latin1.GetBytes((@("fxc.exe", "D3DCompiler_43.dll", "D3DX9_43.dll") | ForEach-Object { Get-Hash ([IO.File]::ReadAllBytes((Join-Path $fxcDir $_))) }) -join " ")
$locks = @{ $false = (Read-Lock $lock); $true = (Read-Lock $LocalLock) }
$newLocks = @{ $false = @{}; $true = @{} }
$jobs = @()
foreach ($s in $shaders)
{
    $local = [bool]$s.Local
    $dir = Split-Path -Parent $s.Src
    $sources = @(Get-ChildItem -Path (Join-Path $root $dir) -Filter (Split-Path -Leaf $s.Src) -File |
        Where-Object { $_.Extension -eq [IO.Path]::GetExtension($s.Src) } | Sort-Object Name)
    if ($sources.Count -eq 0) { throw "compileshaders: no shader matches $($s.Src)" }
    foreach ($src in $sources)
    {
        $name = $src.BaseName
        $fo = $s.Fo -f $name
        $fc = if ($s.Fc) { $s.Fc -f $name } else { $null }
        $arguments = "/T $($s.T) /nologo"
        if ($s.E) { $arguments += " /E $($s.E)" }
        if ($s.Flags) { $arguments += " $($s.Flags)" }
        $arguments += " /Fo `"$fo`""
        if ($fc) { $arguments += " /Fc `"$fc`"" }

        $seen = @{}
        Get-Inputs $src.FullName $src.DirectoryName $seen
        $key = "$fxcHash`n$arguments `"$(Get-RelativePath $src.FullName)`"`n"
        foreach ($in in ($seen.Keys | Sort-Object)) { $key += "$(Get-RelativePath $in) $(Get-ContentHash $in)`n" }
        $inputHash = Get-Hash $latin1.GetBytes($key)

        $old = $locks[$local][$fo]
        $foPath = Join-Path $root $fo
        if (-not $Force -and $old -and [IO.File]::Exists($foPath) -and $old -eq "$inputHash $(Get-Hash ([IO.File]::ReadAllBytes($foPath)))")
        {
            $newLocks[$local][$fo] = $old
            continue
        }
        # fxc gets the full path of the source, for its messages
        $jobs += @{ Arguments = "$arguments `"$($src.FullName)`""; Outputs = @($fo, $fc); Fo = $fo; InputHash = $inputHash; Local = $local }
    }
}

$failed = $false
foreach ($o in ($jobs | ForEach-Object { $_.Outputs } | Where-Object { $_ })) { New-Item -ItemType Directory -Force -Path (Split-Path -Parent (Join-Path $root $o)) | Out-Null }
$running = [Collections.Generic.List[object]]::new()
$queue = [Collections.Queue]::new(@($jobs))
while ($queue.Count -gt 0 -or $running.Count -gt 0)
{
    while ($queue.Count -gt 0 -and $running.Count -lt [Environment]::ProcessorCount)
    {
        $job = $queue.Dequeue()
        $info = New-Object Diagnostics.ProcessStartInfo $fxc, $job.Arguments
        $info.WorkingDirectory = $root
        $info.UseShellExecute = $false
        $info.RedirectStandardOutput = $true
        $info.RedirectStandardError = $true
        $p = [Diagnostics.Process]::Start($info)
        $job.Process = $p
        $job.Out = $p.StandardOutput.ReadToEndAsync()
        $job.Err = $p.StandardError.ReadToEndAsync()
        $running.Add($job)
    }
    $done = @($running | Where-Object { $_.Process.HasExited })
    if ($done.Count -eq 0) { Start-Sleep -Milliseconds 10; continue }
    foreach ($job in $done)
    {
        $job.Process.WaitForExit()
        [void]$running.Remove($job)
        $err = $job.Err.Result.Trim()
        if ($job.Process.ExitCode -ne 0)
        {
            $failed = $true
            [Console]::Out.WriteLine("fxc $($job.Arguments)")
            if ($job.Out.Result.Trim()) { [Console]::Out.WriteLine($job.Out.Result.Trim()) }
            if ($err) { [Console]::Out.WriteLine($err) } else { [Console]::Out.WriteLine("error : fxc failed with exit code $($job.Process.ExitCode)") }
        }
        else
        {
            # Warnings
            if ($err) { [Console]::Out.WriteLine($err) }
            $newLocks[$job.Local][$job.Fo] = "$($job.InputHash) $(Get-Hash ([IO.File]::ReadAllBytes((Join-Path $root $job.Fo))))"
        }
    }
}

Write-Lock $lock $newLocks[$false] "# Generated by tools/compileshaders.ps1: compiled shader, hash of its inputs, hash of the compiled file"
Write-Lock $LocalLock $newLocks[$true] "# Generated by tools/compileshaders.ps1"
if ($failed) { exit 1 }
if ($jobs.Count -gt 0) { [Console]::Out.WriteLine("compileshaders: compiled $($jobs.Count) shader(s)") }

if ($Verify)
{
    $committed = @(Get-RelativePath $lock) + @($newLocks[$false].Keys | Sort-Object)
    $changed = @(& git -C $root status --porcelain --untracked-files=all -- $committed)
    if ($LASTEXITCODE -ne 0) { throw "compileshaders: git status failed" }
    if ($changed.Count -gt 0)
    {
        $changed | ForEach-Object { [Console]::Out.WriteLine("  $_") }
        $message = "the committed compiled shaders don't match their sources: run the build and commit the updated shaders (source/resources/shaders.lock and the files listed above)"
        if ($env:GITHUB_ACTIONS) { [Console]::Out.WriteLine("::error::$message") } else { [Console]::Out.WriteLine("error : $message") }
        exit 1
    }
}
exit 0
