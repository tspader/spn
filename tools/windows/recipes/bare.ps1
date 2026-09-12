$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Write-Host "== bare: remove every Visual C++ redistributable =="

$keys = @(
  'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*',
  'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
)
$entries = Get-ItemProperty $keys | Where-Object {
  $_.PSObject.Properties['DisplayName'] -and $_.DisplayName -like 'Microsoft Visual C++*' -and $_.PSObject.Properties['UninstallString']
}

foreach ($entry in $entries) {
  $name = $entry.DisplayName
  $cmd = $entry.UninstallString
  if ($cmd -like '*VC_redist*') {
    Write-Host "  bundle: $name"
    $proc = Start-Process -FilePath ($cmd -split '"')[1] -ArgumentList '/uninstall', '/quiet', '/norestart' -Wait -PassThru
    if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010) { throw "$name uninstall exited with $($proc.ExitCode)" }
  }
  elseif ($cmd -match '^MsiExec\.exe /X(\{[0-9A-Fa-f-]+\})') {
    Write-Host "  msi: $name"
    $proc = Start-Process -FilePath 'msiexec.exe' -ArgumentList '/x', $Matches[1], '/qn', '/norestart' -Wait -PassThru
    if ($proc.ExitCode -ne 0 -and $proc.ExitCode -ne 3010 -and $proc.ExitCode -ne 1605) { throw "$name uninstall exited with $($proc.ExitCode)" }
  }
}

$left = Get-ChildItem 'C:\Windows\System32\vcruntime140*.dll', 'C:\Windows\System32\msvcp140*.dll' -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -notlike '*_clr0400.dll' }
if ($left) { throw "runtime DLLs still present: $($left.Name -join ', ')" }

Write-Host "== bare: done =="
