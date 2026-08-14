#ifndef SPN_CODEGEN_LOWER_H
#define SPN_CODEGEN_LOWER_H

#include "codegen/codegen.h"
#include "sp.h"
#include "spn/core.h"
#include "index/types.h"
#include "spn/types.h"
#include "manifest.gen.h"
#include "pkg/types.h"

spn_err_t spn_pkg_lower(spn_toml_loader_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out);

spn_err_t spn_pkg_lower_patch_hashes(spn_toml_loader_t* ctx, spn_pkg_info_t* out);

spn_err_t spn_pkg_reject_patches(spn_toml_loader_t* ctx, spn_pkg_info_t* out);
spn_index_info_t spn_index_lower(spn_toml_loader_t* ctx, u32 at, spn_index_kind_t kind, const spn_cg_index_t* decl);

spn_err_t spn_codegen_load_pkg(spn_toml_loader_t* ctx, sp_str_t manifest, spn_pkg_info_t* out);

#endif
