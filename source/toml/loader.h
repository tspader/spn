#ifndef SPN_TOML_LOADER_H
#define SPN_TOML_LOADER_H

#include "codegen/types.h"
#include "forward/types.h"

void      spn_toml_loader_init(spn_toml_loader_t* t, sp_mem_t mem, sp_intern_t* intern);
spn_err_t spn_codegen_load(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_manifest_t* out);
spn_err_t spn_codegen_load_config(spn_toml_loader_t* ctx, sp_str_t path, spn_cg_config_t* out);
spn_err_union_t spn_codegen_err(spn_toml_loader_t* ctx);

spn_err_union_t spn_toml_load_manifest(sp_mem_t mem, sp_intern_t* intern, sp_str_t path, spn_pkg_info_t* plf);

#endif
