#ifndef SPN_WASM_H
#define SPN_WASM_H

#include "forward/types.h"
#include "spn.h"

#include "external/wasm/abi.h"
#include "external/wasm/types.h"

spn_err_t spn_wasm_init();
void      spn_wasm_thread_exit();
void      spn_wasm_script_init(spn_wasm_script_t* script, sp_str_t module);
spn_err_t spn_wasm_script_open(spn_wasm_script_t* script, spn_pkg_unit_t* unit);
bool      spn_wasm_script_exports(spn_wasm_script_t* script, sp_str_t name);
spn_err_t spn_wasm_script_call(spn_wasm_script_t* script, spn_pkg_unit_t* unit, sp_str_t name, spn_abi_kind_t kind, void* arg);
spn_err_t spn_wasm_find_export(spn_pkg_unit_t* unit, sp_str_t name, spn_wasm_script_t** script);
spn_err_t spn_wasm_call_export(spn_pkg_unit_t* unit, sp_str_t name, spn_abi_kind_t kind, void* arg);

#endif
