#ifndef SPN_CODEGEN_LOWER_H
#define SPN_CODEGEN_LOWER_H

#include "codegen/codegen.h"
#include "pkg/types.h"

struct spn_cg_manifest;

void spn_pkg_lower(spn_codegen_ctx_t* ctx, const struct spn_cg_manifest* cg, spn_pkg_info_t* out);

#endif
