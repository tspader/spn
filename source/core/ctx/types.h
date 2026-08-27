#ifndef SPN_CTX_TYPES_H
#define SPN_CTX_TYPES_H

#include "sp.h"
#include "sp/queue.h"
#include "spn/core.h"

#include "event/types.h"
#include "core/types.h"
#include "git/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "session/types.h"
#include "toolchain/types.h"

typedef struct spn_op_t spn_op_t;

struct spn_ctx_t {
  spn_project_t* project;
  spn_session_t* session;
  sp_atomic_s32_t error;
  sp_atomic_ptr_t progress;
  sp_da(spn_index_info_t) indexes;
  struct {
    sp_da(spn_index_info_t) indexes;
  } config;
  spn_event_buffer_t* events;
  sp_intern_t* intern;
  sp_mem_t mem;
  sp_mem_arena_t* arena;
  sp_mem_t heap;
  sp_env_t* env;
  spn_system_paths_t paths;
  spn_path_roots_t roots;
  spn_triple_t host;
  spn_toolchain_catalog_t catalog;

  struct {
    spn_git_cache_t git;
    spn_toolchain_store_t toolchains;
  } caches;

  struct {
    sp_thread_t thread;
    sp_queue_t queue;
    sp_atomic_u32_t running;
  } ops;

  spn_wake_t wake;
};

extern spn_ctx_t spn;

#endif
