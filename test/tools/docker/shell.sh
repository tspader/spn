#!/bin/sh
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../.." && pwd)"
OUT="build/$(uname -m)-linux-musl/musl"
GITDIR="$(cd "$(git -C "$ROOT" rev-parse --git-common-dir)" && pwd)"
TOOLCHAINS="$HOME/.local/share/spn/cache/toolchain"
ZIG_CACHE="$HOME/.cache/zig"
mkdir -p "$TOOLCHAINS" "$ZIG_CACHE"
variant="${1:?usage: shell.sh <variant>}"
image="spn-$variant"

docker build -q -t "$image" "$DIR/variants/$variant" >/dev/null
exec docker run --rm -it --user "$(id -u):$(id -g)" -e HOME="$HOME" -e PATH="$ROOT/$OUT:/usr/local/bin:/usr/bin:/bin" \
  -v "$ROOT:$ROOT" -v "$GITDIR:$GITDIR" -v "$TOOLCHAINS:$TOOLCHAINS" -v "$ZIG_CACHE:$ZIG_CACHE" -w "$ROOT" "$image" /bin/sh
