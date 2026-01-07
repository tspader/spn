#ifndef SPN_NET_H
#define SPN_NET_H

#include "spn.h"
#include "spn_log.h"
#include "spn_json.h"

static inline void spn_net_init(spn_build_ctx_t* ctx) {
  spn_log_init(ctx);
  spn_json_init(ctx);
  spn_log(ctx, "net");
}

#endif
