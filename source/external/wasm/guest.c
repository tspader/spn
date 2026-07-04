#include "sp.h"
#include "external/wasm/abi.h"
#include "unit/types.h"
#include "api/api.h"
#include "ctx/types.h"
#include "event/types.h"
#include "event/event.h"

#define SPN_GUEST_FMT_ARGS 4

static spn_pkg_unit_t* guest_unit(spn_wasm_ctx_t* abi) {
  return spn_api_unit(abi->handles->ctx);
}

static sp_str_t guest_path(spn_wasm_ctx_t* abi, sp_mem_t mem, spn_pkg_unit_t* unit, const c8* path) {
  struct { sp_str_t guest; sp_str_t host; } dirs [] = {
    { sp_str_lit("/work"),   unit->paths.work },
    { sp_str_lit("/source"), unit->paths.source },
    { sp_str_lit("/store"),  unit->paths.store },
  };

  sp_str_t str = sp_str_view(path);
  sp_carr_for(dirs, it) {
    sp_str_t guest = dirs[it].guest;
    if (!sp_str_starts_with(str, guest)) continue;
    if (str.len == guest.len) return dirs[it].host;
    if (str.data[guest.len] != '/') continue;
    return sp_fmt(mem, "{}{}", sp_fmt_str(dirs[it].host), sp_fmt_str(sp_str_sub(str, guest.len, str.len - guest.len))).value;
  }

  wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(mem, "{} is not under /work, /source, or /store", sp_fmt_cstr(path)));
  return sp_str_lit("");
}

static void guest_copy(spn_wasm_ctx_t* abi, const c8* name, const c8* from, const c8* to) {
  spn_pkg_unit_t* unit = guest_unit(abi);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t from_path = guest_path(abi, scratch.mem, unit, from);
  sp_str_t to_path = guest_path(abi, scratch.mem, unit, to);
  if (sp_str_empty(from_path) || sp_str_empty(to_path)) {
    sp_mem_end_scratch(scratch);
    return;
  }

  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_API_CALL,
    .api_call = { .fn = sp_cstr_as_str(name), .args = sp_fmt(spn.mem, "{} -> {}", SP_FMT_STR(from_path), SP_FMT_STR(to_path)).value },
  });

  if (spn_api_copy(from_path, to_path)) {
    wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "{}: {} -> {}", SP_FMT_CSTR(name), SP_FMT_STR(from_path), SP_FMT_STR(to_path)));
  }
  sp_mem_end_scratch(scratch);
}

void spn_abi_fs_copy(spn_wasm_ctx_t* abi, const c8* from, const c8* to) {
  guest_copy(abi, "spn_fs_copy", from, to);
}

void spn_abi_fs_copy_glob(spn_wasm_ctx_t* abi, const c8* glob, const c8* dir) {
  guest_copy(abi, "spn_fs_copy_glob", glob, dir);
}

void spn_abi_fs_create_dir(spn_wasm_ctx_t* abi, const c8* path) {
  spn_pkg_unit_t* unit = guest_unit(abi);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t dir = guest_path(abi, scratch.mem, unit, path);
  if (sp_str_empty(dir)) {
    sp_mem_end_scratch(scratch);
    return;
  }
  SPN_API_LOG(unit, "spn_fs_create_dir", "{}", SP_FMT_STR(dir));

  if (sp_fs_create_dir(dir)) {
    wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "spn_fs_create_dir: {}", SP_FMT_STR(dir)));
  }
  sp_mem_end_scratch(scratch);
}

void spn_abi_fs_cat(spn_wasm_ctx_t* abi, const c8* path, const c8* a0, const c8* a1, const c8* a2, const c8* a3) {
  spn_pkg_unit_t* unit = guest_unit(abi);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t dst = guest_path(abi, scratch.mem, unit, path);
  if (sp_str_empty(dst)) {
    sp_mem_end_scratch(scratch);
    return;
  }
  SPN_API_LOG(unit, "spn_fs_cat", "{}", SP_FMT_STR(dst));

  sp_str_t parent = sp_fs_parent_path(dst);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, dst)) {
    wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "spn_fs_cat: {}", SP_FMT_STR(dst)));
    sp_mem_end_scratch(scratch);
    return;
  }

  const c8* srcs [] = { a0, a1, a2, a3 };
  sp_carr_for(srcs, it) {
    if (!srcs[it]) {
      break;
    }

    sp_str_t src = guest_path(abi, scratch.mem, unit, srcs[it]);
    if (sp_str_empty(src)) {
      break;
    }

    sp_str_t data = sp_zero;
    if (sp_io_read_file(scratch.mem, src, &data)) {
      wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "spn_fs_cat: {} -> {}", SP_FMT_STR(src), SP_FMT_STR(dst)));
      break;
    }
    sp_io_write_str(&writer.base, data, SP_NULLPTR);
  }

  sp_io_file_writer_close(&writer);
  sp_mem_end_scratch(scratch);
}

void spn_abi_io_write(spn_wasm_ctx_t* abi, const c8* path, const c8* contents) {
  spn_pkg_unit_t* unit = guest_unit(abi);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t dst = guest_path(abi, scratch.mem, unit, path);
  if (sp_str_empty(dst)) {
    sp_mem_end_scratch(scratch);
    return;
  }
  SPN_API_LOG(unit, "spn_io_write", "{}", SP_FMT_STR(dst));

  sp_str_t parent = sp_fs_parent_path(dst);
  if (!sp_str_empty(parent)) {
    sp_fs_create_dir(parent);
  }

  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, dst)) {
    wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "spn_io_write: {}", SP_FMT_STR(dst)));
    sp_mem_end_scratch(scratch);
    return;
  }

  sp_io_write_cstr(&writer.base, contents, SP_NULLPTR);
  sp_io_file_writer_close(&writer);
  sp_mem_end_scratch(scratch);
}

const c8* spn_abi_fmt(spn_wasm_ctx_t* abi, const c8* fmt, const c8* a0, const c8* a1, const c8* a2, const c8* a3) {
  const c8* args [SPN_GUEST_FMT_ARGS] = { a0, a1, a2, a3 };

  sp_io_dyn_mem_writer_t io = sp_zero;
  sp_io_dyn_mem_writer_init(spn.mem, &io);

  u32 next = 0;
  for (const c8* it = fmt; *it;) {
    if (it[0] == '{' && it[1] == '{') {
      sp_io_write_c8(&io.base, '{');
      it += 2;
      continue;
    }
    if (it[0] == '}' && it[1] == '}') {
      sp_io_write_c8(&io.base, '}');
      it += 2;
      continue;
    }
    if (it[0] == '{' && it[1] == '}') {
      if (next >= SPN_GUEST_FMT_ARGS || !args[next]) {
        wasm_runtime_set_exception(abi->instance, "spn_fmt: more placeholders than arguments");
        return SP_NULLPTR;
      }
      sp_io_write_cstr(&io.base, args[next++], SP_NULLPTR);
      it += 2;
      continue;
    }
    if (it[0] == '{' || it[0] == '}') {
      wasm_runtime_set_exception(abi->instance, "spn_fmt: bad placeholder; only {} is supported");
      return SP_NULLPTR;
    }
    sp_io_write_c8(&io.base, *it);
    it++;
  }

  sp_io_write_c8(&io.base, '\0');
  return sp_io_dyn_mem_writer_as_cstr(&io);
}
