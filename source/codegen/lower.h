#ifndef SPN_CODEGEN_LOWER_H
#define SPN_CODEGEN_LOWER_H

#include "codegen/codegen.h"
#include "types.gen.h"
#include "pkg/types.h"

void spn_pkg_lower(spn_codegen_ctx_t* ctx, const spn_cg_manifest_t* cg, spn_pkg_info_t* out);

#endif
