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
  SPN_ABI_KIND_CONFIG,
  SPN_ABI_KIND_TARGET,
  SPN_ABI_KIND_NODE,
  SPN_ABI_KIND_NODE_CTX,
  SPN_ABI_KIND_PROFILE,
  SPN_ABI_KIND_MAKE,
  SPN_ABI_KIND_AUTOCONF,
  SPN_ABI_KIND_CMAKE,
} spn_abi_kind_t;

typedef struct {
  void* ptr;
  spn_abi_kind_t kind;
} spn_wasm_handle_t;

struct spn_wasm_handles_t {
  sp_ht(u32, spn_wasm_handle_t) map;
  u32 next;
  spn_t* ctx;
};

typedef struct {
  wasm_module_inst_t instance;
  spn_wasm_handles_t* handles;
} spn_wasm_ctx_t;

typedef u32 spn_wasm_ptr_t;

spn_wasm_ctx_t spn_wasm_ctx(wasm_exec_env_t env);
u32            spn_wasm_add_handle(spn_wasm_handles_t* handles, void* ptr, spn_abi_kind_t kind);
void           spn_wasm_remove_handle(spn_wasm_handles_t* handles, u32 token);
void*          spn_wasm_resolve_handle(spn_wasm_handles_t* handles, u32 token, spn_abi_kind_t kind);
const void*    spn_wasm_resolve_guest_ptr(spn_wasm_ctx_t* abi, u32 addr, u64 size);
bool           spn_wasm_read_str(spn_wasm_ctx_t* abi, u32 offset, const c8** out);
bool           spn_wasm_read_handle(spn_wasm_ctx_t* abi, u32 token, spn_abi_kind_t kind, void** out);
spn_wasm_ptr_t spn_wasm_copy_str(spn_wasm_ctx_t* abi, const c8* str);
void        spn_abi_node_set_user_data(spn_node_t* node, s32 data);
s32         spn_abi_node_ctx_get_user_data(spn_node_ctx_t* ctx);
void        spn_abi_fs_copy(spn_wasm_ctx_t* abi, const c8* from, const c8* to);
void        spn_abi_fs_copy_glob(spn_wasm_ctx_t* abi, const c8* glob, const c8* dir);
void        spn_abi_fs_cat(spn_wasm_ctx_t* abi, const c8* path, const c8* a0, const c8* a1, const c8* a2, const c8* a3);
void        spn_abi_fs_create_dir(spn_wasm_ctx_t* abi, const c8* path);
void        spn_abi_io_write(spn_wasm_ctx_t* abi, const c8* path, const c8* contents);
const c8*   spn_abi_fmt(spn_wasm_ctx_t* abi, const c8* fmt, const c8* a0, const c8* a1, const c8* a2, const c8* a3);
bool        spn_wasm_register_api(void);

#endif
