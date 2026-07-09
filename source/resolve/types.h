#ifndef SPN_RESOLVE_TYPES_H
#define SPN_RESOLVE_TYPES_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "profile/types.h"
#include "semver/types.h"
#include "session/registry/types.h"

typedef enum {
  SPN_DEP_EDGE_SCOPE,
  SPN_DEP_EDGE_PROCESS,
  SPN_DEP_EDGE_PRIVATE,
  SPN_DEP_EDGE_PRUNED,
} spn_dep_edge_t;

typedef struct {
  spn_pkg_id_t id;
  spn_dep_kind_t kind;
  spn_dep_edge_t edge;
  bool private;
} spn_resolved_dep_t;

typedef struct {
  spn_pkg_id_t id;
  sp_intern_str_t qualified;
  spn_pkg_source_t source;
  spn_semver_t version;
  u64 priority;
  sp_da(spn_requested_pkg_t) deps;
  sp_da(spn_resolved_dep_t) edges;
  struct {
    spn_pkg_tree_t recipe;
    spn_pkg_tree_t source;
    spn_index_rel_paths_t paths;
    spn_pkg_info_t* info;
    spn_index_rel_t* release;
  } origin;
} spn_resolved_pkg_t;

typedef sp_ht(spn_pkg_id_t, spn_resolved_pkg_t) spn_resolve_t;

typedef struct {
  sp_da(spn_requested_pkg_t) reqs;
  spn_resolve_t result;
  u64 time;
} spn_resolve_query_t;

typedef struct spn_resolver_t {
  sp_mem_t mem;
  sp_intern_t* intern;
  spn_index_cache_t* index;
  spn_pkg_registry_t* registry;
  spn_event_buffer_t* events;
  spn_profile_info_t profile;
  sp_da(spn_pkg_config_entry_t) config;
  u64 budget;
} spn_resolver_t;

#endif
