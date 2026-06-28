#ifndef SPN_WASM_TYPES_H
#define SPN_WASM_TYPES_H

#include "forward/types.h"
#include "sp/sp_om.h"

typedef struct wasm_store_t wasm_store_t;
typedef struct wasm_instance_t wasm_instance_t;
typedef struct wasm_trap_t wasm_trap_t;
typedef struct wasm_extern_vec_t wasm_extern_vec_t;
typedef struct WASMModuleCommon spn_wasm_module_t;
typedef struct WASMModuleInstanceCommon spn_wasm_module_instance_t;
typedef struct WASMExecEnv spn_wasm_ctx_t;
typedef struct WASMFunctionInstanceCommon spn_wasm_fn_t;

typedef struct {
  spn_wasm_ctx_t* ctx;
  spn_wasm_module_instance_t* instance;
  sp_om(sp_str_t, spn_wasm_fn_t*) functions;
} spn_wasm_pkg_t;

typedef struct {
  sp_mem_t mem;
  sp_intern_t* interner;
  wasm_instance_t* instance;
  wasm_trap_t* trap;
  wasm_extern_vec_t* exports;
  wasm_store_t* store;
  sp_om(sp_str_t, spn_wasm_module_t*) modules;
} spn_wasm_t;


#endif
