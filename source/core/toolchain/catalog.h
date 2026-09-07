#ifndef SPN_TOOLCHAIN_CATALOG_H
#define SPN_TOOLCHAIN_CATALOG_H

#include "common.gen.h"
#include "toolchain/types.h"

spn_toolchain_linkers_t spn_toolchain_linkers_load(const spn_cg_linkers_t* in);
spn_err_t             spn_toolchain_decls_parse(sp_mem_t mem, sp_str_t json, sp_da(spn_toolchain_decl_t)* decls);
void                  spn_toolchain_catalog_init(spn_toolchain_catalog_t* catalog, spn_triple_t host, sp_mem_t mem);
spn_err_t             spn_toolchain_catalog_load(spn_toolchain_catalog_t* catalog, sp_str_t json);
void                  spn_toolchain_catalog_add(spn_toolchain_catalog_t* catalog, spn_toolchain_decl_t decl);
spn_toolchain_info_t* spn_toolchain_catalog_get(spn_toolchain_catalog_t* catalog, sp_str_t name);

#endif
