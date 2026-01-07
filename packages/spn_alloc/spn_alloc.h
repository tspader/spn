#ifndef SPN_ALLOC_H
#define SPN_ALLOC_H

#include "spn.h"
#include "spn_core.h"

static inline void spn_alloc(spn_build_ctx_t* ctx) {
  spn_core(ctx);
  spn_log(ctx, "alloc");
}

#endif
