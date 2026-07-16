#pragma once

#include "sp.h"

typedef enum {
  WASM_EMIT_NONE,
  WASM_EMIT_OPEN_READ,
  WASM_EMIT_OPEN_WRITE,
  WASM_EMIT_OPEN_DIR,
  WASM_EMIT_OPEN_AT,
  WASM_EMIT_STAT,
  WASM_EMIT_READDIR,
  WASM_EMIT_CLOSE,
} wasm_emit_op_kind_t;

typedef struct {
  wasm_emit_op_kind_t kind;
  const c8* path;
  u32 mount;
} wasm_emit_op_t;

typedef struct {
  const c8* name;
  const wasm_emit_op_t* ops;
  u32 count;
} wasm_emit_fn_t;

sp_str_t wasm_emit_module(sp_mem_t mem, const wasm_emit_fn_t* fns, u32 count);
