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

struct spn_app_t {
  spn_app_paths_t paths;
  spn_pkg_info_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_session_t session;
  spn_task_executor_t tasks;

  spn_app_config_t config;

  struct {
    spn_toolchain_store_t store;
    sp_da(spn_sync_pkg_job_t*) packages;
    sp_da(spn_sync_toolchain_job_t*) toolchains;
  } sync;

  sp_da(sp_str_t) search;
};

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

extern spn_app_t app;

#endif
