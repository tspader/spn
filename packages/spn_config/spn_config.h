#ifndef SPN_CONFIG_H
#define SPN_CONFIG_H

#include "spn.h"
#include "spn_json.h"

static inline void spn_config(spn_build_ctx_t* ctx) {
  spn_json(ctx);
  spn_log(ctx, "config");
}

#endif
