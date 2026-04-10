#ifndef SPN_RESOLVE_TYPES_H
#define SPN_RESOLVE_TYPES_H

#include "forward/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "semver/types.h"

typedef enum {
  SPN_RESOLVE_STRATEGY_LOCK_FILE,
  SPN_RESOLVE_STRATEGY_SOLVER,
} spn_resolve_strategy_t;

typedef struct {
  spn_pkg_id_t id;
  sp_intern_str_t qualified;
  spn_pkg_source_t source;
  spn_semver_t version;
  sp_da(spn_requested_pkg_t) deps;
  union {
    struct { spn_index_rel_t* release; } index;
    struct { sp_str_t path; } file;
    struct { spn_pkg_info_t* info; } root;
  };
} spn_resolved_pkg_t;

typedef struct spn_event_buffer_t spn_event_buffer_t;

typedef struct spn_resolver_t {
  spn_pkg_info_t* pkg;
  spn_index_cache_t* index;
  spn_event_buffer_t* events;
  sp_str_ht(u8) visited;
  sp_da(spn_requested_pkg_t) reqs;
  sp_str_ht(spn_resolved_pkg_t) packages;
  sp_tm_timer_t timer;
} spn_resolver_t;

#endif
