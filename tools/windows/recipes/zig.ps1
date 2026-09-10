# zig.ps1 — install zig as a system toolchain on PATH.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Version = '0.16.0'
$Url = "https://ziglang.org/download/$Version/zig-x86_64-windows-$Version.zip"
$Sha = '68659eb5f1e4eb1437a722f1dd889c5a322c9954607f5edcf337bc3684a75a7e'
$Dest = 'C:\toolchains\zig'

function Add-MachinePath([string]$Dir) {
  $path = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
  if (($path -split ';') -notcontains $Dir) {
    [Environment]::SetEnvironmentVariable('PATH', "$path;$Dir", 'Machine')
  }
  $env:PATH = "$env:PATH;$Dir"
}

Write-Host "== zig $Version =="
$zip = Join-Path $env:TEMP "zig-$Version.zip"
Invoke-WebRequest -Uri $Url -OutFile $zip -UseBasicParsing

$actual = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
if ($actual -ne $Sha) { throw "zig sha256 mismatch: got $actual, want $Sha" }

if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
$staging = Join-Path $env:TEMP "zig-extract-$Version"
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
Expand-Archive -Path $zip -DestinationPath $staging -Force

$top = Get-ChildItem -Directory $staging | Select-Object -First 1
Move-Item -Path $top.FullName -Destination $Dest
Remove-Item -Recurse -Force $staging
Remove-Item -Force $zip

Add-MachinePath $Dest
& "$Dest\zig.exe" version
Write-Host "== zig done: $Dest =="
