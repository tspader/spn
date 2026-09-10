# w64devkit.ps1 — install the w64devkit mingw gcc toolchain on PATH.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Version = '1.23.0'
$Url = "https://github.com/skeeto/w64devkit/releases/download/v$Version/w64devkit-$Version.zip"
$Sha = '5c7dce6762be3e0dba648a9317790444c0e2f1ef3e677315c115727d7a549539'
$Dest = 'C:\toolchains\w64devkit'

function Add-MachinePath([string]$Dir) {
  $path = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
  if (($path -split ';') -notcontains $Dir) {
    [Environment]::SetEnvironmentVariable('PATH', "$path;$Dir", 'Machine')
  }
  $env:PATH = "$env:PATH;$Dir"
}

Write-Host "== w64devkit $Version =="
$zip = Join-Path $env:TEMP "w64devkit-$Version.zip"
Invoke-WebRequest -Uri $Url -OutFile $zip -UseBasicParsing

$actual = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
if ($actual -ne $Sha) { throw "w64devkit sha256 mismatch: got $actual, want $Sha" }

if (Test-Path $Dest) { Remove-Item -Recurse -Force $Dest }
$staging = Join-Path $env:TEMP "w64devkit-extract-$Version"
if (Test-Path $staging) { Remove-Item -Recurse -Force $staging }
Expand-Archive -Path $zip -DestinationPath $staging -Force

$top = Get-ChildItem -Directory $staging | Select-Object -First 1
Move-Item -Path $top.FullName -Destination $Dest
Remove-Item -Recurse -Force $staging
Remove-Item -Force $zip

Add-MachinePath (Join-Path $Dest 'bin')
& "$Dest\bin\gcc.exe" --version | Select-Object -First 1
Write-Host "== w64devkit done: $Dest =="
