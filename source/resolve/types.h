#ifndef SPN_RESOLVE_TYPES_H
#define SPN_RESOLVE_TYPES_H

#include "sp.h"
#include "spn.h"

#include "index/cache.h"
#include "pkg/types.h"

typedef enum {
  SPN_RESOLVE_STRATEGY_LOCK_FILE,
  SPN_RESOLVE_STRATEGY_SOLVER,
} spn_resolve_strategy_t;

typedef struct {
  spn_pkg_id_t id;
  spn_semver_t version;
  struct {
    sp_da(sp_str_t) system;
    sp_da(spn_requested_pkg_t) pkg;
  } deps;
} spn_resolve_node_t;

typedef struct {
  spn_index_rel_t* release;
  spn_semver_t version;
} spn_resolved_pkg_t;

typedef struct spn_event_buffer_t spn_event_buffer_t;

typedef struct spn_resolver_t {
  spn_pkg_info_t* pkg;
  spn_index_cache_t* index;
  spn_event_buffer_t* events;
  sp_str_ht(bool) visited;
  sp_str_ht(spn_resolved_pkg_t) resolved;
  sp_da(sp_str_t) system_deps;
  sp_da(sp_str_t) resolution_order;
  sp_da(spn_requested_pkg_t) reqs;
  sp_tm_timer_t timer;
} spn_resolver_t;

#endif
