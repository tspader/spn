param(
  [string]$Lane = '',
  [string]$Filter = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Work = 'C:\spn'
$Src  = "$env:USERPROFILE\spn-src.tar.gz"
$Bins = "$env:USERPROFILE\spn-bins.tar.gz"
$Exe  = "$Work\build\x86_64-windows-gnu\mingw\test\integration.exe"

Write-Host "== wintest: lane=$Lane filter=$Filter =="

$excl = @($Work, $env:TEMP, "$env:LOCALAPPDATA\spn", "$env:APPDATA\spn", "$env:USERPROFILE\.cache")
foreach ($p in $excl) {
  try { Add-MpPreference -ExclusionPath $p -ErrorAction Stop }
  catch { Write-Host "  (skipped exclusion for ${p}: $($_.Exception.Message))" }
}

Write-Host "== unpacking repo + binaries =="
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Force -Path $Work | Out-Null
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

if ($Lane -eq 'msvc') {
  Write-Host "== devenv (vcvarsall) =="
  & "$Work\tools\devenv.ps1"
}

if ($Lane) { $env:SPN_TEST_TOOLCHAIN = $Lane }
Set-Location $Work
if ($Filter) {
  Write-Host "== integration --filter $Filter =="
  & $Exe --filter $Filter
} else {
  Write-Host "== integration (all cases) =="
  & $Exe
}
$rc = $LASTEXITCODE
Write-Host "== INTEGRATION EXIT $rc =="
exit $rc
