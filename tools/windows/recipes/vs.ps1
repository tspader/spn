# vs.ps1 — install Visual Studio Build Tools with the C++ (MSVC) workload.
# Arg: the VS year, "2022" or "2026".
param([string]$Year = '2022')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$channels = @{
  '2022' = 'https://aka.ms/vs/17/release/vs_BuildTools.exe'
  '2026' = 'https://aka.ms/vs/18/release/vs_BuildTools.exe'
}
$url = $channels[$Year]
if (-not $url) { throw "unknown VS year: $Year" }

Write-Host "== Visual Studio $Year Build Tools =="
$exe = Join-Path $env:TEMP "vs_BuildTools_$Year.exe"
Invoke-WebRequest -Uri $url -OutFile $exe -UseBasicParsing

$vsArgs = @(
  '--quiet', '--wait', '--norestart', '--nocache',
  '--add', 'Microsoft.VisualStudio.Workload.VCTools',
  '--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
  '--add', 'Microsoft.VisualStudio.Component.Windows11SDK.22621',
  '--includeRecommended'
)
$proc = Start-Process -FilePath $exe -ArgumentList $vsArgs -Wait -PassThru
Remove-Item -Force $exe
if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) {
  throw "vs_BuildTools exited with $($proc.ExitCode)"
}

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
Write-Host "== Visual Studio $Year done =="
