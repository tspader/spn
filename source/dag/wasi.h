#ifndef SPN_DAG_WASI_H
#define SPN_DAG_WASI_H

#include "sp.h"
#include "spn.h"
#include "dag/types.h"
#include "wasm_export.h"

typedef struct {
  const c8* guest;
  sp_str_t host;
} spn_dag_wasi_mount_t;

typedef struct spn_dag_wasi_t spn_dag_wasi_t;

spn_err_t       spn_dag_wasi_install(void);
spn_dag_wasi_t* spn_dag_wasi_new(sp_mem_t mem, const spn_dag_wasi_mount_t* mounts, u32 count);
void            spn_dag_wasi_bind(spn_dag_wasi_t* w, wasm_module_inst_t instance);
void            spn_dag_wasi_begin(spn_dag_wasi_t* w, sp_mem_t mem, sp_da(spn_dag_obs_t)* obs);
void            spn_dag_wasi_end(spn_dag_wasi_t* w);

#endif
