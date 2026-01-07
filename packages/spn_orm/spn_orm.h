#ifndef SPN_ORM_H
#define SPN_ORM_H

#include "spn.h"
#include "spn_schema.h"
#include "spn_log.h"

static inline void spn_orm_init(spn_build_ctx_t* ctx) {
  spn_schema_init(ctx);
  spn_log_init(ctx);
  spn_log(ctx, "orm");
}

#endif
