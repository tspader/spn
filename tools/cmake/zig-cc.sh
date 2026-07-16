#!/usr/bin/env sh
# zig cc is the compiler, always. Pinned to the zvm-managed 0.16.0 (failing, checks if
# whatever zig is on PATH is 0.16.0), with its object cache kept in-repo. The cache
# survives `make clean` and dies with `make nuke`.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$ROOT/.cache/zig}"
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$ROOT/.cache/zig}"
ZIG="$HOME/.zvm/0.16.0/zig"
if [ ! -x "$ZIG" ]; then
  ZIG="$(command -v zig)"
  if [ -z "$ZIG" ] || [ "$("$ZIG" version)" != "0.16.0" ]; then
    echo "zig-cc.sh: need zig 0.16.0, found none at $HOME/.zvm/0.16.0/zig or on PATH" >&2
    exit 1
  fi
fi
exec "$ZIG" cc "$@"
