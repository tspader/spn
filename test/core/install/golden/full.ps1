$Version = "0.0.0"
$Tag = "v0.0.0"
$Repo = "A/B"
$Targets = @{
  "aarch64-macos" = @{ Asset = "spn-aarch64-macos.tar.gz"; Sha = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; Exe = "spn" }
  "x86_64-linux" = @{ Asset = "spn-x86_64-linux.tar.gz"; Sha = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"; Exe = "spn" }
  "x86_64-windows" = @{ Asset = "spn-x86_64-windows.zip"; Sha = "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"; Exe = "spn.exe" }
}
