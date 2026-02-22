#ifndef SPN_RESOLVE_H
#define SPN_RESOLVE_H

#include "sp.h"

#define SPN_RESOLVE_STRATEGY(X) \
  X(SPN_RESOLVE_STRATEGY_LOCK_FILE, "lockfile") \
  X(SPN_RESOLVE_STRATEGY_SOLVER, "solver")

typedef enum {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_DEFINE)
} spn_resolve_strategy_t;

spn_resolve_strategy_t spn_resolve_strategy_from_str(sp_str_t str);
sp_str_t spn_resolve_strategy_to_str(spn_resolve_strategy_t strategy);

#endif
