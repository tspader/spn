#ifndef SPN_SESSION_TYPES_H
#define SPN_SESSION_TYPES_H

#include "sp.h"
#include "spn.h"

#include "event/types.h"
#include "filter/types.h"
#include "graph/types.h"
#include "profile/types.h"
#include "semver/types.h"
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

typedef struct spn_toolchain {
  spn_toolchain_kind_t source;
  spn_toolchain_info_t info;
  spn_toolchain_launcher_t compiler;
  spn_toolchain_launcher_t linker;
  spn_toolchain_launcher_t archiver;
  spn_pkg_info_t* pkg;
} spn_toolchain_t;

typedef struct {
  spn_pkg_kind_t kind;
  spn_pkg_info_t* pkg;
  struct {
    sp_str_t manifest;
    sp_str_t script;
    sp_str_t source;
  } paths;
  u64 elapsed;
} spn_loaded_pkg_t;

struct spn_session_t {
  spn_pkg_info_t* pkg;
  spn_target_filter_t filter;
  sp_env_t env;
  spn_event_buffer_t* events;

  spn_profile_table_t profiles;
  sp_str_ht(spn_toolchain_entry_t) toolchains;
  sp_str_ht(spn_loaded_pkg_t) packages;

  spn_profile_info_t profile;
  spn_toolchain_t toolchain;
  struct {
    sp_om(spn_target_unit_t) targets;
    sp_om(spn_pkg_unit_t) packages;
    spn_toolchain_unit_t* toolchain;
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
