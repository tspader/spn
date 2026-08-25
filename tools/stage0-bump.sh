#!/usr/bin/env sh
set -eu

if [ $# -ne 1 ]; then
  echo "usage: stage0-bump.sh <version>" >&2
  exit 1
fi
VERSION="${1#v}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STAGE0="$ROOT/tools/stage0.sh"

ASSETS="$(gh api "repos/tspader/spn/releases/tags/v$VERSION" --jq '.assets[] | "\(.name) \(.digest)"')"

digest() {
  SHA="$(printf '%s\n' "$ASSETS" | sed -n "s/^$1 sha256://p")"
  printf '%s' "$SHA" | grep -Eq '^[0-9a-f]{64}$' || { echo "stage0-bump: no sha256 digest for $1 on v$VERSION" >&2; exit 1; }
  printf '%s\n' "$SHA"
}

LINUX="$(digest spn-x86_64-linux.tar.gz)"
MACOS="$(digest spn-aarch64-macos.tar.gz)"
WINDOWS="$(digest spn-x86_64-windows.zip)"

sed \
  -e "s/^VERSION=\".*\"/VERSION=\"$VERSION\"/" \
  -e "s/\(ASSET=\"spn-x86_64-linux[^\"]*\" *SHA=\)[0-9a-f]*/\1$LINUX/" \
  -e "s/\(ASSET=\"spn-aarch64-macos[^\"]*\" *SHA=\)[0-9a-f]*/\1$MACOS/" \
  -e "s/\(ASSET=\"spn-x86_64-windows[^\"]*\" *SHA=\)[0-9a-f]*/\1$WINDOWS/" \
  "$STAGE0" > "$STAGE0.tmp"
mv "$STAGE0.tmp" "$STAGE0"
chmod +x "$STAGE0"

echo "stage0-bump: pinned v$VERSION" >&2
