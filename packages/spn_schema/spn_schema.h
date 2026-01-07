#ifndef SPN_SCHEMA_H
#define SPN_SCHEMA_H

#include "spn.h"
#include "spn_json.h"

static inline void spn_schema(spn_build_ctx_t* ctx) {
  spn_json(ctx);
  spn_log(ctx, "schema");
}

#endif
