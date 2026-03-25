#ifndef SPN_GRAPH_TYPES_H
#define SPN_GRAPH_TYPES_H

#include "sp.h"
#include "err.h"

typedef struct {
  u32 index;
  u32 occupied;
} spn_bg_id_t;

typedef enum {
  SPN_BUILD_GRAPH_NODE_FILE,
  SPN_BUILD_GRAPH_NODE_CMD,
} spn_bg_node_kind_t;

typedef enum {
  SPN_BUILD_CMD_SUBPROCESS = 0,
  SPN_BUILD_CMD_FN = 1,
} spn_bg_cmd_kind_t;

typedef enum {
  SPN_BG_VIZ_DEFAULT = 0,
  SPN_BG_VIZ_MANIFEST,
  SPN_BG_VIZ_STAMP,
  SPN_BG_VIZ_BINARY,
  SPN_BG_VIZ_SOURCE,
  SPN_BG_VIZ_CMD,
} spn_bg_viz_kind_t;

typedef struct spn_build_cmd spn_bg_cmd_t;
SP_TYPEDEF_FN(s32, spn_bg_fn_t, spn_bg_cmd_t* cmd, void* user_data);

struct spn_build_cmd {
  spn_bg_cmd_kind_t kind;
  spn_bg_viz_kind_t viz;
  sp_str_t package;
  spn_bg_id_t id;
  sp_str_t tag;
  sp_da(spn_bg_id_t) consumes;
  sp_da(spn_bg_id_t) produces;

  union {
    sp_ps_config_t ps;
    struct {
      spn_bg_fn_t on_execute;
      void* user_data;
    } fn;
  };
};

typedef struct {
  spn_bg_id_t id;
  spn_bg_viz_kind_t viz;
  sp_str_t package;
  sp_str_t path;
  sp_tm_epoch_t mod_time;

  spn_bg_id_t producer;
  sp_da(spn_bg_id_t) consumers;
} spn_bg_file_t;

typedef enum {
  SPN_BG_OK,
  SPN_BG_ERR_MISSING_INPUT,
  SPN_BG_ERR_DUPLICATE_OUTPUT,
} spn_bg_err_kind_t;

struct spn_bg_err {
  spn_bg_err_kind_t kind;
  union {
    struct { spn_bg_id_t file_id; } missing_input;
    struct {
      spn_bg_id_t file;
      struct {
        spn_bg_id_t a;
        spn_bg_id_t b;
      } cmds;
    } duplicate_output;
  };
};
typedef struct spn_bg_err spn_bg_err_t;

typedef struct {
  sp_da(spn_bg_file_t) files;
  sp_da(spn_bg_cmd_t) commands;
  sp_opt(spn_bg_err_t) error;
} spn_build_graph_t;

SP_TYPEDEF_FN(void, spn_bg_cmd_fn_t, spn_build_graph_t* graph, spn_bg_cmd_t* cmd, void* user_data);
SP_TYPEDEF_FN(void, spn_bg_file_fn_t, spn_build_graph_t* graph, spn_bg_file_t* file, void* user_data);

typedef struct {
  sp_ht(spn_bg_id_t, bool) files;
  sp_ht(spn_bg_id_t, bool) commands;
} spn_bg_visited_t;

typedef struct {
  sp_tm_epoch_t in;
  sp_tm_epoch_t out;
  u32 degree;
} spn_bg_dirty_cmd_metadata_t;

typedef struct {
  sp_tm_epoch_t mod_time;
  bool exists;
} spn_bg_dirty_file_metadata_t;

typedef struct {
  sp_ht(spn_bg_id_t, spn_bg_dirty_file_metadata_t) files;
  sp_ht(spn_bg_id_t, spn_bg_dirty_cmd_metadata_t) commands;
} spn_bg_dirty_metadata_t;

typedef struct {
  sp_ht(spn_bg_id_t, bool) files;
  sp_ht(spn_bg_id_t, bool) commands;
  sp_da(spn_bg_err_t) errors;
  spn_bg_dirty_metadata_t metadata;

} spn_bg_dirty_t;

typedef struct {
  spn_bg_node_kind_t kind;
  spn_bg_id_t id;
} spn_bg_node_t;

typedef enum {
  SPN_BG_ITER_MODE_DEPTH_FIRST,
  SPN_BG_ITER_MODE_BREADTH_FIRST
} spn_bg_it_mode_t;

typedef enum {
  SPN_BG_ITER_DIR_IN_TO_OUT,
  SPN_BG_ITER_DIR_OUT_TO_IN,
} spn_bg_it_dir_t;

typedef struct {
  spn_build_graph_t* graph;
  spn_bg_it_mode_t mode;
  spn_bg_it_dir_t direction;
  spn_bg_cmd_fn_t on_cmd;
  spn_bg_file_fn_t on_file;
  void* user_data;
} spn_bg_it_config_t;

typedef struct {
  spn_bg_it_mode_t mode;
  spn_bg_it_dir_t direction;
  spn_bg_cmd_fn_t on_cmd;
  spn_bg_file_fn_t on_file;
  void* user_data;
  spn_build_graph_t* graph;
  sp_da(spn_bg_node_t) nodes;
  u32 index;
} spn_bg_it_t;

typedef struct {
  sp_str_t package;
  sp_str_t tag;
  spn_bg_viz_kind_t viz;
} spn_bg_node_metadata_t;

typedef struct {
  spn_bg_fn_t fn;
  void* user_data;
  spn_bg_node_metadata_t metadata;
} spn_bg_cmd_config_t;

typedef struct {
  sp_str_t path;
  spn_bg_node_metadata_t metadata;
} spn_bg_file_config_t;

typedef struct {
  spn_bg_id_t cmd_id;
  s32 result;
} spn_bg_exec_error_t;

typedef struct {
  u32 num_threads;
  bool enable_logging;
} spn_bg_executor_config_t;

typedef struct {
  sp_rb(spn_bg_id_t) ready_queue;
  sp_mutex_t mutex;
  sp_semaphore_t work_available;

  sp_ht(spn_bg_id_t, bool) completed;
  sp_ht(spn_bg_id_t, bool) enqueued;

  spn_build_graph_t* graph;
  spn_bg_dirty_t* dirty;
  u32 num_threads;
  bool enable_logging;
  sp_thread_t driver;
  sp_tm_timer_t timer;
  u64 elapsed;
  sp_da(sp_thread_t) workers;
  sp_da(spn_bg_id_t) ran;
  sp_da(spn_bg_exec_error_t) errors;

  s32 active_workers;
  sp_atomic_s32_t shutdown;
} spn_bg_executor_t;

#endif
