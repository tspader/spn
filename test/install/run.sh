#!/usr/bin/env sh
set -eu

usage() {
  printf 'usage: run.sh <installer_render> [--assets <dir>]\n' >&2
  exit 1
}

RENDER="${1:-}"
[ -n "$RENDER" ] || usage
RENDER="$(cd "$(dirname "$RENDER")" && pwd)/$(basename "$RENDER")"
shift

ASSETS=""
if [ $# -gt 0 ]; then
  [ "$1" = "--assets" ] && [ $# -eq 2 ] || usage
  ASSETS="$(cd "$2" && pwd)"
fi

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TEMPLATES="$ROOT/tools/install/templates"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

die() {
  printf 'run.sh: %s\n' "$1" >&2
  exit 1
}

has() {
  command -v "$1" >/dev/null 2>&1
}

checksum() {
  if has sha256sum; then
    sha256sum "$1" | cut -d' ' -f1
  else
    shasum -a 256 "$1" | cut -d' ' -f1
  fi
}

TOOLS="sh env mktemp mkdir mv chmod rm grep cut tar gzip curl sha256sum shasum"

make_bin() {
  mkdir -p "$1"
  for tool in $TOOLS; do
    if has "$tool"; then
      ln -s "$(command -v "$tool")" "$1/$tool"
    fi
  done
}

assert_rc() {
  [ "$RC" = "$1" ] || die "$NAME: expected rc $1, got $RC$(printf '\n'; cat "$CASE/out")"
}

assert_fails() {
  [ "$RC" != 0 ] || die "$NAME: expected failure, got rc 0$(printf '\n'; cat "$CASE/out")"
}

assert_out() {
  grep -Fq "$1" "$CASE/out" || die "$NAME: output missing '$1'$(printf '\n'; cat "$CASE/out")"
}

assert_file() {
  [ -f "$1" ] || die "$NAME: expected file $1"
}

assert_no_file() {
  [ ! -e "$1" ] || die "$NAME: expected no file $1"
}

assert_line() {
  grep -Fq "$1" "$2" || die "$NAME: $2 missing '$1'"
}

assert_line_once() {
  [ "$(grep -Fc "$1" "$2")" = 1 ] || die "$NAME: $2 does not contain '$1' exactly once"
}

FIX="$WORK/fix"
mkdir -p "$FIX"
printf '#!/bin/sh\necho "spn 0.0.0 (stub)"\n' > "$WORK/spn"
chmod +x "$WORK/spn"
tar -czf "$FIX/spn-x86_64-linux.tar.gz" -C "$WORK" spn
cp "$FIX/spn-x86_64-linux.tar.gz" "$FIX/spn-aarch64-macos.tar.gz"
{
  printf '%s  spn-x86_64-linux.tar.gz\n' "$(checksum "$FIX/spn-x86_64-linux.tar.gz")"
  printf '%s  spn-aarch64-macos.tar.gz\n' "$(checksum "$FIX/spn-aarch64-macos.tar.gz")"
  printf 'cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc  spn-x86_64-windows.zip\n'
} > "$FIX/SHASUMS256.txt"
"$RENDER" "$FIX/SHASUMS256.txt" "$TEMPLATES" "$FIX" 0.0.0 v0.0.0 tspader/spn

if has shellcheck; then
  shellcheck -s sh "$FIX/install.sh"
elif [ "$(uname -s)" = "Linux" ]; then
  die "shellcheck is required on linux"
fi

BAD="$WORK/bad"
mkdir -p "$BAD"
cp "$FIX"/spn-* "$BAD"
printf 'x' >> "$BAD/spn-x86_64-linux.tar.gz"

run_installer() {
  if (cd "$CASE" && env -i PATH="$CASE/bin" HOME="$HOME_DIR" SPN_INSTALL_DOWNLOAD_URL="$URL" "$@" sh "$FIX/install.sh" > "$CASE/out" 2>&1); then
    RC=0
  else
    RC=$?
  fi
}

run_case() {
  NAME="$1"
  CASE="$WORK/case-$NAME"
  HOME_DIR="$CASE/home"
  BIN_DIR="$HOME_DIR/.spn/bin"
  mkdir -p "$HOME_DIR"
  make_bin "$CASE/bin"
  URL="file://$FIX"
  UNAME_S="Linux"
  UNAME_M="x86_64"
  set --

  case "$NAME" in
    mismatch) URL="file://$BAD" ;;
    unsupported_arch) UNAME_M="riscv64" ;;
    no_build) UNAME_M="aarch64" ;;
    intel_mac)
      UNAME_S="Darwin"
      printf '#!/bin/sh\necho 0\n' > "$CASE/bin/sysctl"
      ;;
    rosetta)
      UNAME_S="Darwin"
      printf '#!/bin/sh\necho 1\n' > "$CASE/bin/sysctl"
      ;;
    trampoline)
      set -- "OS=Windows_NT"
      printf '#!/bin/sh\nprintf %%s "$*" > ps-args\n' > "$CASE/bin/powershell"
      ;;
    trampoline_msys)
      UNAME_S="MINGW64_NT-10.0"
      set -- "OS=Windows_NT"
      printf '#!/bin/sh\nprintf %%s "$*" > ps-args\n' > "$CASE/bin/powershell"
      ;;
    unsafe_install) set -- "SPN_INSTALL=$CASE/say\"hi" ;;
    stuck_rc)
      : > "$HOME_DIR/.profile"
      chmod 400 "$HOME_DIR/.profile"
      ;;
    github_path) set -- "GITHUB_PATH=$CASE/gh" ;;
    no_modify) set -- "SPN_INSTALL_NO_MODIFY_PATH=1" ;;
    custom_install)
      set -- "SPN_INSTALL=$CASE/custom"
      BIN_DIR="$CASE/custom/bin"
      ;;
    zdotdir)
      set -- "ZDOTDIR=$CASE/zd"
      mkdir -p "$CASE/zd"
      ;;
    shasum_fallback)
      if ! has sha256sum; then
        printf 'run.sh: %s skipped\n' "$NAME"
        return 0
      fi
      rm "$CASE/bin/sha256sum" "$CASE/bin/shasum" 2>/dev/null || :
      printf '#!/bin/sh\nshift 2\nexec %s "$@"\n' "$(command -v sha256sum)" > "$CASE/bin/shasum"
      ;;
    no_downloader) rm "$CASE/bin/curl" ;;
    wget_fallback)
      rm "$CASE/bin/curl"
      printf '#!/bin/sh\nexec %s -fSsL -o "$3" "$4"\n' "$(command -v curl)" > "$CASE/bin/wget"
      ;;
    shadow)
      cp "$WORK/spn" "$CASE/bin/spn"
      ;;
  esac

  printf '#!/bin/sh\ncase "$1" in\n  -s) echo %s ;;\n  -m) echo %s ;;\nesac\n' "$UNAME_S" "$UNAME_M" > "$CASE/bin/uname"
  for entry in "$CASE/bin/"*; do
    if [ ! -L "$entry" ]; then
      chmod +x "$entry"
    fi
  done
  touch "$HOME_DIR/.bashrc"

  run_installer "$@"

  case "$NAME" in
    linux)
      assert_rc 0
      assert_out "installed to"
      assert_file "$BIN_DIR/spn"
      assert_file "$HOME_DIR/.spn/env"
      assert_line '. "$HOME/.spn/env"' "$HOME_DIR/.profile"
      assert_line '. "$HOME/.spn/env"' "$HOME_DIR/.bashrc"
      assert_line '. "$HOME/.spn/env"' "$HOME_DIR/.zshrc"
      assert_no_file "$HOME_DIR/.bash_profile"
      assert_line '"$HOME/.spn/bin"' "$HOME_DIR/.config/fish/conf.d/spn.fish"
      ;;
    idempotent)
      run_installer "$@"
      assert_rc 0
      assert_line_once '. "$HOME/.spn/env"' "$HOME_DIR/.profile"
      assert_line_once '. "$HOME/.spn/env"' "$HOME_DIR/.bashrc"
      assert_line_once '. "$HOME/.spn/env"' "$HOME_DIR/.zshrc"
      ;;
    mismatch)
      assert_fails
      assert_out "sha256 mismatch"
      assert_no_file "$BIN_DIR/spn"
      ;;
    unsupported_arch)
      assert_fails
      assert_out "unsupported architecture"
      ;;
    no_build)
      assert_fails
      assert_out "has no build for aarch64-linux"
      ;;
    intel_mac)
      assert_fails
      assert_out "has no build for x86_64-macos"
      ;;
    rosetta)
      assert_rc 0
      assert_out "(aarch64-macos)"
      assert_file "$BIN_DIR/spn"
      ;;
    trampoline|trampoline_msys)
      assert_rc 0
      grep -Fq "install.ps1" "$CASE/ps-args" || die "$NAME: powershell was not invoked with install.ps1"
      assert_no_file "$BIN_DIR/spn"
      ;;
    unsafe_install)
      assert_fails
      assert_out "SPN_INSTALL may not contain"
      ;;
    stuck_rc)
      assert_rc 0
      assert_out "installed to"
      assert_file "$BIN_DIR/spn"
      assert_out "could not update"
      assert_out "$HOME_DIR/.profile"
      assert_line '. "$HOME/.spn/env"' "$HOME_DIR/.bashrc"
      ;;
    github_path)
      assert_rc 0
      grep -Fxq "$BIN_DIR" "$CASE/gh" || die "$NAME: GITHUB_PATH does not contain $BIN_DIR"
      assert_file "$HOME_DIR/.spn/env"
      assert_no_file "$HOME_DIR/.profile"
      ;;
    no_modify)
      assert_rc 0
      assert_out "add $BIN_DIR to your PATH"
      assert_no_file "$HOME_DIR/.profile"
      assert_no_file "$HOME_DIR/.spn/env"
      ;;
    custom_install)
      assert_rc 0
      assert_file "$BIN_DIR/spn"
      assert_file "$CASE/custom/env"
      assert_line ". \"$CASE/custom/env\"" "$HOME_DIR/.profile"
      ;;
    zdotdir)
      assert_rc 0
      assert_line '. "$HOME/.spn/env"' "$CASE/zd/.zshrc"
      assert_no_file "$HOME_DIR/.zshrc"
      ;;
    shasum_fallback)
      assert_rc 0
      assert_file "$BIN_DIR/spn"
      ;;
    no_downloader)
      assert_fails
      assert_out "curl or wget is required"
      ;;
    wget_fallback)
      assert_rc 0
      assert_file "$BIN_DIR/spn"
      ;;
    shadow)
      assert_rc 0
      assert_out "shadows"
      ;;
    *)
      die "unknown case $NAME"
      ;;
  esac
  printf 'run.sh: %s ok\n' "$NAME"
}

CASES="linux idempotent mismatch unsupported_arch no_build intel_mac rosetta trampoline trampoline_msys unsafe_install stuck_rc github_path no_modify custom_install zdotdir shasum_fallback no_downloader wget_fallback shadow"
for name in $CASES; do
  run_case "$name"
done

if [ -n "$ASSETS" ]; then
  REAL="$WORK/real"
  mkdir -p "$REAL"
  for asset in "$ASSETS"/spn-*.tar.gz "$ASSETS"/spn-*.zip; do
    if [ -f "$asset" ]; then
      printf '%s  %s\n' "$(checksum "$asset")" "$(basename "$asset")"
    fi
  done > "$REAL/SHASUMS256.txt"
  "$RENDER" "$REAL/SHASUMS256.txt" "$TEMPLATES" "$REAL" 0.0.0 v0.0.0 tspader/spn

  NAME="real"
  CASE="$WORK/case-real"
  HOME_DIR="$CASE/home"
  mkdir -p "$HOME_DIR"
  ASSETS_URL="file://$ASSETS"
  if (cd "$CASE" && HOME="$HOME_DIR" GITHUB_PATH='' SPN_INSTALL_DOWNLOAD_URL="$ASSETS_URL" sh "$REAL/install.sh" > "$CASE/out" 2>&1); then
    RC=0
  else
    RC=$?
  fi
  assert_rc 0
  assert_out "installed to"
  printf 'run.sh: real ok\n'
fi

printf 'run.sh: all cases passed\n'
