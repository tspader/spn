#ifndef SPN_TEST_GRAPH_H
#define SPN_TEST_GRAPH_H

#include "sp.h"
#include "sp/it.h"
#include "sp/tm.h"

#define SP_ALLOC(T) (T*)sp_alloc(sizeof(T))

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

// Visualization kinds (used by mermaid renderer)
typedef enum {
  SPN_BG_VIZ_DEFAULT = 0,
  SPN_BG_VIZ_MANIFEST,  // spn.toml, spn.c
  SPN_BG_VIZ_STAMP,     // .stamp files
  SPN_BG_VIZ_BINARY,    // target binaries
  SPN_BG_VIZ_SOURCE,    // source files (.c, .h)
  SPN_BG_VIZ_CMD,       // generic command
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

typedef struct {
  sp_da(spn_bg_file_t) files;
  sp_da(spn_bg_cmd_t) commands;
} spn_build_graph_t;

SP_TYPEDEF_FN(void, spn_bg_cmd_fn_t, spn_build_graph_t* graph, spn_bg_cmd_t* cmd, void* user_data);
SP_TYPEDEF_FN(void, spn_bg_file_fn_t, spn_build_graph_t* graph, spn_bg_file_t* file, void* user_data);

typedef struct {
  sp_ht(spn_bg_id_t, bool) files;
  sp_ht(spn_bg_id_t, bool) commands;
} spn_bg_visited_t;

typedef enum {
  SPN_BG_OK,
  SPN_BG_ERR_MISSING_INPUT,
} spn_bg_err_kind_t;

typedef struct {
  spn_bg_err_kind_t kind;
  union {
    struct { spn_bg_id_t file_id; } missing_input;
  };
} spn_bg_err_t;

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


spn_build_graph_t* spn_bg_new();
spn_bg_id_t        spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
spn_bg_id_t        spn_bg_add_file_ex(spn_build_graph_t* graph, sp_str_t path, spn_bg_viz_kind_t viz, sp_str_t package);
spn_bg_id_t        spn_bg_add_file_c(spn_build_graph_t* graph, spn_bg_file_config_t config);
spn_bg_id_t        spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data);
spn_bg_id_t        spn_bg_add_fn_ex(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data, spn_bg_viz_kind_t viz, sp_str_t package, sp_str_t tag);
spn_bg_id_t        spn_bg_add_fn_c(spn_build_graph_t* graph, spn_bg_cmd_config_t config);
void               spn_bg_file_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_file_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
void               spn_bg_cmd_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_cmd_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph);
spn_bg_file_t*     spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
bool               spn_bg_is_file_input(spn_bg_file_t* file);
void               spn_bg_file_set_producer(spn_build_graph_t* g, spn_bg_id_t file, spn_bg_id_t cmd);
sp_str_t           spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_file_to_str(spn_bg_file_t* file);
spn_bg_cmd_t*      spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
void               spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag);
void               spn_bg_cmd_set_fn(spn_build_graph_t* g, spn_bg_id_t id, spn_bg_fn_t fn, void* ud);
void               spn_bg_cmd_add_output(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
void               spn_bg_cmd_add_input(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
sp_str_t           spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_cmd_to_str(spn_bg_cmd_t* cmd);
spn_bg_it_t        spn_bg_it_new(spn_bg_it_config_t config);
spn_bg_node_t      spn_bg_it_next(spn_bg_it_t* it);
void               spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
bool               spn_bg_it_done(spn_bg_it_t* it);
sp_str_t           spn_bg_dfs(spn_bg_it_config_t config);
sp_str_t           spn_bg_bfs(spn_bg_it_config_t config);
void               spn_bg_to_mermaid(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, sp_io_writer_t* stream, sp_str_t project_dir, sp_str_t cache_dir, sp_str_t work_dir, sp_str_t store_dir);
spn_bg_dirty_t*    spn_bg_dirty_new();
spn_bg_dirty_t*    spn_bg_compute_dirty(spn_build_graph_t* graph);
bool               spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
bool               spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
spn_bg_visited_t*  spn_bg_visited_new();
bool               spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t id);



//////////////
// EXECUTOR //
//////////////
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
  sp_atomic_s32 shutdown;
} spn_bg_executor_t;

spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config);
void spn_bg_executor_run(spn_bg_executor_t* ex);
void spn_bg_executor_join(spn_bg_executor_t* ex);
void spn_bg_executor_free(spn_bg_executor_t* ex);
spn_bg_dirty_t* spn_bg_compute_forced_dirty(spn_build_graph_t* graph);
sp_str_t spn_bg_mermaid_class_ex(sp_str_t name, sp_str_t fill, sp_str_t stroke, sp_str_t color, sp_str_t font_size);
spn_build_graph_t* spn_bg_new();
spn_bg_id_t        spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
spn_bg_id_t        spn_bg_add_file_ex(spn_build_graph_t* graph, sp_str_t path, spn_bg_viz_kind_t viz, sp_str_t package);
spn_bg_id_t        spn_bg_add_file_c(spn_build_graph_t* graph, spn_bg_file_config_t config);
spn_bg_id_t        spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data);
spn_bg_id_t        spn_bg_add_fn_ex(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data, spn_bg_viz_kind_t viz, sp_str_t package, sp_str_t tag);
spn_bg_id_t        spn_bg_add_fn_c(spn_build_graph_t* graph, spn_bg_cmd_config_t config);
void               spn_bg_file_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_file_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
void               spn_bg_cmd_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag, sp_str_t package, spn_bg_viz_kind_t viz);
void               spn_bg_cmd_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata);
sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph);
spn_bg_file_t*     spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
bool               spn_bg_is_file_input(spn_bg_file_t* file);
void               spn_bg_file_set_producer(spn_build_graph_t* g, spn_bg_id_t file, spn_bg_id_t cmd);
sp_str_t           spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_file_to_str(spn_bg_file_t* file);
spn_bg_cmd_t*      spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
void               spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag);
void               spn_bg_cmd_set_fn(spn_build_graph_t* g, spn_bg_id_t id, spn_bg_fn_t fn, void* ud);
void               spn_bg_cmd_add_output(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
void               spn_bg_cmd_add_input(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
sp_str_t           spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_cmd_to_str(spn_bg_cmd_t* cmd);
spn_bg_it_t        spn_bg_it_new(spn_bg_it_config_t config);
spn_bg_node_t      spn_bg_it_next(spn_bg_it_t* it);
void               spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
bool               spn_bg_it_done(spn_bg_it_t* it);
sp_str_t           spn_bg_dfs(spn_bg_it_config_t config);
sp_str_t           spn_bg_bfs(spn_bg_it_config_t config);
void               spn_bg_to_mermaid(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, sp_io_writer_t* stream, sp_str_t project_dir, sp_str_t cache_dir, sp_str_t work_dir, sp_str_t store_dir);
spn_bg_dirty_t*    spn_bg_dirty_new();
spn_bg_dirty_t*    spn_bg_compute_dirty(spn_build_graph_t* graph);
bool               spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
bool               spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
spn_bg_visited_t*  spn_bg_visited_new();
bool               spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t id);


#endif // SPN_TEST_GRAPH_H
