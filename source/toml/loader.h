#ifndef SPN_TOML_LOADER_H
#define SPN_TOML_LOADER_H

#include "codegen/types.h"
#include "intern/types.h"

void      spn_toml_loader_init(spn_toml_loader_t* t, sp_mem_t mem, sp_intern_t* intern);
spn_err_t spn_codegen_load(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_manifest_t* out);
spn_err_t spn_codegen_load_config(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_config_t* out);
spn_err_union_t spn_codegen_err(spn_toml_loader_t* ctx);

#endif
