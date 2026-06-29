#ifndef SPN_SESSION_TYPES_H
#define SPN_SESSION_TYPES_H

#include "sp.h"

#include "git/types.h"
#include "filter/types.h"
#include "graph/types.h"
#include "intern/types.h"
#include "profile/types.h"
#include "resolve/types.h"
#include "session/registry/types.h"
#include "toolchain/types.h"
#include "unit/types.h"

typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  spn_build_graph_t graph;
  spn_bg_dirty_t *dirty;
  spn_bg_executor_t *executor;
} spn_bg_ctx_t;

typedef sp_om(spn_pkg_unit_id_t, sp_da(spn_pkg_unit_t*)) spn_unit_graph_t;

struct spn_session_t {
  sp_mem_t mem;
  sp_intern_t* intern;
  spn_pkg_info_t* pkg;
  spn_event_buffer_t* events;
  spn_git_cache_t* git;
  spn_target_filter_t filter;
  sp_env_t env;

  spn_profile_table_t profiles;
  sp_str_ht(spn_toolchain_entry_t) toolchains;

  spn_resolve_t resolve;
  spn_pkg_registry_t packages;

  spn_profile_info_t profile;
  struct {
    spn_unit_graph_t graph;
    sp_str_om(spn_compile_unit_t) objects;
    sp_om(spn_target_unit_id_t, spn_target_unit_t) targets;
    sp_om(spn_pkg_unit_id_t, spn_pkg_unit_t) packages;
    spn_toolchain_unit_t* toolchain;
    spn_toolchain_unit_t* zig;
  } units;

  struct {
    sp_str_t root;
    sp_str_t build;
    sp_str_t profile;
  } paths;

  spn_bg_ctx_t configure;
  spn_bg_ctx_t build;
  sp_mutex_t mutex;
};

#endif
