# msys2.ps1 — install MSYS2 mingw toolchain groups.
# Arg: comma-separated environments, e.g. "clang64,ucrt64,mingw64".
param([string]$Environments = 'clang64,ucrt64,mingw64')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Msys = 'C:\tools\msys64'
$Bash = Join-Path $Msys 'usr\bin\bash.exe'

Write-Host "== msys2: $Environments =="

if (-not (Test-Path $Bash)) {
  if (Get-Command choco -ErrorAction SilentlyContinue) {
    choco install msys2 -y --no-progress
    if ($LASTEXITCODE -ne 0) { throw "choco install msys2 failed ($LASTEXITCODE)" }
  }
  else {
    throw "MSYS2 not found at $Msys and choco is unavailable"
  }
}

& $Bash -lc 'pacman -Syuu --noconfirm'

$map = @{
  'clang64'    = 'mingw-w64-clang-x86_64-toolchain'
  'ucrt64'     = 'mingw-w64-ucrt-x86_64-toolchain'
  'mingw64'    = 'mingw-w64-x86_64-toolchain'
  'clangarm64' = 'mingw-w64-clang-aarch64-toolchain'
}

foreach ($environment in ($Environments -split '[,\s]+' | Where-Object { $_ })) {
  $group = $map[$environment]
  if (-not $group) { throw "unknown msys2 environment: $environment" }
  Write-Host "  installing $group"
  & $Bash -lc "pacman -S --needed --noconfirm $group"
  if ($LASTEXITCODE -ne 0) { throw "pacman failed for $group ($LASTEXITCODE)" }
}

Write-Host "== msys2 done =="
