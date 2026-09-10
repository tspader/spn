# llvm.ps1 — install LLVM (clang, clang-cl, lld-link, llvm-ar) on PATH.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Version = '22.1.7'
$Dest = 'C:\Program Files\LLVM'

function Add-MachinePath([string]$Dir) {
  $path = [Environment]::GetEnvironmentVariable('PATH', 'Machine')
  if (($path -split ';') -notcontains $Dir) {
    [Environment]::SetEnvironmentVariable('PATH', "$path;$Dir", 'Machine')
  }
  $env:PATH = "$env:PATH;$Dir"
}

Write-Host "== llvm $Version =="

if (Get-Command choco -ErrorAction SilentlyContinue) {
  choco install llvm --version=$Version -y --no-progress
  if ($LASTEXITCODE -ne 0) { throw "choco install llvm failed ($LASTEXITCODE)" }
}
else {
  $exe = Join-Path $env:TEMP "LLVM-$Version-win64.exe"
  $url = "https://github.com/llvm/llvm-project/releases/download/llvmorg-$Version/LLVM-$Version-win64.exe"
  Invoke-WebRequest -Uri $url -OutFile $exe -UseBasicParsing
  Start-Process -FilePath $exe -ArgumentList '/S' -Wait
  Remove-Item -Force $exe
}

Add-MachinePath (Join-Path $Dest 'bin')
& "$Dest\bin\clang.exe" --version | Select-Object -First 1
Write-Host "== llvm done: $Dest =="
