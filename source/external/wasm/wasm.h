#ifndef SPN_WASM_H
#define SPN_WASM_H

#include "forward/types.h"
#include "spn.h"

spn_err_t spn_wasm_smoke(sp_mem_t mem, sp_intern_t* interner, sp_str_t path);
spn_err_t spn_wasm_init_stupid_global_runtime();
spn_err_t spn_wasm_run_configure(spn_pkg_unit_t* unit);

#endif
