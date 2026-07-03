#ifndef SPN_WASM_H
#define SPN_WASM_H

#include "forward/types.h"
#include "spn.h"

#include "external/wasm/types.h"

bool      spn_wasm_enabled(void);
spn_err_t spn_wasm_smoke(sp_mem_t mem, sp_intern_t* interner, sp_str_t path);
spn_err_t spn_wasm_init_stupid_global_runtime();
spn_err_t spn_wasm_script_open(spn_wasm_script_t** out, spn_pkg_unit_t* unit, sp_str_t path);
bool      spn_wasm_script_has(spn_wasm_script_t* script, const c8* name);
bool      spn_wasm_script_has_node_fn(spn_wasm_script_t* script, spn_user_node_t* node);
spn_err_t spn_wasm_script_call_hook(spn_wasm_script_t* script, spn_pkg_unit_t* unit, const c8* name);
spn_err_t spn_wasm_script_call_node(spn_wasm_script_t* script, spn_user_node_t* node);

#endif
