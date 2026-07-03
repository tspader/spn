#include "sp.h"
#include "external/wasm/abi.h"
#include "unit/types.h"

spn_wasm_ctx_t spn_wasm_ctx(wasm_exec_env_t env) {
  return (spn_wasm_ctx_t) {
    .instance = wasm_runtime_get_module_inst(env),
    .handles = wasm_runtime_get_user_data(env),
  };
}

void spn_abi_table_init(spn_wasm_handles_t* table, sp_mem_t mem) {
  table->next = 0;
  sp_ht_init(mem, table->map);
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

spn_node_t* spn_abi_add_node(spn_config_t* config, const c8* tag) {
  spn_node_t* node = spn_add_node(config, tag);
  spn_find_user_node(node)->wasm = true;
  return node;
}

void spn_abi_node_set_user_data(spn_node_t* node, s32 data) {
  spn_find_user_node(node)->user_data = (void*)(u64)(u32)data;
}

s32 spn_abi_node_ctx_get_user_data(spn_node_ctx_t* ctx) {
  return (s32)(u32)(u64)ctx->user_data;
}
