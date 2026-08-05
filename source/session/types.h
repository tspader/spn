#ifndef SPN_SESSION_TYPES_H
#define SPN_SESSION_TYPES_H

#include "sp.h"

#include "compiler/types.h"
#include "filter/types.h"
#include "forward/types.h"
#include "profile/types.h"
#include "resolve/types.h"
#include "session/registry/types.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "unit/types.h"

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

typedef enum {
  SPN_PHASE_NONE,
  SPN_PHASE_PACKAGES,
  SPN_PHASE_CONFIGURED,
  SPN_PHASE_UNITS,
  SPN_PHASE_BUILT,
} spn_phase_t;

typedef struct {
  spn_target_selection_t selection;
  bool force;
  spn_profile_info_t overrides;
} spn_app_config_t;

struct spn_session_t {
  spn_ctx_t* ctx;
  sp_mem_t mem;
  spn_pkg_info_t* pkg;
  sp_env_t env;
  spn_phase_t phase;

  spn_profile_table_t profiles;
  spn_toolchain_catalog_t catalog;
  spn_profile_info_t profile;

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

  sp_da(spn_build_plan_t) plans;
  struct {
    sp_om(spn_build_id_t, spn_build_unit_t) builds;
    spn_build_unit_t* target;
    spn_build_unit_t* metaprogram;
    sp_da(spn_toolchain_unit_t*) toolchains;
    sp_om(spn_pkg_unit_id_t, spn_pkg_unit_t) packages;
    sp_om(spn_target_unit_id_t, spn_target_unit_t) targets;
    sp_om(spn_compile_unit_id_t, spn_compile_unit_t) objects;
  } units;

  struct {
    sp_str_t root;
    sp_str_t build;
  } paths;

  struct {
    spn_dag_build_t* configure;
    spn_dag_build_t* build;
  } dag;
};

#endif
