#include "toolchain/select.h"
#include "toolchain/catalog.h"

sp_str_t spn_toolchain_script_default(void) {
  return sp_str_lit("zig");
}

spn_triple_t spn_toolchain_script_target(void) {
  return (spn_triple_t) { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI, .abi = SPN_ABI_NONE };
}

bool spn_toolchain_supports(spn_toolchain_t* toolchain, spn_triple_t target, spn_triple_t host) {
  return false;
}

spn_toolchain_select_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_selection_t* out) {
  return (spn_toolchain_select_err_t) { .status = SPN_TOOLCHAIN_SELECT_ERR_UNKNOWN, .name = query.build };
}
