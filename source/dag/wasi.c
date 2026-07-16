#include "dag/wasi.h"

struct spn_dag_wasi_t {
  sp_mem_t mem;
  wasm_module_inst_t instance;
  sp_da(spn_dag_obs_t)* obs;
};

spn_err_t spn_dag_wasi_install(void) {
  return SPN_OK;
}

spn_dag_wasi_t* spn_dag_wasi_new(sp_mem_t mem, const spn_dag_wasi_mount_t* mounts, u32 count) {
  spn_dag_wasi_t* w = sp_alloc_type(mem, spn_dag_wasi_t);
  w->mem = mem;
  w->instance = SP_NULLPTR;
  w->obs = SP_NULLPTR;
  return w;
}

void spn_dag_wasi_bind(spn_dag_wasi_t* w, wasm_module_inst_t instance) {
  w->instance = instance;
}

void spn_dag_wasi_begin(spn_dag_wasi_t* w, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs) {
  w->obs = obs;
}

void spn_dag_wasi_end(spn_dag_wasi_t* w) {
  w->obs = SP_NULLPTR;
}
