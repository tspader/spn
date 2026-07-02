#include "sp.h"
#include "external/wasm/abi.h"

void spn_abi_table_init(spn_abi_table_t* table, sp_mem_t mem) {
  table->entries = sp_da_new(mem, spn_abi_entry_t);
}

u32 spn_abi_table_add(spn_abi_table_t* table, void* ptr, spn_abi_kind_t kind) {
  spn_abi_entry_t entry = { .ptr = ptr, .kind = kind };
  sp_da_push(table->entries, entry);
  return (u32)sp_da_size(table->entries);
}

void* spn_abi_table_get(spn_abi_table_t* table, u32 token, spn_abi_kind_t kind) {
  if (!token || token > sp_da_size(table->entries)) return SP_NULLPTR;
  spn_abi_entry_t* entry = &table->entries[token - 1];
  if (entry->kind != kind) return SP_NULLPTR;
  return entry->ptr;
}

const void* spn_abi_deref(spn_abi_ctx_t* abi, u32 addr, u64 size) {
  if (!addr) return SP_NULLPTR;
  if (!wasm_runtime_validate_app_addr(abi->inst, addr, size)) return SP_NULLPTR;
  return wasm_runtime_addr_app_to_native(abi->inst, addr);
}

bool spn_abi_read_str(spn_abi_ctx_t* abi, u32 offset, const c8** out) {
  *out = SP_NULLPTR;
  if (!offset) return true;
  if (!wasm_runtime_validate_app_str_addr(abi->inst, offset)) return false;
  *out = wasm_runtime_addr_app_to_native(abi->inst, offset);
  return true;
}

bool spn_abi_read_handle(spn_abi_ctx_t* abi, u32 token, spn_abi_kind_t kind, void** out) {
  *out = SP_NULLPTR;
  if (!token) return true;
  *out = spn_abi_table_get(abi->table, token, kind);
  return *out != SP_NULLPTR;
}

u32 spn_abi_thunk_get_target(wasm_exec_env_t env, u32 ctx, u32 name_addr) {
  spn_abi_ctx_t abi = {
    .inst = wasm_runtime_get_module_inst(env),
    .table = wasm_runtime_get_user_data(env),
  };

  spn_t* spn = spn_abi_table_get(abi.table, ctx, SPN_ABI_KIND_CTX);
  if (!spn) return 0;

  const c8* name = SP_NULLPTR;
  if (!spn_abi_read_str(&abi, name_addr, &name) || !name) return 0;

  spn_target_t* target = spn_get_target(spn, name);
  if (!target) return 0;

  return spn_abi_table_add(abi.table, target, SPN_ABI_KIND_TARGET);
}
