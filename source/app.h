#ifndef SPN_APP_H
#define SPN_APP_H

#include "sp.h"
#include "spn.h"

#include "event.h"
#include "intern.h"
#include "jit.h"
#include "lock.h"
#include "log.h"
#include "registry.h"
#include "resolve.h"
#include "session.h"
#include "cli.h"
#include "tui.h"
#include "task/task.h"

#define SPN_VERSION "1.0.0"
#define SPN_COMMIT "00c0fa98"

typedef struct {
  sp_str_t dir;
  sp_str_t lock;
} spn_app_paths_t;

typedef struct {
  sp_str_t dir;
  sp_str_t manifest;
  sp_str_t lock;
} spn_tools_paths_t;

typedef struct {
  sp_str_t dir;
  sp_str_t bin;
  sp_str_t lib;
} spn_tool_paths_t;

typedef struct spn_app_t spn_app_t;

typedef struct {
  spn_target_filter_t filter;
  spn_profile_t* profile;
  bool force;
} spn_app_config_t;

struct spn_pkg_cache sp_om_body(spn_pkg_t);
typedef struct spn_pkg_cache* spn_pkg_cache_t;

struct spn_app_t {
  spn_app_paths_t paths;
  spn_pkg_t package;
  sp_opt(spn_lock_file_t) lock;
  spn_resolver_t resolver;
  spn_session_t session;
  spn_task_executor_t tasks;

  spn_app_config_t config;

  sp_da(sp_str_t) search;
  sp_str_ht(sp_str_t) registry;
  spn_pkg_cache_t cache;
};

typedef enum {
  SPN_APP_INIT_NORMAL,
  SPN_APP_INIT_BARE,
} spn_app_init_mode_t;

typedef struct {
  spn_cli_t cli;
  struct {
    spn_tools_paths_t tools;
    sp_str_t cwd;
    sp_str_t project;
    sp_str_t manifest;
    sp_str_t executable;
    sp_str_t config_dir;
    sp_str_t config;
    sp_str_t bin;
    sp_str_t storage;
    sp_str_t runtime;
    sp_str_t log;
    sp_str_t spn;
    sp_str_t include;
    sp_str_t index;
    sp_str_t cache;
    sp_str_t build;
    sp_str_t store;
    sp_str_t source;
  } paths;
  spn_tui_t tui;
  sp_atomic_s32 control;
  sp_str_t tcc_error;
  sp_da(spn_index_t) registries;
  spn_index_t registry;
  spn_event_buffer_t* events;
  sp_app_t* sp;
  s32 num_args;
  const c8** args;
  sp_intern_t* intern;
  spn_jit_entry_t jit;
  sp_mem_arena_t* arena;
  sp_env_t* env;

  struct {
    sp_io_writer_t out;
    sp_io_writer_t err;
  } logger;
  spn_verbosity_t verbosity;
  spn_log_level_t log_level;
} spn_ctx_t;

extern spn_app_t app;
extern spn_ctx_t spn;

void       spn_app_load(spn_app_t* app, sp_str_t manifest_path);
void       spn_app_write_manifest(spn_pkg_t* package, sp_str_t path);
spn_pkg_t* spn_app_find_package(spn_app_t* app, sp_str_t name);
spn_pkg_t* spn_app_find_package_from_request(spn_app_t* app, spn_pkg_req_t dep);
spn_app_t  spn_app_init_and_write(sp_str_t path, sp_str_t name, spn_app_init_mode_t mode);
spn_pkg_t* spn_app_ensure_package(spn_app_t* app, spn_pkg_req_t dep);
spn_err_t  spn_app_add_pkg_constraints(spn_app_t* app, spn_pkg_t* pkg);
void       spn_app_update_lock_file(spn_app_t* app);
void       spn_app_resolve(spn_app_t* app);
spn_err_t  spn_app_resolve_from_solver(spn_app_t* app);
void       spn_app_resolve_from_lock_file(spn_app_t* app);

sp_app_result_t spn_init(sp_app_t* app);
sp_app_result_t spn_poll(sp_app_t* app);
sp_app_result_t spn_update(sp_app_t* app);
sp_app_result_t spn_deinit(sp_app_t* app);
void            spn_push_event(spn_build_event_kind_t kind);
void            spn_push_event_ex(spn_build_event_t event);

#endif
