#include "wasm_emit.h"

#define WASM_OP_I32_CONST 0x41
#define WASM_OP_I64_CONST 0x42
#define WASM_OP_I32_LOAD 0x28
#define WASM_OP_CALL 0x10
#define WASM_OP_DROP 0x1A
#define WASM_OP_END 0x0B

#define WASM_SEC_TYPE 1
#define WASM_SEC_IMPORT 2
#define WASM_SEC_FUNCTION 3
#define WASM_SEC_MEMORY 5
#define WASM_SEC_EXPORT 7
#define WASM_SEC_CODE 10
#define WASM_SEC_DATA 11

#define WASM_TYPE_FUNC 0x60
#define WASM_TYPE_I32 0x7F
#define WASM_TYPE_I64 0x7E

#define WASM_EXPORT_FUNC 0x00
#define WASM_EXPORT_MEMORY 0x02
#define WASM_IMPORT_FUNC 0x00

#define WASM_FN_PATH_OPEN 0
#define WASM_FN_PATH_FILESTAT_GET 1
#define WASM_FN_FD_READDIR 2
#define WASM_FN_FD_CLOSE 3
#define WASM_TYPE_GUEST 4
#define WASM_TYPE_INIT 5
#define WASM_NUM_IMPORTS 4

#define WASM_SLOT_FD 0
#define WASM_SLOT_USED 8
#define WASM_SLOT_STAT 16
#define WASM_SLOT_READDIR 256
#define WASM_SLOT_READDIR_LEN 512
#define WASM_DATA_BASE 1024

#define WASM_PREOPEN_BASE_FD 3

#define WASI_OFLAGS_CREAT 0x1
#define WASI_OFLAGS_DIRECTORY 0x2
#define WASI_RIGHTS_FD_READ 0x2
#define WASI_RIGHTS_FD_WRITE 0x40
#define WASI_RIGHTS_PATH_OPEN 0x2000

typedef struct {
  u32 offset;
  u32 len;
} wasm_path_t;

static void put_u8(sp_io_writer_t* w, u8 v) {
  sp_io_write_c8(w, (c8)v);
}

static void put_bytes(sp_io_writer_t* w, const void* data, u64 len) {
  sp_io_write(w, data, len, SP_NULLPTR);
}

static void put_uleb(sp_io_writer_t* w, u64 v) {
  do {
    u8 byte = v & 0x7F;
    v >>= 7;
    if (v) {
      byte |= 0x80;
    }
    put_u8(w, byte);
  } while (v);
}

static void put_sleb(sp_io_writer_t* w, s64 v) {
  while (true) {
    u8 byte = v & 0x7F;
    v >>= 7;
    bool done = (v == 0 && !(byte & 0x40)) || (v == -1 && (byte & 0x40));
    if (!done) {
      byte |= 0x80;
    }
    put_u8(w, byte);
    if (done) {
      return;
    }
  }
}

static void put_name(sp_io_writer_t* w, const c8* name) {
  u64 len = sp_cstr_len(name);
  put_uleb(w, len);
  put_bytes(w, name, len);
}

static void put_section(sp_io_writer_t* out, u8 id, sp_io_dyn_mem_writer_t* body) {
  sp_str_t bytes = sp_io_dyn_mem_writer_as_str(body);
  put_u8(out, id);
  put_uleb(out, bytes.len);
  put_bytes(out, bytes.data, bytes.len);
}

static void put_functype(sp_io_writer_t* w, const u8* params, u32 count, bool result) {
  put_u8(w, WASM_TYPE_FUNC);
  put_uleb(w, count);
  put_bytes(w, params, count);
  put_uleb(w, result ? 1 : 0);
  if (result) {
    put_u8(w, WASM_TYPE_I32);
  }
}

static void put_import(sp_io_writer_t* w, const c8* name, u32 type) {
  put_name(w, "wasi_snapshot_preview1");
  put_name(w, name);
  put_u8(w, WASM_IMPORT_FUNC);
  put_uleb(w, type);
}

static void put_i32_const(sp_io_writer_t* w, s64 v) {
  put_u8(w, WASM_OP_I32_CONST);
  put_sleb(w, v);
}

static void put_i64_const(sp_io_writer_t* w, s64 v) {
  put_u8(w, WASM_OP_I64_CONST);
  put_sleb(w, v);
}

static void put_call(sp_io_writer_t* w, u32 fn) {
  put_u8(w, WASM_OP_CALL);
  put_uleb(w, fn);
}

static void put_fd_load(sp_io_writer_t* w) {
  put_i32_const(w, WASM_SLOT_FD);
  put_u8(w, WASM_OP_I32_LOAD);
  put_uleb(w, 2);
  put_uleb(w, 0);
}

static void put_path_open(sp_io_writer_t* w, wasm_path_t path, s64 oflags, s64 rights, s64 inherit) {
  put_i32_const(w, 0);
  put_i32_const(w, path.offset);
  put_i32_const(w, path.len);
  put_i32_const(w, oflags);
  put_i64_const(w, rights);
  put_i64_const(w, inherit);
  put_i32_const(w, 0);
  put_i32_const(w, WASM_SLOT_FD);
  put_call(w, WASM_FN_PATH_OPEN);
}

static void put_op(sp_io_writer_t* w, const wasm_emit_op_t* op, wasm_path_t path) {
  switch (op->kind) {
    case WASM_EMIT_OPEN_READ: {
      put_i32_const(w, WASM_PREOPEN_BASE_FD + op->mount);
      put_path_open(w, path, 0, WASI_RIGHTS_FD_READ, 0);
      break;
    }
    case WASM_EMIT_OPEN_WRITE: {
      put_i32_const(w, WASM_PREOPEN_BASE_FD + op->mount);
      put_path_open(w, path, WASI_OFLAGS_CREAT, WASI_RIGHTS_FD_WRITE, 0);
      break;
    }
    case WASM_EMIT_OPEN_DIR: {
      put_i32_const(w, WASM_PREOPEN_BASE_FD + op->mount);
      put_path_open(w, path, WASI_OFLAGS_DIRECTORY, WASI_RIGHTS_PATH_OPEN, WASI_RIGHTS_FD_READ);
      break;
    }
    case WASM_EMIT_OPEN_AT: {
      put_fd_load(w);
      put_path_open(w, path, 0, WASI_RIGHTS_FD_READ, 0);
      break;
    }
    case WASM_EMIT_STAT: {
      put_i32_const(w, WASM_PREOPEN_BASE_FD + op->mount);
      put_i32_const(w, 0);
      put_i32_const(w, path.offset);
      put_i32_const(w, path.len);
      put_i32_const(w, WASM_SLOT_STAT);
      put_call(w, WASM_FN_PATH_FILESTAT_GET);
      break;
    }
    case WASM_EMIT_READDIR: {
      put_i32_const(w, WASM_PREOPEN_BASE_FD + op->mount);
      put_i32_const(w, WASM_SLOT_READDIR);
      put_i32_const(w, WASM_SLOT_READDIR_LEN);
      put_i64_const(w, 0);
      put_i32_const(w, WASM_SLOT_USED);
      put_call(w, WASM_FN_FD_READDIR);
      break;
    }
    case WASM_EMIT_CLOSE: {
      put_fd_load(w);
      put_call(w, WASM_FN_FD_CLOSE);
      break;
    }
    case WASM_EMIT_NONE: {
      sp_unreachable_case();
    }
  }
}

static sp_io_dyn_mem_writer_t section_new(sp_mem_t mem) {
  sp_io_dyn_mem_writer_t w = sp_zero;
  sp_io_dyn_mem_writer_init(mem, &w);
  return w;
}

sp_str_t wasm_emit_module(sp_mem_t mem, const wasm_emit_fn_t* fns, u32 count) {
  sp_io_dyn_mem_writer_t data = section_new(mem);
  sp_da(wasm_path_t) paths = sp_da_new(mem, wasm_path_t);
  sp_for(fi, count) {
    sp_for(oi, fns[fi].count) {
      wasm_path_t path = sp_zero;
      const c8* str = fns[fi].ops[oi].path;
      if (str) {
        path.offset = WASM_DATA_BASE + (u32)sp_io_dyn_mem_writer_as_str(&data).len;
        path.len = (u32)sp_cstr_len(str);
        put_bytes(&data.base, str, path.len);
      }
      sp_da_push(paths, path);
    }
  }

  sp_io_dyn_mem_writer_t out = section_new(mem);
  const u8 header [] = { 0x00, 0x61, 0x73, 0x6D, 0x01, 0x00, 0x00, 0x00 };
  put_bytes(&out.base, header, sizeof(header));

  {
    const u8 params_path_open [] = {
      WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32,
      WASM_TYPE_I64, WASM_TYPE_I64, WASM_TYPE_I32, WASM_TYPE_I32
    };
    const u8 params_stat [] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32 };
    const u8 params_readdir [] = { WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I32, WASM_TYPE_I64, WASM_TYPE_I32 };
    const u8 params_close [] = { WASM_TYPE_I32 };

    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, 6);
    put_functype(&s.base, params_path_open, sp_carr_len(params_path_open), true);
    put_functype(&s.base, params_stat, sp_carr_len(params_stat), true);
    put_functype(&s.base, params_readdir, sp_carr_len(params_readdir), true);
    put_functype(&s.base, params_close, sp_carr_len(params_close), true);
    put_functype(&s.base, SP_NULLPTR, 0, true);
    put_functype(&s.base, SP_NULLPTR, 0, false);
    put_section(&out.base, WASM_SEC_TYPE, &s);
  }

  {
    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, WASM_NUM_IMPORTS);
    put_import(&s.base, "path_open", WASM_FN_PATH_OPEN);
    put_import(&s.base, "path_filestat_get", WASM_FN_PATH_FILESTAT_GET);
    put_import(&s.base, "fd_readdir", WASM_FN_FD_READDIR);
    put_import(&s.base, "fd_close", WASM_FN_FD_CLOSE);
    put_section(&out.base, WASM_SEC_IMPORT, &s);
  }

  {
    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, count + 1);
    sp_for(fi, count) {
      put_uleb(&s.base, WASM_TYPE_GUEST);
    }
    put_uleb(&s.base, WASM_TYPE_INIT);
    put_section(&out.base, WASM_SEC_FUNCTION, &s);
  }

  {
    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, 1);
    put_uleb(&s.base, 0);
    put_uleb(&s.base, 1);
    put_section(&out.base, WASM_SEC_MEMORY, &s);
  }

  {
    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, 2 + count);
    put_name(&s.base, "memory");
    put_u8(&s.base, WASM_EXPORT_MEMORY);
    put_uleb(&s.base, 0);
    put_name(&s.base, "_initialize");
    put_u8(&s.base, WASM_EXPORT_FUNC);
    put_uleb(&s.base, WASM_NUM_IMPORTS + count);
    sp_for(fi, count) {
      put_name(&s.base, fns[fi].name);
      put_u8(&s.base, WASM_EXPORT_FUNC);
      put_uleb(&s.base, WASM_NUM_IMPORTS + fi);
    }
    put_section(&out.base, WASM_SEC_EXPORT, &s);
  }

  {
    sp_io_dyn_mem_writer_t s = section_new(mem);
    put_uleb(&s.base, count + 1);
    u32 next = 0;
    sp_for(fi, count) {
      sp_assert(fns[fi].count);
      sp_io_dyn_mem_writer_t code = section_new(mem);
      put_uleb(&code.base, 0);
      sp_for(oi, fns[fi].count) {
        put_op(&code.base, &fns[fi].ops[oi], paths[next++]);
        if (oi + 1 < fns[fi].count) {
          put_u8(&code.base, WASM_OP_DROP);
        }
      }
      put_u8(&code.base, WASM_OP_END);
      sp_str_t body = sp_io_dyn_mem_writer_as_str(&code);
      put_uleb(&s.base, body.len);
      put_bytes(&s.base, body.data, body.len);
    }
    put_uleb(&s.base, 2);
    put_uleb(&s.base, 0);
    put_u8(&s.base, WASM_OP_END);
    put_section(&out.base, WASM_SEC_CODE, &s);
  }

  {
    sp_str_t bytes = sp_io_dyn_mem_writer_as_str(&data);
    if (bytes.len) {
      sp_io_dyn_mem_writer_t s = section_new(mem);
      put_uleb(&s.base, 1);
      put_uleb(&s.base, 0);
      put_i32_const(&s.base, WASM_DATA_BASE);
      put_u8(&s.base, WASM_OP_END);
      put_uleb(&s.base, bytes.len);
      put_bytes(&s.base, bytes.data, bytes.len);
      put_section(&out.base, WASM_SEC_DATA, &s);
    }
  }

  return sp_io_dyn_mem_writer_as_str(&out);
}
