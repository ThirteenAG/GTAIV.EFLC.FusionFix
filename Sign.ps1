param(
    [string]$SearchPathsRaw = ".\*.asi",
    [int]$MaxParallel = 8
)

$SearchPaths = $SearchPathsRaw -split '[, ]+' | Where-Object { $_ -ne '' }

Write-Host "=== Parallel Code Signing Script ===" -ForegroundColor Cyan
Write-Host "Current folder: $PWD" -ForegroundColor Gray

if (-not ($env:CODE_SIGNING_PFX -and $env:CODE_SIGNING_PASSWORD)) {
    Write-Host "No CODE_SIGNING_PFX found." -ForegroundColor Red
    exit 0
}

# Decode PFX
$cleanBase64 = $env:CODE_SIGNING_PFX -replace "\s+", ""
$pfxBytes = [Convert]::FromBase64String($cleanBase64)
$password = $env:CODE_SIGNING_PASSWORD.Trim()

$tempPfx = Join-Path $env:TEMP "signing_$(Get-Random).pfx"
[IO.File]::WriteAllBytes($tempPfx, $pfxBytes)

$signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "signtool.exe" -ErrorAction SilentlyContinue | Select-Object -Last 1).FullName

if (-not $signtool) {
    Write-Host "ERROR: signtool.exe not found!" -ForegroundColor Red
    Remove-Item $tempPfx -Force
    exit 0
}

$filesToSign = @()

foreach ($pattern in $SearchPaths) {
    $clean = $pattern.Trim()
    if ([string]::IsNullOrWhiteSpace($clean)) { continue }

    Write-Host "Searching: $clean" -ForegroundColor Gray

    if (Test-Path $clean -PathType Leaf) {
        # Specific file
        $filesToSign += Get-Item $clean
    } 
    elseif ($clean -like "*\*") {
        # Wildcard pattern → recursive search
        $filesToSign += Get-ChildItem -Path "." -Recurse -Include (Split-Path $clean -Leaf) -ErrorAction SilentlyContinue
    } 
    else {
        # Folder
        $filesToSign += Get-ChildItem -Path $clean -Recurse -Include "*.asi","*.dll" -ErrorAction SilentlyContinue
    }
}

$filesToSign = $filesToSign | Where-Object { 
    $_.FullName -notlike "*\Archives\*" -and 
    $_.FullName -notlike "*\.git\*" -and 
    $_.FullName -notlike "*\obj\*" 
} | Select-Object -Unique

if ($filesToSign.Count -eq 0) {
    Write-Host "No files found to sign." -ForegroundColor Yellow
    Remove-Item $tempPfx -Force
    exit 0
}

Write-Host "Found $($filesToSign.Count) files to sign" -ForegroundColor Cyan

$jobs = @()
foreach ($file in $filesToSign) {
    $job = Start-Job -ScriptBlock {
        param($tool, $pfx, $pass, $fpath)
        & $tool sign /fd SHA256 /f $pfx /p $pass /tr http://timestamp.digicert.com /td SHA256 $fpath | Out-Null
    } -ArgumentList $signtool, $tempPfx, $password, $file.FullName

    $jobs += $job

    while (@(Get-Job -State Running).Count -ge $MaxParallel) {
        Start-Sleep -Milliseconds 200
    }
}

Wait-Job $jobs | Out-Null

$successCount = 0
foreach ($job in $jobs) {
    if ($job.State -eq "Completed") {
        $successCount = $successCount + 1
    }
    Remove-Job $job
}

Remove-Item $tempPfx -Force

Write-Host "Signing completed successfully!"
Write-Host "($successCount files signed)"