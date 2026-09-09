#ifndef SPN_CODEGEN_TOOLCHAIN_H
#define SPN_CODEGEN_TOOLCHAIN_H

#include "codegen/codegen.h"
#include "config.gen.h"
#include "paths/types.h"
#include "toolchain/types.h"

// Every [[toolchain]] file is read through here: the builtin catalog, the
// user config, a manifest, and the test lanes. Parse issues land in ctx.
void                 spn_toolchains_parse(spn_toml_loader_t* ctx, sp_str_t toml, spn_cg_config_t* out);
spn_toolchain_decl_t spn_toolchain_lower(spn_toml_loader_t* ctx, u32 at, spn_path_root_t base, const spn_cg_toolchain_decl_t* decl);

#endif
