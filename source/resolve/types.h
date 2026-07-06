#ifndef SPN_RESOLVE_TYPES_H
#define SPN_RESOLVE_TYPES_H

#include "sp.h"
#include "spn.h"

#include "forward/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "pkg/types.h"
#include "semver/types.h"
#include "session/registry/types.h"

// A link unit is one final linked artifact and everything resolved into it: an
// executable, a shared library, or a build-time tool. The zero value is the
// root unit. Packages resolve per-unit; two units may hold the same package at
// different versions.
typedef struct {
  sp_intern_id_t root;
  spn_triple_t triple;
} spn_link_unit_id_t;

// A resolved dep edge, materialized to the exact instance the dependency
// resolved to in the consumer's scope
typedef struct {
  spn_pkg_id_t id;
  spn_dep_kind_t kind;
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
  sp_da(spn_link_unit_id_t) units;
  union {
    struct { spn_index_rel_t* release; } index;
    struct { sp_str_t path; } file;
    struct { spn_pkg_info_t* info; } root;
  };
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
  spn_linkage_t linkage;
  sp_da(spn_pkg_config_entry_t) config;
  u64 budget;
} spn_resolver_t;

#endif
