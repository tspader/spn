#ifndef SPN_CTX_TYPES_H
#define SPN_CTX_TYPES_H

#include "sp.h"

#include "codegen/types.h"
#include "cli/types.h"
#include "event/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "paths/types.h"
#include "toolchain/types.h"
#include "tui/types.h"

#include "log/types.h"

typedef spn_cg_config_t spn_config_file_t;

typedef struct {
  spn_cli_t cli;
  spn_tui_t tui;
  sp_atomic_s32_t control;
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

  struct {
    struct {
      u64 level;
    } log;
    struct {
      const c8* config;
      const c8* storage;
    } dir;
  } config;

  struct {
    sp_io_stream_writer_t out;
    sp_io_stream_writer_t err;
    spn_log_level_t level;
    spn_verbosity_t verbosity;
  } logger;
} spn_ctx_t;

extern spn_ctx_t spn;

#endif
