#include "toolchain/catalog.h"

spn_err_t spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, sp_str_t builtins_json, spn_triple_t host, sp_mem_t mem) {
  catalog->mem = mem;
  sp_str_ht_init(mem, catalog->entries);
  return SPN_ERROR;
}

void spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_t toolchain) {
}

spn_toolchain_t* spn_toolchain_catalog_get(spn_toolchain_catalog_t* catalog, sp_str_t name) {
  spn_toolchain_t** entry = sp_str_ht_get(catalog->entries, name);
  return entry ? *entry : SP_NULLPTR;
}

sp_opt_spn_artifact_t spn_toolchain_select_artifact(sp_da(spn_toolchain_host_t) hosts, spn_triple_t host) {
  sp_opt_spn_artifact_t result = sp_zero;
  return result;
}
