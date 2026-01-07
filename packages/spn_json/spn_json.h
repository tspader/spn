#ifndef SPN_JSON_H
#define SPN_JSON_H

#include "spn.h"
#include "spn_simd.h"

static inline void spn_json(spn_build_ctx_t* ctx) {
  spn_simd(ctx);
  spn_log(ctx, "json");
}

#endif
