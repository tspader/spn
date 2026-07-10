#ifndef SPN_APP_TYPES_H
#define SPN_APP_TYPES_H

#include "sp.h"
#include "spn.h"

#include "lock/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "task/task.h"


// @spader @nuke
typedef struct {
  sp_str_t lock;
  sp_str_t manifest;
} spn_app_paths_t;

typedef struct {
  spn_session_t* session;
  spn_resolved_pkg_t* pkg;
  spn_loaded_pkg_t loaded;
} spn_sync_pkg_job_t;

typedef struct {
  spn_session_t* session;
  spn_toolchain_store_t* store;
  spn_toolchain_t* toolchain;
  spn_toolchain_unit_t* unit;
} spn_sync_toolchain_job_t;

typedef struct {
  spn_index_info_t* index;
  bool force;
  spn_err_t err;
} spn_sync_index_job_t;

typedef enum {
  SPN_ADD_DEP_PACKAGE,
  SPN_ADD_DEP_TEST,
  SPN_ADD_DEP_BUILD,
} spn_add_dep_t;

typedef struct {
  spn_pkg_name_t name;
  sp_str_t key;
  sp_str_t requested;
  spn_semver_range_t range;
  spn_add_dep_t dep;
} spn_add_request_t;

struct spn_app_t {
  spn_app_paths_t paths;
  spn_pkg_info_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_session_t session;
  spn_task_executor_t tasks;

  spn_app_config_t config;

  union {
    spn_add_request_t add;
  } request;

  struct {
    spn_toolchain_store_t store;
    sp_da(spn_sync_pkg_job_t*) packages;
    sp_da(spn_sync_toolchain_job_t*) toolchains;
  } sync;

  struct {
    spn_bg_ctx_t bg;
    sp_da(spn_sync_index_job_t*) jobs;
  } index_sync;

  sp_da(sp_str_t) search;
};

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

extern spn_app_t app;

#endif
