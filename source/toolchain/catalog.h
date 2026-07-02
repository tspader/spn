#ifndef SPN_TOOLCHAIN_CATALOG_H
#define SPN_TOOLCHAIN_CATALOG_H

#include "toolchain/types.h"

spn_err_t         spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, sp_str_t builtins_json, spn_triple_t host, sp_mem_t mem);
void              spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_t toolchain);
spn_toolchain_t*  spn_toolchain_catalog_get(spn_toolchain_catalog_t* catalog, sp_str_t name);

#endif
