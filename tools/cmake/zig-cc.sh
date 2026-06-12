#!/usr/bin/env sh
# zig cc is the compiler, always. Pinned to the zvm-managed 0.16.0 rather than
# whatever zig is on PATH, with its object cache kept in-repo. The cache
# survives `make clean` and dies with `make nuke`.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$ROOT/.cache/zig}"
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$ROOT/.cache/zig}"
exec "$HOME/.zvm/0.16.0/zig" cc "$@"
