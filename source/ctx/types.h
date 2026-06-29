#ifndef SPN_CTX_TYPES_H
#define SPN_CTX_TYPES_H

#include "sp.h"

#include "cli/types.h"
#include "event/types.h"
#include "index/types.h"
#include "intern/types.h"
#include "toolchain/types.h"
#include "tui/types.h"

#include "jit.h"
#include "log/types.h"

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
    sp_str_t version;
    sp_str_t log;
    sp_str_t include;
    sp_str_t cache;
    sp_str_t build;
    sp_str_t store;
    sp_str_t source;
    sp_str_t toolchain;
  } paths;
  spn_tui_t tui;
  sp_atomic_s32_t control;
  sp_str_t tcc_error;
  spn_index_arr_t indexes;
  sp_da(spn_toolchain_entry_t) toolchains;
  spn_event_buffer_t* events;
  sp_app_t* sp;
  s32 num_args;
  const c8** args;
  sp_intern_t* intern;
  spn_jit_entry_t jit;
  sp_mem_t mem;
  sp_env_t* env;

  struct {
    sp_mem_arena_t* bulk;
  } arenas;
  sp_mem_heap_t* heap;

  struct {
    sp_mem_t bulk;
    sp_mem_t heap;
  } allocators;

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
    sp_io_writer_t* out;
    sp_io_writer_t* err;
    sp_io_writer_t* jsonl;
    spn_log_level_t level;
    spn_verbosity_t verbosity;
  } logger;
} spn_ctx_t;

extern spn_ctx_t spn;

#endif
