#ifndef SPN_WASM_TYPES_H
#define SPN_WASM_TYPES_H

#include "dag/types.h"
#include "core/types.h"
#include "spn/core.h"

typedef enum {
  SPN_ABI_KIND_NONE = 0,
  SPN_ABI_KIND_CTX,
  SPN_ABI_KIND_CONFIG,
  SPN_ABI_KIND_TARGET,
  SPN_ABI_KIND_NODE,
  SPN_ABI_KIND_PROFILE,
} spn_abi_kind_t;

typedef struct WASMModuleCommon spn_wasm_module_t;
typedef struct WASMModuleInstanceCommon spn_wasm_instance_t;
typedef struct WASMExecEnv spn_wasm_exec_env_t;
typedef struct spn_wasm_handles_t spn_wasm_handles_t;
typedef struct spn_dag_wasi_t spn_dag_wasi_t;

typedef struct {
  sp_mem_t mem;
  sp_da(spn_dag_obs_t)* out;
} spn_wasm_obs_t;

typedef union {
  struct {
    const c8* work;
    const c8* source;
    const c8* manifest;
    const c8* store;
  };
  const c8* array [4];
} spn_wasm_preopens_t;

typedef enum {
  SPN_WASM_SCRIPT_NONE = 0,
  SPN_WASM_SCRIPT_CLOSED,
  SPN_WASM_SCRIPT_OPEN,
  SPN_WASM_SCRIPT_FAILED,
} spn_wasm_script_state_t;

typedef struct {
  spn_wasm_script_state_t state;
  spn_err_t err;
  sp_str_t path;
  sp_mutex_t mutex;
  spn_wasm_module_t* module;
  spn_wasm_instance_t* instance;
  spn_wasm_exec_env_t* env;
  spn_wasm_handles_t* handles;
  spn_dag_wasi_t* wasi;
  u32 ctx;
  spn_wasm_preopens_t preopens;
} spn_wasm_script_t;

#endif
