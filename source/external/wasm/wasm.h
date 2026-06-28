#ifndef SPN_WASM_H
#define SPN_WASM_H

#include "spn.h"
#include "forward/types.h"

spn_err_t spn_wasm_smoke(sp_mem_t mem, sp_intern_t* interner, const c8* path);
spn_err_t spn_wasm_init_stupid_global_runtime();

#endif
