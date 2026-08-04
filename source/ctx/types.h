#ifndef SPN_CTX_TYPES_H
#define SPN_CTX_TYPES_H

#include "sp.h"
#include "spn.h"

#include "codegen/types.h"
#include "codegen/gen/config.gen.h"
#include "cli/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "git/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "pkg/types.h"
#include "semver/types.h"
#include "session/types.h"
#include "task/types.h"
#include "thread_pool/types.h"
#include "toolchain/types.h"
#include "tui/types.h"

#include "log/types.h"

typedef spn_cg_config_t spn_config_file_t;

typedef struct {
  spn_session_t* session;
  spn_resolved_pkg_t* pkg;
  spn_loaded_pkg_t loaded;
  spn_err_t err;
} spn_sync_pkg_job_t;

typedef struct {
  spn_toolchain_unit_t* unit;
  spn_err_t err;
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

struct spn_ctx_t {
  spn_app_t* app;
  spn_cli_t cli;
  spn_tui_t tui;
  sp_atomic_s32_t aborted;
  spn_index_arr_t indexes;
  spn_event_buffer_t* events;
  sp_app_t* sp;
  s32 num_args;
  const c8** args;
  sp_intern_t* intern;
  sp_mem_t mem;
  sp_mem_arena_t* arena;
  sp_mem_t heap;
  sp_env_t* env;
  spn_system_paths_t paths;
  spn_config_file_t config_file;
  spn_err_union_t result;

  struct {
    spn_git_cache_t git;
    spn_toolchain_store_t toolchains;
  } caches;

  spn_task_executor_t tasks;
  spn_app_config_t config;

  spn_add_request_t add;

  struct {
    spn_thread_pool_t pool;
    sp_da(spn_sync_pkg_job_t*) packages;
    sp_da(spn_sync_toolchain_job_t*) toolchains;
    sp_tm_timer_t timer;
  } sync;

  struct {
    spn_thread_pool_t pool;
    sp_da(spn_sync_index_job_t*) jobs;
  } index_sync;

  struct {
    sp_io_stream_writer_t out;
    sp_io_stream_writer_t err;
    sp_io_file_writer_t jsonl;
    spn_log_level_t level;
    spn_verbosity_t verbosity;
  } logger;
};

extern spn_ctx_t spn;

#endif
