#ifndef SPN_RESOLVE_H
#define SPN_RESOLVE_H

#include "sp.h"
#include "app.h"
#include "pkg.h"
#include "spn.h"


#define SPN_RESOLVE_STRATEGY(X) \
  X(SPN_RESOLVE_STRATEGY_LOCK_FILE, "lockfile") \
  X(SPN_RESOLVE_STRATEGY_SOLVER, "solver")

typedef enum {
  SPN_RESOLVE_STRATEGY(SP_X_NAMED_ENUM_DEFINE)
} spn_resolve_strategy_t;

typedef struct {
  sp_opt(u32) low;
  sp_opt(u32) high;
  spn_pkg_req_t source;
} spn_resolve_range_t;

typedef struct {
  spn_pkg_t* pkg;
  spn_pkg_kind_t kind;
  spn_semver_t version;
} spn_resolved_pkg_t;

typedef struct spn_resolver_t {
  spn_pkg_t* pkg;
  spn_pkg_cache_t* cache;
  sp_str_ht(sp_str_t)* registry;
  sp_str_ht(sp_da(spn_resolve_range_t)) ranges;
  sp_str_ht(bool) visited;
  sp_str_ht(spn_resolved_pkg_t) resolved;
  sp_da(sp_str_t) system_deps;
} spn_resolver_t;

void spn_resolver_init(spn_resolver_t* r, spn_pkg_t* pkg, spn_pkg_cache_t* cache);

spn_pkg_req_t          spn_pkg_req_from_str(sp_str_t str);
sp_str_t               spn_pkg_req_to_str(spn_pkg_req_t dep);
spn_resolve_strategy_t spn_resolve_strategy_from_str(sp_str_t str);
sp_str_t               spn_resolve_strategy_to_str(spn_resolve_strategy_t strategy);

#endif
