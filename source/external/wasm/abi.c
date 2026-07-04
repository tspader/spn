#include "sp.h"
#include "external/wasm/abi.h"
#include "unit/types.h"
#include "api/api.h"
#include "ctx/types.h"
#include "event/types.h"
#include "event/event.h"

spn_wasm_ctx_t spn_wasm_ctx(wasm_exec_env_t env) {
  return (spn_wasm_ctx_t) {
    .instance = wasm_runtime_get_module_inst(env),
    .handles = wasm_runtime_get_user_data(env),
  };
}

u32 spn_wasm_add_handle(spn_wasm_handles_t* table, void* ptr, spn_abi_kind_t kind) {
  spn_wasm_handle_t handle = { ptr, kind };
  u32 token = ++table->next;
  sp_ht_insert(table->map, token, handle);
  return token;
}

void* spn_wasm_resolve_handle(spn_wasm_handles_t* handles, u32 token, spn_abi_kind_t kind) {
  if (!token) return SP_NULLPTR;

  spn_wasm_handle_t* handle = sp_ht_getp(handles->map, token);
  if (!handle) return SP_NULLPTR;
  if (handle->kind != kind) return SP_NULLPTR;

  return handle->ptr;
}

const void* spn_wasm_resolve_guest_ptr(spn_wasm_ctx_t* wasm, u32 addr, u64 size) {
  if (!addr) return SP_NULLPTR;
  if (!wasm_runtime_validate_app_addr(wasm->instance, addr, size)) return SP_NULLPTR;
  return wasm_runtime_addr_app_to_native(wasm->instance, addr);
}

bool spn_wasm_read_str(spn_wasm_ctx_t* abi, u32 offset, const c8** out) {
  *out = SP_NULLPTR;
  if (!offset) return true;
  if (!wasm_runtime_validate_app_str_addr(abi->instance, offset)) return false;
  *out = wasm_runtime_addr_app_to_native(abi->instance, offset);
  return true;
}

bool spn_wasm_read_handle(spn_wasm_ctx_t* abi, u32 token, spn_abi_kind_t kind, void** out) {
  *out = SP_NULLPTR;
  if (!token) return true;
  *out = spn_wasm_resolve_handle(abi->handles, token, kind);
  return *out != SP_NULLPTR;
}

spn_wasm_ptr_t spn_wasm_copy_str(spn_wasm_ctx_t* abi, const c8* str) {
  if (!str) return 0;

  u64 size = sp_cstr_len(str) + 1;

  void* ptr = SP_NULLPTR;
  u64 offset = wasm_runtime_module_malloc(abi->instance, size, &ptr);
  if (!offset) return 0;

  sp_mem_copy(ptr, str, size);
  return sp_cast(spn_wasm_ptr_t, offset);
}

void spn_wasm_remove_handle(spn_wasm_handles_t* table, u32 token) {
  sp_ht_erase(table->map, token);
}

void spn_abi_node_set_user_data(spn_node_t* node, s32 data) {
  spn_find_user_node(node)->user_data = (void*)(u64)(u32)data;
}

s32 spn_abi_node_ctx_get_user_data(spn_node_ctx_t* ctx) {
  return (s32)(u32)(u64)ctx->user_data;
}

static sp_str_t spn_abi_fs_path(sp_mem_t mem, spn_pkg_unit_t* unit, const c8* path) {
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
  return str;
}

void spn_abi_fs_copy(spn_wasm_ctx_t* abi, const c8* from, const c8* to) {
  spn_pkg_unit_t* unit = spn_api_unit(abi->handles->ctx);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_str_t from_path = spn_abi_fs_path(scratch.mem, unit, from);
  sp_str_t to_path = spn_abi_fs_path(scratch.mem, unit, to);
  SPN_API_LOG(unit, "spn_fs_copy", "{} -> {}", sp_fmt_str(from_path), sp_fmt_str(to_path));

  if (sp_fs_copy(from_path, to_path)) {
    wasm_runtime_set_exception(abi->instance, sp_fmt_mem_cstr(scratch.mem, "spn_fs_copy: {} -> {}", sp_fmt_str(from_path), sp_fmt_str(to_path)));
  }
  sp_mem_end_scratch(scratch);
}
