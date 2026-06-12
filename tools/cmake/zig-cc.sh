#!/usr/bin/env sh
# zig cc is the compiler, always. Pinned to the zvm-managed 0.16.0 rather than
# whatever zig is on PATH, with its object cache kept inside .build so a clean
# is actually a clean.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$ROOT/.build/zig}"
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$ROOT/.build/zig}"
exec "$HOME/.zvm/0.16.0/zig" cc "$@"
