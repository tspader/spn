$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# >>> spn release data
$Version = "0.0.0"
$Tag = "v0.0.0"
$Repo = "tspader/spn"
$Targets = @{}
# <<< spn release data

$BaseUrl = if ($env:SPN_INSTALL_DOWNLOAD_URL) {
  $env:SPN_INSTALL_DOWNLOAD_URL
} else {
  "https://github.com/$Repo/releases/download/$Tag"
}

function Fetch-Quiet($Url, $Path) {
  & curl.exe -fSsL -o $Path $Url
  if ($LASTEXITCODE -ne 0) { throw "install: failed to download $Url" }
}

function Fetch-Progress($Url, $Path) {
  & curl.exe -fSL --progress-bar -o $Path $Url
  if ($LASTEXITCODE -ne 0) { throw "install: failed to download $Url" }
}

function Fetch-Web($Url, $Path) {
  Invoke-RestMethod -Uri $Url -OutFile $Path
}

if ($PSVersionTable.PSVersion.Major -lt 5) {
  throw "install: powershell 5 or later is required to install spn"
}
[Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor 3072

$Download = if (-not (Get-Command curl.exe -ErrorAction SilentlyContinue)) {
  "Fetch-Web"
} elseif ([Console]::IsErrorRedirected) {
  "Fetch-Quiet"
} else {
  "Fetch-Progress"
}

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

$Stage = New-Item -ItemType Directory -Path (Join-Path ([IO.Path]::GetTempPath()) "spn-install-$([Guid]::NewGuid())")
try {
  $Archive = Join-Path $Stage $Target.Asset

  Write-Output "install: downloading spn $Version ($TargetName)"
  & $Download "$BaseUrl/$($Target.Asset)" $Archive

  $Got = (Get-FileHash -Algorithm SHA256 -Path $Archive).Hash.ToLower()
  if ($Got -ne $Target.Sha) {
    throw "install: sha256 mismatch for $($Target.Asset) (got $Got, want $($Target.Sha)); if a release is being published right now, retry in a minute"
  }

  Expand-Archive -Path $Archive -DestinationPath $Stage -Force
  & (Join-Path $Stage $Target.Exe) self install
  if ($LASTEXITCODE -ne 0) {
    throw "install: spn self install failed"
  }
} finally {
  Remove-Item -Recurse -Force $Stage -ErrorAction SilentlyContinue
}
