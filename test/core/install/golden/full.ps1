param(
  [switch]$NoModifyPath
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Version = "0.0.0"
$Tag = "v0.0.0"
$BaseUrl = if ($env:SPN_INSTALL_DOWNLOAD_URL) { $env:SPN_INSTALL_DOWNLOAD_URL } else { "https://github.com/A/B/releases/download/$Tag" }
$Targets = @{
  "x86_64-windows" = @{ Asset = "spn-x86_64-windows.zip"; Sha = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; Exe = "spn.exe" }
}

function Fetch($Url, $Path) {
  if (Get-Command curl.exe -ErrorAction SilentlyContinue) {
    $Flags = if ([Console]::IsErrorRedirected) { @("-fSsL") } else { @("-fSL", "--progress-bar") }
    & curl.exe @Flags -o $Path $Url
    if ($LASTEXITCODE -eq 0) { return }
  }
  Invoke-RestMethod -Uri $Url -OutFile $Path
}

function Add-UserPath($Dir) {
  $Key = (Get-Item -Path 'HKCU:').OpenSubKey('Environment', $true)
  try {
    $Path = $Key.GetValue('Path', '', 'DoNotExpandEnvironmentNames') -split ';' -ne ''
    if ($Path -notcontains $Dir) {
      $Kind = if ($Path) { $Key.GetValueKind('Path') } else { [Microsoft.Win32.RegistryValueKind]::String }
      $Key.SetValue('Path', (@($Dir) + $Path) -join ';', $Kind)
      $Dummy = "spn-install-" + [Guid]::NewGuid()
      [Environment]::SetEnvironmentVariable($Dummy, "1", "User")
      [Environment]::SetEnvironmentVariable($Dummy, [NullString]::Value, "User")
    }
  } finally {
    $Key.Dispose()
  }
  $env:Path = "$Dir;$env:Path"
}

if ($PSVersionTable.PSVersion.Major -lt 5) {
  throw "install: powershell 5 or later is required to install spn"
}
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072

$Machine = (Get-ItemProperty 'HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment').PROCESSOR_ARCHITECTURE
$Cpu = switch ($Machine) {
  "AMD64" { "x86_64" }
  "ARM64" { "aarch64" }
  default { throw "install: spn does not support windows on $Machine" }
}
$TargetName = "$Cpu-windows"
if (-not $Targets.ContainsKey($TargetName)) {
  if (-not $Targets.ContainsKey("x86_64-windows")) {
    throw "install: spn $Version has no build for $TargetName"
  }
  Write-Output "install: no $TargetName build; using x86_64-windows under emulation"
  $TargetName = "x86_64-windows"
}
$Target = $Targets[$TargetName]

$Root = if ($env:SPN_INSTALL) { $env:SPN_INSTALL } else { Join-Path $Home ".spn" }
$Bin = Join-Path $Root "bin"
$Exe = Join-Path $Bin $Target.Exe
$null = New-Item -ItemType Directory -Force -Path $Bin

$Stage = New-Item -ItemType Directory -Path (Join-Path $Bin ".stage-$([Guid]::NewGuid())")
try {
  $Archive = Join-Path $Stage $Target.Asset
  $Url = "$BaseUrl/$($Target.Asset)"

  Write-Output "install: downloading spn $Version ($TargetName)"
  Fetch $Url $Archive

  $Got = (Get-FileHash -Algorithm SHA256 -Path $Archive).Hash.ToLower()
  if ($Got -ne $Target.Sha) {
    throw "install: sha256 mismatch for $($Target.Asset) (got $Got, want $($Target.Sha)); if a release is being published right now, retry in a minute"
  }

  Expand-Archive -Path $Archive -DestinationPath $Stage -Force
  try {
    Move-Item -Force (Join-Path $Stage $Target.Exe) $Exe
  } catch {
    throw "install: failed to replace ${Exe}; close any running spn and retry ($_)"
  }
} finally {
  Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
}

$VersionOut = & $Exe --version
if ($LASTEXITCODE -ne 0) {
  throw "install: the installed spn failed to run"
}

$PathState = "ok"
if (($env:Path -split ';') -notcontains $Bin) {
  if ($NoModifyPath -or $env:SPN_INSTALL_NO_MODIFY_PATH) {
    $PathState = "manual"
  } elseif ($env:GITHUB_PATH) {
    Add-Content -Path $env:GITHUB_PATH -Value $Bin
    $PathState = "ci"
  } else {
    Add-UserPath $Bin
    $PathState = "updated"
  }
}

Write-Output "install: $VersionOut installed to $Exe"
switch ($PathState) {
  "ok" { }
  "ci" { }
  "updated" { Write-Output "install: restart your terminal to use spn" }
  "manual" { Write-Output "install: add $Bin to your PATH" }
}

$Shadow = Get-Command spn -ErrorAction SilentlyContinue
if ($Shadow -and $Shadow.Source -ne $Exe) {
  Write-Output "install: another spn at $($Shadow.Source) shadows $Exe"
}
