param()
$ErrorActionPreference = 'SilentlyContinue'

function Find-VsWhere {
    $cmd = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $roots = @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }
    foreach ($root in $roots) {
        $candidate = Join-Path $root 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (Test-Path $candidate) { return $candidate }
    }
    return $null
}

$vswhere = Find-VsWhere
if (-not $vswhere) { exit 0 }

$installationVersion = & $vswhere -latest -products * -property installationVersion

if (-not $installationVersion) { exit 0 }

$major = $installationVersion.Split('.')[0]
if ([int]$major -lt 17) { exit 0 }

Write-Output "Visual Studio $major"
