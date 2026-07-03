#ifndef SPN_WASM_ABI_H
#define SPN_WASM_ABI_H

#include <stdbool.h>
#include "sp.h"
#include "spn.h"
#include "wasm_export.h"

#include "external/wasm/types.h"

typedef enum {
  SPN_ABI_KIND_NONE = 0,
  SPN_ABI_KIND_CTX,
  SPN_ABI_KIND_TARGET,
  SPN_ABI_KIND_NODE,
  SPN_ABI_KIND_PROFILE,
} spn_abi_kind_t;

typedef struct {
  void* ptr;
  spn_abi_kind_t kind;
} spn_abi_entry_t;

struct spn_abi_table_t {
  sp_da(spn_abi_entry_t) entries;
};

typedef struct {
  wasm_module_inst_t inst;
  spn_abi_table_t* table;
} spn_abi_ctx_t;

spn_abi_ctx_t spn_abi_ctx(wasm_exec_env_t env);
void        spn_abi_table_init(spn_abi_table_t* table, sp_mem_t mem);
u32         spn_abi_table_add(spn_abi_table_t* table, void* ptr, spn_abi_kind_t kind);
void*       spn_abi_table_get(spn_abi_table_t* table, u32 token, spn_abi_kind_t kind);
const void* spn_abi_deref(spn_abi_ctx_t* abi, u32 addr, u64 size);
bool        spn_abi_read_str(spn_abi_ctx_t* abi, u32 offset, const c8** out);
bool        spn_abi_read_handle(spn_abi_ctx_t* abi, u32 token, spn_abi_kind_t kind, void** out);
u32         spn_abi_str_to_guest(spn_abi_ctx_t* abi, const c8* str);
void        spn_abi_node_set_fn(spn_node_t* node, u32 fn);
bool        spn_abi_register(void);

#endif
