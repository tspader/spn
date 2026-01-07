#ifndef SPN_CODEGEN_H
#define SPN_CODEGEN_H

#include "spn.h"
#include "spn_schema.h"

static inline void spn_codegen(spn_build_ctx_t* ctx) {
  spn_schema(ctx);
  spn_log(ctx, "codegen");
}

#endif
