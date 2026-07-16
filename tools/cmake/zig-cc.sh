#!/usr/bin/env sh
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$ROOT/.cache/zig}"
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$ROOT/.cache/zig}"
ZIG="$(sh "$ROOT/tools/cmake/zig-toolchain.sh")"
exec "$ZIG" cc "$@"
