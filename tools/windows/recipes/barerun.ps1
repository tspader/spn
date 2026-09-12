param(
  [Parameter(Mandatory=$true)][string]$Exe
)
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
Set-StrictMode -Version Latest

$STATUS_DLL_NOT_FOUND       = -1073741515
$STATUS_INVALID_IMAGE_FORMAT = -1073741701
$SEM_NO_ERROR_DIALOGS        = 0x8003

if (-not (Test-Path $Exe)) { throw "no probe at $Exe" }

Add-Type -Namespace Win32 -Name ErrorMode -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
[void][Win32.ErrorMode]::SetErrorMode($SEM_NO_ERROR_DIALOGS)

Write-Host "== barerun: $Exe =="
& $Exe
$code = $LASTEXITCODE
Write-Host "== barerun: exit $code =="

if ($code -eq 0) { exit 0 }
if ($code -eq $STATUS_DLL_NOT_FOUND -or $code -eq $STATUS_INVALID_IMAGE_FORMAT) { exit 10 }
exit 20
