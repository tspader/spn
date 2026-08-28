#!/usr/bin/env sh
set -eu

VERSION="0.4.2"

if [ -n "${SPN_STAGE0:-}" ]; then
  printf '%s\n' "$SPN_STAGE0"
  exit 0
fi

case "$(uname -s)" in
  Linux) OS=linux ;;
  Darwin) OS=macos ;;
  MINGW*|MSYS*) OS=windows ;;
  *) echo "stage0: unsupported OS $(uname -s); set SPN_STAGE0 to a spn $VERSION binary" >&2; exit 1 ;;
esac
case "$(uname -m)" in
  x86_64) ARCH=x86_64 ;;
  aarch64|arm64) ARCH=aarch64 ;;
  *) echo "stage0: unsupported arch $(uname -m); set SPN_STAGE0 to a spn $VERSION binary" >&2; exit 1 ;;
esac

EXE=spn
TAR=tar
case "$ARCH-$OS" in
  x86_64-linux)   ASSET="spn-x86_64-linux.tar.gz"  SHA=86140cfc9e5ff4937c2e371f5fa70f8543e6fb94c2754a7fc0f2e1c46e1d44e6 ;;
  aarch64-macos)  ASSET="spn-aarch64-macos.tar.gz" SHA=19ca2d3b638f30044fc54672f00d11f69efdd46bc32e346458c6fab1ff80454c ;;
  x86_64-windows) ASSET="spn-x86_64-windows.zip"   SHA=55c179b4cf92251e22980dd5ae35cb1d94b69f48d97bb07b66bfab4dc14deef6 EXE=spn.exe TAR="C:/Windows/System32/tar.exe" ;;
  *) echo "stage0: no release asset for $ARCH-$OS; set SPN_STAGE0 to a spn $VERSION binary" >&2; exit 1 ;;
esac

CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/spn/stage0"
SPN="$CACHE/$VERSION/$EXE"

if [ ! -x "$SPN" ]; then
  mkdir -p "$CACHE"
  TMP="$(mktemp -d "$CACHE/fetch.XXXXXX")"
  trap 'rm -rf "$TMP"' EXIT
  echo "stage0: fetching spn $VERSION ($ARCH-$OS)" >&2
  curl -fsSL "https://github.com/tspader/spn/releases/download/v$VERSION/$ASSET" -o "$TMP/$ASSET"
  if command -v sha256sum >/dev/null 2>&1; then
    GOT="$(sha256sum "$TMP/$ASSET" | cut -d' ' -f1)"
  else
    GOT="$(shasum -a 256 "$TMP/$ASSET" | cut -d' ' -f1)"
  fi
  if [ "$GOT" != "$SHA" ]; then
    echo "stage0: sha256 mismatch for $ASSET (got $GOT, want $SHA)" >&2
    exit 1
  fi
  "$TAR" -xf "$TMP/$ASSET" -C "$TMP"
  mkdir -p "$CACHE/$VERSION"
  [ -x "$CACHE/$VERSION/$EXE" ] || mv "$TMP/$EXE" "$CACHE/$VERSION/$EXE"
fi

[ -x "$SPN" ] || { echo "stage0: $SPN missing after fetch" >&2; exit 1; }
if command -v cygpath >/dev/null 2>&1; then
  SPN="$(cygpath -m "$SPN")"
fi
printf '%s\n' "$SPN"
