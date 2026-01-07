#ifndef SPN_LOG_H
#define SPN_LOG_H

#include "spn.h"
#include "spn_alloc.h"

static inline void spn_log_init(spn_build_ctx_t* ctx) {
  spn_alloc(ctx);
  spn_log(ctx, "log");
}

#endif
