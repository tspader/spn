param(
  [Parameter(Mandatory=$true)][string]$Lane,
  [Parameter(Mandatory=$true)][string]$Filter,
  [Parameter(Mandatory=$true)][string]$Probes
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest

$Work = 'C:\spn'
$Src  = "$env:USERPROFILE\spn-src.tar.gz"
$Bins = "$env:USERPROFILE\spn-bins.tar.gz"
$Exe  = "$Work\build\x86_64-windows-gnu\mingw\test\integration.exe"

Write-Host "== wintest: lane=$Lane filter=$Filter probes=$Probes =="

$excl = @($Work, $env:TEMP, "$env:LOCALAPPDATA\spn", "$env:APPDATA\spn", "$env:USERPROFILE\.cache")
foreach ($p in $excl) {
  try { Add-MpPreference -ExclusionPath $p -ErrorAction Stop }
  catch { Write-Host "  (skipped exclusion for ${p}: $($_.Exception.Message))" }
}

Write-Host "== unpacking repo + binaries =="
if (Test-Path $Work) {
  & cmd /c "rmdir /s /q `"$Work`"" 2>$null
  if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
}
New-Item -ItemType Directory -Force -Path $Work | Out-Null
New-Item -ItemType Directory -Force -Path $Probes | Out-Null
& tar.exe -xzf $Src -C $Work
if ($LASTEXITCODE -ne 0) { throw "source extract failed ($LASTEXITCODE)" }
& tar.exe -xzf $Bins -C $Work
if ($LASTEXITCODE -ne 0) { throw "binaries extract failed ($LASTEXITCODE)" }
if (-not (Test-Path $Exe)) { throw "integration exe missing at $Exe" }

Push-Location $Work
& git init -q .
& git config user.email "winvm@localhost"
& git config user.name  "winvm"
& git add -A
& git -c core.safecrlf=false commit -q -m "winvm snapshot" | Out-Null
Pop-Location
if (-not (Test-Path "$Work\.git")) { throw "git init failed in $Work" }

$env:SPN_TEST_TOOLCHAIN = $Lane
$env:SPN_BARE_PROBES = $Probes
Set-Location $Work
Write-Host "== integration --filter $Filter =="
& $Exe --filter $Filter
$rc = $LASTEXITCODE
Write-Host "== INTEGRATION EXIT $rc =="
exit $rc
