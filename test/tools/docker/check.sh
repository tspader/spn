#!/bin/sh
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$DIR/../../.." && pwd)"
PROFILE=musl
OUT="build/$(uname -m)-linux-$PROFILE/$PROFILE"
GITDIR="$(cd "$(git -C "$ROOT" rev-parse --git-common-dir)" && pwd)"
TOOLCHAINS="$HOME/.local/share/spn/cache/toolchain"
ZIG_CACHE="$HOME/.cache/zig"
mkdir -p "$TOOLCHAINS" "$ZIG_CACHE"

if [ ! -x "$ROOT/$OUT/test/integration" ]; then
  echo "building $PROFILE profile" >&2
  (cd "$ROOT" && spn build -p "$PROFILE" && spn build -p "$PROFILE" --test integration)
fi

selected="$*"
failed=""
while read -r variant toolchain filter; do
  case "$variant" in ''|'#'*) continue ;; esac
  if [ -n "$selected" ]; then
    case " $selected " in *" $variant "*) ;; *) continue ;; esac
  fi
  image="spn-$variant"
  docker build -q -t "$image" "$DIR/variants/$variant" >/dev/null
  echo "== $variant (SPN_TEST_TOOLCHAIN=$toolchain, --filter '$filter')"
  if docker run --rm --user "$(id -u):$(id -g)" -e HOME="$HOME" -e "SPN_TEST_TOOLCHAIN=$toolchain" \
      -v "$ROOT:$ROOT" -v "$GITDIR:$GITDIR" -v "$TOOLCHAINS:$TOOLCHAINS" -v "$ZIG_CACHE:$ZIG_CACHE" -w "$ROOT" "$image" \
      "$ROOT/$OUT/test/integration" --filter "$filter"; then
    echo "PASS $variant"
  else
    echo "FAIL $variant"
    failed="$failed $variant"
  fi
done < "$DIR/variants.txt"

if [ -n "$failed" ]; then
  echo "failed:$failed" >&2
  exit 1
fi
