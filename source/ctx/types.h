#ifndef SPN_CTX_TYPES_H
#define SPN_CTX_TYPES_H

#include "sp.h"
#include "spn/core.h"

#include "event/types.h"
#include "core/types.h"
#include "git/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "session/types.h"
#include "toolchain/types.h"

struct spn_ctx_t {
  spn_project_t* project;
  spn_session_t* session;
  sp_atomic_s32_t aborted;
  sp_atomic_s32_t error;
  sp_atomic_ptr_t progress;
  spn_index_arr_t indexes;
  struct {
    spn_index_arr_t indexes;
  } config;
  spn_event_buffer_t* events;
  sp_intern_t* intern;
  sp_mem_t mem;
  sp_mem_arena_t* arena;
  sp_mem_t heap;
  sp_env_t* env;
  spn_system_paths_t paths;

  struct {
    spn_git_cache_t git;
    spn_toolchain_store_t toolchains;
  } caches;
};

extern spn_ctx_t spn;

#endif
