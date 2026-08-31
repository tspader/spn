#include "external/wasm/wasm.h"

// Script handles are plain data until they are opened. Keeping their
// initializer out of wasm.c means a caller can construct one without linking
// the WAMR runtime, which is what lets unit/target.c be tested on its own.
void spn_wasm_script_init(spn_wasm_script_t* script, sp_str_t module) {
  *script = (spn_wasm_script_t) {
    .state = SPN_WASM_SCRIPT_CLOSED,
    .path = module,
  };
}
