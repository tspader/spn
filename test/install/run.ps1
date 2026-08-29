param(
  [Parameter(Mandatory)] [string]$Renderer,
  [Parameter(Mandatory)] [string]$Assets,
  [switch]$Registry
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Templates = Join-Path $Root "tools\install\templates"
$Assets = Resolve-Path $Assets
$Work = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) "spn-install-test-$([Guid]::NewGuid())")

function FileUrl($Path) {
  "file:///" + ("$Path" -replace '\\', '/')
}

$AssetFiles = Get-ChildItem (Join-Path $Assets "spn-*") | Where-Object { $_.Name -match '\.(zip|tar\.gz)$' }
$Lines = $AssetFiles | ForEach-Object {
  "{0}  {1}" -f (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower(), $_.Name
}
Set-Content -Path (Join-Path $Work "SHASUMS256.txt") -Value $Lines
& $Renderer (Join-Path $Work "SHASUMS256.txt") $Templates $Work "0.0.0" "v0.0.0" "tspader/spn"
if ($LASTEXITCODE -ne 0) {
  throw "run.ps1: renderer failed"
}
$Installer = Join-Path $Work "install.ps1"

$Bad = New-Item -ItemType Directory -Path (Join-Path $Work "bad")
$AssetFiles | Copy-Item -Destination $Bad
Get-ChildItem (Join-Path $Bad "spn-*") | ForEach-Object { Add-Content -Path $_.FullName -Value "x" }

$Cases = @(
  @{
    Name = "happy"
    Env = @{ SPN_INSTALL = "$Work\happy"; SPN_INSTALL_NO_MODIFY_PATH = "1"; SPN_INSTALL_DOWNLOAD_URL = FileUrl $Assets }
    ExpectOut = @("installed to", "add $Work\happy\bin to your PATH")
    ExpectFile = "$Work\happy\bin\spn.exe"
  },
  @{
    Name = "replace"
    Env = @{ SPN_INSTALL = "$Work\happy"; SPN_INSTALL_NO_MODIFY_PATH = "1"; SPN_INSTALL_DOWNLOAD_URL = FileUrl $Assets }
    ExpectOut = @("installed to")
    ExpectFile = "$Work\happy\bin\spn.exe"
  },
  @{
    Name = "mismatch"
    Env = @{ SPN_INSTALL = "$Work\mismatch"; SPN_INSTALL_NO_MODIFY_PATH = "1"; SPN_INSTALL_DOWNLOAD_URL = FileUrl $Bad }
    ExpectFail = $true
    ExpectOut = @("sha256 mismatch")
  },
  @{
    Name = "github_path"
    Env = @{ SPN_INSTALL = "$Work\ghp"; SPN_INSTALL_DOWNLOAD_URL = FileUrl $Assets; GITHUB_PATH = "$Work\ghp-file" }
    ExpectOut = @("installed to")
    ExpectPath = @{ File = "$Work\ghp-file"; Line = "$Work\ghp\bin" }
  }
)

if ($Registry) {
  $Cases += @{
    Name = "registry"
    Env = @{ SPN_INSTALL = "$Work\reg"; SPN_INSTALL_DOWNLOAD_URL = FileUrl $Assets }
    ExpectOut = @("installed to")
    ExpectRegistry = "$Work\reg\bin"
  }
}

$Failed = $false
$RunnerGithubPath = $env:GITHUB_PATH
foreach ($Case in $Cases) {
  Remove-Item -Path env:GITHUB_PATH -ErrorAction SilentlyContinue
  foreach ($Key in $Case.Env.Keys) {
    Set-Item -Path "env:$Key" -Value $Case.Env[$Key]
  }
  $SavedPath = if ($Case.ExpectRegistry) {
    (Get-Item -Path 'HKCU:\Environment').GetValue('Path', '', 'DoNotExpandEnvironmentNames')
  }

  $PrevPreference = $ErrorActionPreference
  $ErrorActionPreference = "Continue"
  $Out = & powershell -NoProfile -ExecutionPolicy Bypass -Command "iex (Get-Content -Raw '$Installer')" 2>&1 | Out-String
  $Rc = $LASTEXITCODE
  $ErrorActionPreference = $PrevPreference

  foreach ($Key in $Case.Env.Keys) {
    Remove-Item -Path "env:$Key" -ErrorAction SilentlyContinue
  }

  $Errors = @()
  if ($Case.ExpectFail -and $Rc -eq 0) { $Errors += "expected failure, got rc 0" }
  if (-not $Case.ExpectFail -and $Rc -ne 0) { $Errors += "expected rc 0, got $Rc" }
  foreach ($Expected in $Case.ExpectOut) {
    if (-not $Out.Contains($Expected)) { $Errors += "output missing '$Expected'" }
  }
  if ($Case.ExpectFile -and -not (Test-Path $Case.ExpectFile)) { $Errors += "missing file $($Case.ExpectFile)" }
  if ($Case.ExpectPath) {
    $Content = Get-Content -Path $Case.ExpectPath.File -ErrorAction SilentlyContinue
    if ($Content -notcontains $Case.ExpectPath.Line) { $Errors += "GITHUB_PATH missing $($Case.ExpectPath.Line)" }
  }
  if ($Out -match 'curl:\s') { $Errors += "curl reported an error" }
  $CaseRoot = $Case.Env.SPN_INSTALL
  if ($CaseRoot) {
    $Stages = Get-ChildItem -Path (Join-Path $CaseRoot "bin") -Filter ".stage-*" -Directory -ErrorAction SilentlyContinue
    if ($Stages) { $Errors += "staging directories left behind: $($Stages.Name -join ', ')" }
  }
  if ($Case.ExpectRegistry) {
    $RegPath = (Get-Item -Path 'HKCU:\Environment').GetValue('Path', '', 'DoNotExpandEnvironmentNames') -split ';'
    if ($RegPath -notcontains $Case.ExpectRegistry) { $Errors += "registry PATH missing $($Case.ExpectRegistry)" }
    $Key = (Get-Item -Path 'HKCU:').OpenSubKey('Environment', $true)
    $Key.SetValue('Path', $SavedPath, $Key.GetValueKind('Path'))
  }

  if ($Errors.Count -gt 0) {
    $Failed = $true
    Write-Output "run.ps1: $($Case.Name) FAILED"
    $Errors | ForEach-Object { Write-Output "  $_" }
    Write-Output $Out
  } else {
    Write-Output "run.ps1: $($Case.Name) ok"
  }
}

if ($RunnerGithubPath) {
  $env:GITHUB_PATH = $RunnerGithubPath
}
Remove-Item -Recurse -Force $Work
if ($Failed) {
  exit 1
}
Write-Output "run.ps1: all cases passed"
