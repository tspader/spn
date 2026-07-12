#ifndef SPN_SESSION_TYPES_H
#define SPN_SESSION_TYPES_H

#include "sp.h"

#include "compiler/types.h"
#include "git/types.h"
#include "filter/types.h"
#include "graph/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "pkg/options.h"
#include "profile/types.h"
#include "resolve/types.h"
#include "session/registry/types.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "toolchain/select.h"
#include "unit/types.h"

typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  spn_pkg_source_t source;
  spn_pkg_info_t* info;
  struct {
    sp_str_t recipe;
    sp_str_t source;
  } roots;
  struct {
    sp_str_t manifest;
    sp_str_t script;
  } paths;
  spn_target_info_t configure;
  spn_target_info_t build;
  u64 elapsed;
} spn_loaded_pkg_t;

typedef struct {
  spn_build_graph_t graph;
  spn_bg_dirty_t *dirty;
  spn_bg_executor_t *executor;
} spn_bg_ctx_t;

typedef enum {
  SPN_ACTION_NONE,
  SPN_ACTION_RUN_ROOTS,
  SPN_ACTION_RUN_SOURCE,
} spn_action_kind_t;

typedef struct {
  spn_action_kind_t kind;
  struct {
    sp_str_t path;
  } source;
} spn_action_t;

typedef struct {
  spn_compile_request_t compile;
  bool force;
  spn_action_t action;
  spn_profile_info_t overrides;
} spn_app_config_t;


struct spn_session_t {
  sp_mem_t mem;
  sp_intern_t* intern;
  spn_pkg_info_t* pkg;
  spn_event_buffer_t* events;
  spn_git_cache_t* git;
  sp_env_t env;

  spn_profile_table_t profiles;
  spn_toolchain_catalog_t catalog;
  spn_toolchain_selection_t selection;

  spn_resolve_t resolve;
  spn_pkg_registry_t registry;
  sp_ht(spn_pkg_id_t, spn_loaded_pkg_t) packages;
  sp_ht(spn_pkg_id_t, spn_resolved_options_t) options;
  sp_ht(spn_pkg_unit_id_t, sp_hash_t) fingerprints;

  struct {
    spn_option_seeds_t seeds;
    u32 resolves;
    bool reresolve;
  } gates;

  spn_profile_info_t profile;
  struct {
    spn_compile_request_t request;
    sp_da(spn_pkg_id_t) requested;
    sp_da(spn_build_plan_t) builds;
  } plan;
  struct {
    sp_da(spn_build_unit_t*) builds;
    spn_build_unit_t* target;
    spn_build_unit_t* program;
    sp_om(spn_compile_unit_id_t, spn_compile_unit_t) objects;
    sp_da(spn_compile_command_t) compile_commands;
    sp_om(spn_target_unit_id_t, spn_target_unit_t) targets;
    sp_om(spn_pkg_unit_id_t, spn_pkg_unit_t) packages;
    spn_toolchain_unit_t* toolchain;
    spn_toolchain_unit_t* script;
    sp_da(spn_toolchain_unit_t*) toolchains;
  } units;

  struct {
    sp_str_t root;
    sp_str_t build;
    spn_system_paths_t system;
  } paths;

  spn_bg_ctx_t sync;
  spn_bg_ctx_t configure;
  spn_bg_ctx_t build;
  sp_ht(spn_bg_id_t, spn_build_unit_id_t) owners;
  sp_mutex_t mutex;
};

#endif
