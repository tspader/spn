#ifndef SPN_APP_TYPES_H
#define SPN_APP_TYPES_H

#include "sp.h"
#include "spn.h"

#include "lock/types.h"
#include "resolve/types.h"
#include "session/types.h"
#include "task/task.h"


typedef struct {
  sp_str_t dir;
  sp_str_t lock;
} spn_app_paths_t;

typedef struct {
  spn_target_filter_t filter;
  spn_profile_t* profile;
  bool force;
} spn_app_config_t;

struct spn_app_t {
  spn_app_paths_t paths;
  spn_pkg_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_resolver_t* resolver;
  spn_session_t session;
  spn_task_executor_t tasks;

  spn_app_config_t config;

  sp_da(sp_str_t) search;
  spn_pkg_registry_t registry;
  spn_pkg_cache_t cache;
};

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

extern spn_app_t app;

#endif
