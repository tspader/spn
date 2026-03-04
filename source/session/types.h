#ifndef SPN_SESSION_TYPES_H
#define SPN_SESSION_TYPES_H

#include "sp.h"
#include "spn.h"

#include "filter.h"
#include "graph.h"
#include "profile.h"
#include "semver/types.h"
#include "unit/types.h"

typedef enum {
  SPN_DEP_REPO_STATE_NOT_CLONED,
  SPN_DEP_REPO_STATE_UNLOCKED,
  SPN_DEP_REPO_STATE_LOCKED,
} spn_dep_repo_state_t;

typedef enum {
  SPN_DEP_BUILD_STATE_NONE,
  SPN_DEP_BUILD_STATE_IDLE,
  SPN_DEP_BUILD_STATE_CLONING,
  SPN_DEP_BUILD_STATE_FETCHING,
  SPN_DEP_BUILD_STATE_CHECKING_OUT,
  SPN_DEP_BUILD_STATE_RESOLVING,
  SPN_DEP_BUILD_STATE_BUILDING,
  SPN_DEP_BUILD_STATE_PACKAGING,
  SPN_DEP_BUILD_STATE_STAMPING,
  SPN_DEP_BUILD_STATE_DONE,
  SPN_DEP_BUILD_STATE_CANCELED,
  SPN_DEP_BUILD_STATE_FAILED
} spn_dep_state_t;

typedef enum {
  SPN_PACKAGE_STATE_UNLOADED,
  SPN_PACKAGE_STATE_LOADED,
} spn_package_state_t;

typedef struct {
  spn_build_graph_t graph;
  spn_bg_dirty_t* dirty;
  spn_bg_executor_t* executor;
} spn_bg_ctx_t;

struct spn_session_t {
  spn_pkg_t* pkg;
  spn_profile_t* profile;
  spn_target_filter_t filter;

  struct {
    sp_om(spn_target_unit_t) targets;
    sp_om(spn_pkg_unit_t) packages;
    spn_pkg_unit_t root;
  } units;

  struct {
    sp_str_t root;
    sp_str_t build;
    sp_str_t profile;
  } paths;

  spn_bg_ctx_t build;
  spn_bg_ctx_t sync;
  spn_bg_ctx_t configure;
  sp_mutex_t mutex;
};

#endif
