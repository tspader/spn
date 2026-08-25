param(
  [string]$Arch = "x64"
)

$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$Vs = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $Vs) {
  throw "devenv: no Visual Studio installation with the VC toolset"
}

cmd /c "`"$Vs\VC\Auxiliary\Build\vcvarsall.bat`" $Arch && set" |
  ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') {
      [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
      if ($env:GITHUB_ENV) {
        Add-Content $env:GITHUB_ENV $matches[0]
      }
    }
  }
