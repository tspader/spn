param(
  [Parameter(Mandatory=$true)][string]$Dir,
  [Parameter(Mandatory=$true)][string]$Exe
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$exe = Join-Path $Dir $Exe
Write-Host "== barerun: $exe =="

& $exe
$code = $LASTEXITCODE

# STATUS_DLL_NOT_FOUND (0xC0000135) and STATUS_INVALID_IMAGE_FORMAT (0xC000007B)
# arrive as their signed 32-bit values; both mean the loader refused the image.
$DLL_NOT_FOUND     = -1073741515
$INVALID_IMAGE_FMT = -1073741701

Write-Host "== barerun exit $code =="
if ($code -eq 0) { exit 0 }
elseif ($code -eq $DLL_NOT_FOUND -or $code -eq $INVALID_IMAGE_FMT) { exit 10 }
else { exit 20 }
