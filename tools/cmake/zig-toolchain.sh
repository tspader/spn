#!/usr/bin/env sh
set -eu

VERSION="0.16.0"

if [ -n "${SPN_ZIG:-}" ]; then
  printf '%s\n' "$SPN_ZIG"
  exit 0
fi

case "$(uname -s)" in
  Linux) OS=linux ;;
  Darwin) OS=macos ;;
  *) echo "zig-toolchain: unsupported OS $(uname -s); set SPN_ZIG to a zig $VERSION binary" >&2; exit 1 ;;
esac
case "$(uname -m)" in
  x86_64) ARCH=x86_64 ;;
  aarch64|arm64) ARCH=aarch64 ;;
  *) echo "zig-toolchain: unsupported arch $(uname -m); set SPN_ZIG to a zig $VERSION binary" >&2; exit 1 ;;
esac

case "$ARCH-$OS" in
  x86_64-linux)   SHA=70e49664a74374b48b51e6f3fdfbf437f6395d42509050588bd49abe52ba3d00 ;;
  aarch64-linux)  SHA=ea4b09bfb22ec6f6c6ceac57ab63efb6b46e17ab08d21f69f3a48b38e1534f17 ;;
  x86_64-macos)   SHA=0387557ed1877bc6a2e1802c8391953baddba76081876301c522f52977b52ba7 ;;
  aarch64-macos)  SHA=b23d70deaa879b5c2d486ed3316f7eaa53e84acf6fc9cc747de152450d401489 ;;
esac

NAME="zig-$ARCH-$OS-$VERSION"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/spn/toolchain"
ZIG="$CACHE/$NAME/zig"

if [ ! -x "$ZIG" ]; then
  mkdir -p "$CACHE"
  TMP="$(mktemp -d "$CACHE/fetch.XXXXXX")"
  trap 'rm -rf "$TMP"' EXIT
  echo "zig-toolchain: fetching zig $VERSION ($ARCH-$OS)" >&2
  curl -fsSL "https://ziglang.org/download/$VERSION/$NAME.tar.xz" -o "$TMP/$NAME.tar.xz"
  if command -v sha256sum >/dev/null 2>&1; then
    GOT="$(sha256sum "$TMP/$NAME.tar.xz" | cut -d' ' -f1)"
  else
    GOT="$(shasum -a 256 "$TMP/$NAME.tar.xz" | cut -d' ' -f1)"
  fi
  if [ "$GOT" != "$SHA" ]; then
    echo "zig-toolchain: sha256 mismatch for $NAME.tar.xz (got $GOT, want $SHA)" >&2
    exit 1
  fi
  tar -xJf "$TMP/$NAME.tar.xz" -C "$TMP"
  [ -d "$CACHE/$NAME" ] || mv "$TMP/$NAME" "$CACHE/$NAME"
fi

[ -x "$ZIG" ] || { echo "zig-toolchain: $ZIG missing after fetch" >&2; exit 1; }
printf '%s\n' "$ZIG"
