#!/usr/bin/env sh
# zig c++ is the C++ compiler, matching zig-cc.sh for C. Fast JIT (asmjit +
# WAMR's x86_64 codegen) is the only C++ in the tree; keeping it on zig's
# toolchain keeps the C++ runtime (libc++/libunwind/compiler-rt) consistent
# with the zig-linked binary instead of dragging in system libstdc++.
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
export ZIG_LOCAL_CACHE_DIR="${ZIG_LOCAL_CACHE_DIR:-$ROOT/.cache/zig}"
export ZIG_GLOBAL_CACHE_DIR="${ZIG_GLOBAL_CACHE_DIR:-$ROOT/.cache/zig}"
ZIG="$HOME/.zvm/0.16.0/zig"
if [ ! -x "$ZIG" ]; then
  ZIG="$(command -v zig)"
  if [ -z "$ZIG" ] || [ "$("$ZIG" version)" != "0.16.0" ]; then
    echo "zig-cxx.sh: need zig 0.16.0, found none at $HOME/.zvm/0.16.0/zig or on PATH" >&2
    exit 1
  fi
fi
exec "$ZIG" c++ "$@"
