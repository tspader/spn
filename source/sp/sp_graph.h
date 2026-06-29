#ifndef SP_GRAPH_H
#define SP_GRAPH_H

#include "sp.h"

////////////
// TYPES  //
////////////
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

typedef struct spn_build_cmd spn_bg_cmd_t;
SP_TYPEDEF_FN(s32, spn_bg_fn_t, spn_bg_cmd_t* cmd, void* user_data);

struct spn_build_cmd {
  spn_bg_cmd_kind_t kind;
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
  sp_mem_arena_t* arena;
  sp_mem_t allocator;
  sp_mem_t backing;
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
  sp_mem_arena_t* arena;
  sp_mem_t allocator;
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
  sp_mem_t mem;
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
  spn_bg_id_t cmd_id;
  s32 result;
} spn_bg_exec_error_t;

typedef struct {
  u32 num_threads;
  bool enable_logging;
} spn_bg_executor_config_t;

typedef struct {
  sp_mem_arena_t* arena;
  sp_mem_t allocator;

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
  sp_atomic_s32_t num_completed;
} spn_bg_executor_t;

SP_API spn_build_graph_t*  spn_bg_new(sp_mem_t mem);
SP_API void                spn_bg_init(spn_build_graph_t* graph, sp_mem_t mem);
SP_API void                spn_bg_free(spn_build_graph_t* graph);
SP_API spn_bg_id_t         spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
SP_API spn_bg_id_t         spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data);
SP_API void                spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag);
SP_API void                spn_bg_cmd_set_fn(spn_build_graph_t* g, spn_bg_id_t id, spn_bg_fn_t fn, void* ud);
SP_API sp_err_t            spn_bg_cmd_add_output(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
SP_API sp_err_t            spn_bg_cmd_add_input(spn_build_graph_t* g, spn_bg_id_t cmd, spn_bg_id_t file);
SP_API sp_err_t            spn_bg_file_set_producer(spn_build_graph_t* g, spn_bg_id_t file, spn_bg_id_t cmd);
SP_API spn_bg_file_t*      spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
SP_API spn_bg_cmd_t*       spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
SP_API bool                spn_bg_is_file_input(spn_bg_file_t* file);
SP_API sp_da(spn_bg_id_t)  spn_bg_find_outputs(spn_build_graph_t* graph);
SP_API sp_str_t            spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
SP_API sp_str_t            spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
SP_API sp_str_t            spn_bg_err_to_str(spn_build_graph_t* graph, spn_bg_err_t err);
SP_API sp_str_t            spn_bg_dfs(spn_bg_it_config_t config);
SP_API sp_str_t            spn_bg_bfs(spn_bg_it_config_t config);
SP_API spn_bg_dirty_t*     spn_bg_dirty_new(sp_mem_t mem);
SP_API void                spn_bg_dirty_free(spn_bg_dirty_t* dirty);
SP_API spn_bg_dirty_t*     spn_bg_compute_dirty(spn_build_graph_t* graph);
SP_API spn_bg_dirty_t*     spn_bg_compute_forced_dirty(spn_build_graph_t* graph);
SP_API bool                spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
SP_API bool                spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t id);
SP_API spn_bg_executor_t*  spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config);
SP_API void                spn_bg_executor_run(spn_bg_executor_t* ex);
SP_API void                spn_bg_executor_join(spn_bg_executor_t* ex);
SP_API void                spn_bg_executor_free(spn_bg_executor_t* ex);

///////////////////
// PRIVATE API   //
///////////////////
SP_IMP spn_bg_it_t         spn_bg_it_new(spn_bg_it_config_t config);
SP_IMP spn_bg_node_t       spn_bg_it_next(spn_bg_it_t* it);
SP_IMP bool                spn_bg_it_done(spn_bg_it_t* it);
SP_IMP void                spn_bg_it_visit_node(spn_bg_it_t* it, spn_bg_node_t node);
SP_IMP void                spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
SP_IMP spn_bg_visited_t*   spn_bg_visited_new(sp_mem_t mem);
SP_IMP bool                spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t id);
SP_IMP sp_str_t            spn_bg_traverse(spn_bg_it_config_t config);
SP_IMP sp_str_t            spn_bg_file_to_str(spn_bg_file_t* file);
SP_IMP sp_str_t            spn_bg_cmd_to_str(spn_build_graph_t* graph, spn_bg_cmd_t* cmd);

#endif // SP_GRAPH_H

#if defined(SP_IMPLEMENTATION) && !defined(SP_GRAPH_IMPLEMENTATION)
  #define SP_GRAPH_IMPLEMENTATION
#endif

#ifdef SP_GRAPH_IMPLEMENTATION

#define spn_bg_ht_has(__ht, __key) (sp_ht_getp((__ht), (__key)) != SP_NULLPTR)

SP_IMP bool spn_bg_epoch_gt(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s > b.s || (a.s == b.s && a.ns > b.ns);
}

SP_IMP sp_tm_epoch_t spn_bg_epoch_min(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return spn_bg_epoch_gt(a, b) ? b : a;
}

SP_IMP sp_tm_epoch_t spn_bg_epoch_max(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return spn_bg_epoch_gt(a, b) ? a : b;
}

//////////////
// ITERATOR //
//////////////
spn_bg_it_t spn_bg_it_new(spn_bg_it_config_t config) {
  spn_bg_it_t it = {
    .mode = config.mode,
    .direction = config.direction,
    .on_cmd = config.on_cmd,
    .on_file = config.on_file,
    .user_data = config.user_data,
    .graph = config.graph
  };
  sp_da_init(config.mem, it.nodes);

  sp_da_for(it.graph->files, n) {
    spn_bg_file_t* file = it.graph->files + n;

    bool is_leaf = false;
    switch (it.direction) {
      case SPN_BG_ITER_DIR_IN_TO_OUT: { is_leaf = !file->producer.occupied; break; }
      case SPN_BG_ITER_DIR_OUT_TO_IN: { is_leaf = sp_da_empty(file->consumers); break; }
    }

    if (!is_leaf) continue;

    spn_bg_node_t node = {
      .kind = SPN_BUILD_GRAPH_NODE_FILE,
      .id = file->id
    };
    sp_da_push(it.nodes, node);
  }

  sp_da_for(it.graph->commands, n) {
    spn_bg_cmd_t* cmd = it.graph->commands + n;

    bool is_leaf = false;
    switch (it.direction) {
      case SPN_BG_ITER_DIR_IN_TO_OUT: { is_leaf = sp_da_empty(cmd->consumes); break; }
      case SPN_BG_ITER_DIR_OUT_TO_IN: { is_leaf = sp_da_empty(cmd->produces); break; }
    }

    if (!is_leaf) continue;

    spn_bg_node_t node = {
      .kind = SPN_BUILD_GRAPH_NODE_CMD,
      .id = cmd->id
    };
    sp_da_push(it.nodes, node);
  }

  return it;
}

spn_bg_node_t spn_bg_it_next(spn_bg_it_t* it) {
  switch (it->mode) {
    case SPN_BG_ITER_MODE_BREADTH_FIRST: {
      spn_bg_node_t node = it->nodes[it->index];
      it->index++;
      return node;
    }
    case SPN_BG_ITER_MODE_DEPTH_FIRST: {
      spn_bg_node_t node = *sp_da_back(it->nodes);
      sp_da_pop(it->nodes);
      return node;
    }
  }

  SP_UNREACHABLE_RETURN(sp_zero_s(spn_bg_node_t));
}

bool spn_bg_it_done(spn_bg_it_t* it) {
  switch (it->mode) {
    case SPN_BG_ITER_MODE_BREADTH_FIRST: return it->index >= sp_da_size(it->nodes);
    case SPN_BG_ITER_MODE_DEPTH_FIRST: return sp_da_empty(it->nodes);
  }
  SP_UNREACHABLE_RETURN(true);
}

void spn_bg_it_visit_node(spn_bg_it_t* it, spn_bg_node_t node) {
  switch (node.kind) {
    case SPN_BUILD_GRAPH_NODE_FILE: {
      spn_bg_file_t* file = spn_bg_find_file(it->graph, node.id);
      if (it->on_file) {
        it->on_file(it->graph, file, it->user_data);
      }
      break;
    }
    case SPN_BUILD_GRAPH_NODE_CMD: {
      spn_bg_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);
      if (it->on_cmd) {
        it->on_cmd(it->graph, cmd, it->user_data);
      }
      break;
    }
  }
}

void spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node) {
  switch (it->direction) {
    case SPN_BG_ITER_DIR_OUT_TO_IN: {
      switch (node.kind) {
        case SPN_BUILD_GRAPH_NODE_FILE: {
          spn_bg_file_t* file = spn_bg_find_file(it->graph, node.id);

          if (file->producer.occupied) {
            spn_bg_node_t cmd = {
              .kind = SPN_BUILD_GRAPH_NODE_CMD,
              .id = file->producer
            };
            sp_da_push(it->nodes, cmd);
          }
          break;
        }
        case SPN_BUILD_GRAPH_NODE_CMD: {
          spn_bg_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

          sp_da_for(cmd->consumes, n) {
            spn_bg_node_t file = {
              .kind = SPN_BUILD_GRAPH_NODE_FILE,
              .id = cmd->consumes[n]
            };
            sp_da_push(it->nodes, file);
          }
          break;
        }
      }
      break;
    }
    case SPN_BG_ITER_DIR_IN_TO_OUT: {
      switch (node.kind) {
        case SPN_BUILD_GRAPH_NODE_FILE: {
          spn_bg_file_t* file = spn_bg_find_file(it->graph, node.id);

          sp_da_for(file->consumers, n) {
            spn_bg_node_t child = {
              .kind = SPN_BUILD_GRAPH_NODE_CMD,
              .id = file->consumers[n]
            };
            sp_da_push(it->nodes, child);
          }
          break;
        }
        case SPN_BUILD_GRAPH_NODE_CMD: {
          spn_bg_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

          sp_da_for(cmd->produces, n) {
            spn_bg_node_t child = {
              .kind = SPN_BUILD_GRAPH_NODE_FILE,
              .id = cmd->produces[n]
            };
            sp_da_push(it->nodes, child);
          }
          break;
        }
      }
      break;
    }
  }
}

///////////
// GRAPH //
///////////
spn_build_graph_t* spn_bg_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_build_graph_t* graph = sp_mem_allocator_alloc_type(a, spn_build_graph_t);
  graph->arena = arena;
  graph->allocator = a;
  graph->backing = mem;
  sp_da_init(a, graph->files);
  sp_da_init(a, graph->commands);
  return graph;
}

void spn_bg_init(spn_build_graph_t* graph, sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  graph->arena = arena;
  graph->allocator = a;
  graph->backing = mem;
  sp_da_init(a, graph->files);
  sp_da_init(a, graph->commands);
}

void spn_bg_free(spn_build_graph_t* graph) {
  if (!graph) return;
  sp_mem_arena_destroy(graph->arena);
}

spn_bg_id_t spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path) {
  // @spader: obviously a hack
  sp_da_for(graph->files, it) {
    spn_bg_file_t* existing = &graph->files[it];
    if (sp_str_equal(existing->path, path)) {
      return existing->id;
    }
  }

  spn_bg_file_t file = {
    .id = {
      .index = (u32)sp_da_size(graph->files),
      .occupied = true
    },
    .path = path,
  };
  sp_da_init(graph->allocator, file.consumers);
  sp_da_push(graph->files, file);
  return file.id;
}

spn_bg_id_t spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data) {
  spn_bg_id_t id = {
    .index = (u32)sp_da_size(graph->commands),
    .occupied = true
  };

  spn_bg_cmd_t cmd = {
    .kind = SPN_BUILD_CMD_FN,
    .id = id,
    .fn = {
      .on_execute = fn,
      .user_data = user_data
    },
  };
  sp_da_init(graph->allocator, cmd.consumes);
  sp_da_init(graph->allocator, cmd.produces);
  sp_da_push(graph->commands, cmd);

  return id;
}

bool spn_bg_is_file_input(spn_bg_file_t* file) {
  return !file->producer.occupied;
}

sp_err_t spn_bg_file_set_producer(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id) {
  spn_bg_file_t* file = spn_bg_find_file(graph, file_id);
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);

  if (file->producer.occupied) {
    if (!graph->error.some) {
      sp_opt_set(graph->error, ((spn_bg_err_t) {
        .kind = SPN_BG_ERR_DUPLICATE_OUTPUT,
        .duplicate_output = {
          .file = file_id,
          .cmds = { file->producer, cmd_id }
        }
      }));
    }

    return SP_ERR;
  }

  file->producer = cmd_id;
  sp_da_push(cmd->produces, file_id);
  return SP_OK;
}

void spn_bg_cmd_set_fn(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_fn_t fn, void* user_data) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  SP_ASSERT(cmd->kind == SPN_BUILD_CMD_FN);
  cmd->fn.on_execute = fn;
  cmd->fn.user_data = user_data;
}

sp_err_t spn_bg_cmd_add_output(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  sp_assert(cmd_id.occupied);
  sp_assert(file_id.occupied);
  sp_try(spn_bg_file_set_producer(graph, file_id, cmd_id));
  return SP_OK;
}

sp_err_t spn_bg_cmd_add_input(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  sp_assert(cmd_id.occupied);
  sp_assert(file_id.occupied);
  spn_bg_file_t* file = spn_bg_find_file(graph, file_id);
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
  sp_da_push(cmd->consumes, file_id);
  sp_da_push(file->consumers, cmd_id);
  return SP_OK;
}

spn_bg_file_t* spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id) {
  return graph->files + id.index;
}

spn_bg_cmd_t* spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id) {
  return graph->commands + id.index;
}

void spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  cmd->tag = sp_str_from_cstr(graph->allocator, tag);
}

sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph) {
  sp_da(spn_bg_id_t) ids = sp_da_new(graph->allocator, spn_bg_id_t);

  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = graph->files + it;
    if (sp_da_empty(file->consumers)) {
      sp_da_push(ids, file->id);
    }
  }

  return ids;
}

/////////////
// VISITED //
/////////////
bool spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t node) {
  switch (node.kind) {
    case SPN_BUILD_GRAPH_NODE_CMD: {
      if (spn_bg_ht_has(visited->commands, node.id)) {
        return true;
      }
      sp_ht_insert(visited->commands, node.id, true);
      return false;
    }
    case SPN_BUILD_GRAPH_NODE_FILE: {
      if (spn_bg_ht_has(visited->files, node.id)) {
        return true;
      }
      sp_ht_insert(visited->files, node.id, true);
      return false;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_bg_visited_t* spn_bg_visited_new(sp_mem_t mem) {
  spn_bg_visited_t* visited = sp_mem_allocator_alloc_type(mem, spn_bg_visited_t);
  sp_ht_init(mem, visited->files);
  sp_ht_init(mem, visited->commands);
  return visited;
}

//////////////
// TRAVERSE //
//////////////
sp_str_t spn_bg_traverse(spn_bg_it_config_t config) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  config.mem = scratch.mem;

  spn_bg_visited_t* visited = spn_bg_visited_new(scratch.mem);
  spn_bg_it_t it = spn_bg_it_new(config);

  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    if (spn_bg_visited_mark(visited, node)) continue;

    spn_bg_it_visit_node(&it, node);
    spn_bg_it_add_children(&it, node);
  }

  sp_mem_end_scratch(scratch);
  return sp_str_lit("");
}

sp_str_t spn_bg_dfs(spn_bg_it_config_t config) {
  config.mode = SPN_BG_ITER_MODE_DEPTH_FIRST;
  return spn_bg_traverse(config);
}

sp_str_t spn_bg_bfs(spn_bg_it_config_t config) {
  config.mode = SPN_BG_ITER_MODE_BREADTH_FIRST;
  return spn_bg_traverse(config);
}

/////////////
// DISPLAY //
/////////////
sp_str_t spn_bg_file_to_str(spn_bg_file_t* file) {
  return file->path;
}

sp_str_t spn_bg_cmd_to_str(spn_build_graph_t* graph, spn_bg_cmd_t* cmd) {
  if (!sp_str_empty(cmd->tag)) {
    return cmd->tag;
  } else if (cmd->kind == SPN_BUILD_CMD_SUBPROCESS && !sp_str_empty(cmd->ps.command)) {
    return cmd->ps.command;
  } else {
    return sp_fmt(graph->allocator, "cmd@{}", sp_fmt_ptr(cmd)).value;
  }
}

sp_str_t spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  SP_ASSERT(file);
  return spn_bg_file_to_str(file);
}

sp_str_t spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  return spn_bg_cmd_to_str(graph, cmd);
}

sp_str_t spn_bg_err_to_str(spn_build_graph_t* graph, spn_bg_err_t err) {
  switch (err.kind) {
    case SPN_BG_OK: {
      return sp_str_lit("ok");
    }
    case SPN_BG_ERR_MISSING_INPUT: {
      return sp_fmt(graph->allocator, "missing build graph input {}",
        sp_fmt_str(spn_bg_file_id_to_str(graph, err.missing_input.file_id))).value;
    }
    case SPN_BG_ERR_DUPLICATE_OUTPUT: {
      return sp_fmt(graph->allocator,
        "error: two graph nodes output the same file\nfile: {}\n{}\n{}",
        sp_fmt_str(spn_bg_file_id_to_str(graph, err.duplicate_output.file)),
        sp_fmt_str(spn_bg_cmd_id_to_str(graph, err.duplicate_output.cmds.a)),
        sp_fmt_str(spn_bg_cmd_id_to_str(graph, err.duplicate_output.cmds.b))).value;
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit("unknown build graph error"));
}

///////////
// DIRTY //
///////////
spn_bg_dirty_t* spn_bg_dirty_new(sp_mem_t mem) {
  sp_mem_arena_t* arena = sp_mem_arena_new(mem);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_bg_dirty_t* dirty = sp_mem_allocator_alloc_type(a, spn_bg_dirty_t);
  dirty->arena = arena;
  dirty->allocator = a;
  sp_ht_init(a, dirty->files);
  sp_ht_init(a, dirty->commands);
  sp_da_init(a, dirty->errors);
  sp_ht_init(a, dirty->metadata.files);
  sp_ht_init(a, dirty->metadata.commands);
  return dirty;
}

void spn_bg_dirty_free(spn_bg_dirty_t* dirty) {
  if (!dirty) return;
  sp_mem_arena_destroy(dirty->arena);
}

spn_bg_dirty_t* spn_bg_compute_forced_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = spn_bg_dirty_new(graph->backing);

  sp_da_for(graph->files, it) {
    sp_ht_insert(dirty->files, graph->files[it].id, true);
  }

  sp_da_for(graph->commands, it) {
    sp_ht_insert(dirty->commands, graph->commands[it].id, true);
  }

  return dirty;
}

spn_bg_dirty_t* spn_bg_compute_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = spn_bg_dirty_new(graph->backing);
  spn_bg_dirty_metadata_t* metadata = &dirty->metadata;

  spn_bg_it_t iter = spn_bg_it_new((spn_bg_it_config_t){
    .graph = graph,
    .mem = dirty->allocator,
    .mode = SPN_BG_ITER_MODE_BREADTH_FIRST,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
  });

  // get file mod times in a single pass
  sp_da_for(graph->files, n) {
    spn_bg_file_t* file = &graph->files[n];
    spn_bg_dirty_file_metadata_t metadatum = {
      .mod_time = sp_fs_get_mod_time(file->path),
      .exists = sp_fs_exists(file->path)
    };

    sp_ht_insert(metadata->files, file->id, metadatum);

    if (spn_bg_is_file_input(file)) {
      if (!metadatum.exists) {
        spn_bg_err_t err = {
          .kind = SPN_BG_ERR_MISSING_INPUT,
          .missing_input = { .file_id = file->id }
        };
        sp_da_push(dirty->errors, err);
      }
    }
  }

  // first pass: check each command, locally, for inputs newer than outputs
  sp_da_for(graph->commands, n) {
    spn_bg_cmd_t* cmd = &graph->commands[n];
    spn_bg_dirty_cmd_metadata_t metadatum = {
      .in = SP_LIMIT_EPOCH_MIN,
      .out = SP_LIMIT_EPOCH_MAX,
      .degree = (u32)sp_max(sp_da_size(cmd->consumes), 1)
    };

    bool missing_output = false;
    sp_da_for(cmd->produces, p) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(metadata->files, cmd->produces[p]);
      SP_ASSERT(m);
      metadatum.out = spn_bg_epoch_min(m->mod_time, metadatum.out);
      missing_output |= !m->exists;
    }

    sp_da_for(cmd->consumes, c) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(metadata->files, cmd->consumes[c]);
      SP_ASSERT(m);
      metadatum.in = spn_bg_epoch_max(m->mod_time, metadatum.in);
    }

    if (spn_bg_epoch_gt(metadatum.in, metadatum.out) || missing_output) {
      sp_ht_insert(dirty->commands, cmd->id, true);
    }

    sp_ht_insert(metadata->commands, cmd->id, metadatum);
  }

  // second pass: bootleg kahn's, to propagate dirtiness
  while (!spn_bg_it_done(&iter)) {
    spn_bg_node_t node = spn_bg_it_next(&iter);

    switch (node.kind) {
      case SPN_BUILD_GRAPH_NODE_FILE: {
        spn_bg_file_t* file = spn_bg_find_file(graph, node.id);
        if (spn_bg_ht_has(dirty->commands, file->producer)) {
          sp_ht_insert(dirty->files, node.id, true);
        }

        spn_bg_it_add_children(&iter, node);
        break;
      }
      case SPN_BUILD_GRAPH_NODE_CMD: {
        spn_bg_dirty_cmd_metadata_t* m = sp_ht_getp(metadata->commands, node.id);
        m->degree--;

        if (!m->degree) {
          spn_bg_cmd_t* cmd = spn_bg_find_command(graph, node.id);
          sp_da_for(cmd->consumes, c) {
            if (spn_bg_ht_has(dirty->files, cmd->consumes[c])) {
              sp_ht_insert(dirty->commands, node.id, true);
            }
          }

          spn_bg_it_add_children(&iter, node);
        }
        break;
      }
    }
  }

  return dirty;
}

bool spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t file_id) {
  return spn_bg_ht_has(dirty->files, file_id);
}

bool spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t cmd_id) {
  return spn_bg_ht_has(dirty->commands, cmd_id);
}

//////////////
// EXECUTOR //
//////////////
void spn_bg_executor_log_cmd(spn_bg_executor_t* ex, spn_bg_cmd_t* cmd) {
  if (!ex->enable_logging) return;

  sp_mutex_lock(&ex->mutex);
  if (!sp_str_empty(cmd->tag)) {
    sp_os_print(sp_fmt(ex->allocator, "[exec] {}\n", sp_fmt_str(cmd->tag)).value);
  } else if (cmd->kind == SPN_BUILD_CMD_SUBPROCESS && !sp_str_empty(cmd->ps.command)) {
    sp_os_print(sp_fmt(ex->allocator, "[exec] {}\n", sp_fmt_str(cmd->ps.command)).value);
  } else {
    sp_os_print(sp_fmt(ex->allocator, "[exec] cmd@{}\n", sp_fmt_ptr(cmd)).value);
  }
  sp_mutex_unlock(&ex->mutex);
}

bool spn_bg_cmd_is_ready(spn_bg_executor_t* ex, spn_bg_id_t cmd_id) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);
  sp_da_for(cmd->consumes, i) {
    spn_bg_file_t* file = spn_bg_find_file(ex->graph, cmd->consumes[i]);
    if (!file->producer.occupied) continue;
    if (!spn_bg_ht_has(ex->dirty->commands, file->producer)) continue;
    if (!spn_bg_ht_has(ex->completed, file->producer)) return false;
  }
  return true;
}

s32 spn_bg_worker_fn(void* user_data) {
  spn_bg_executor_t* ex = (spn_bg_executor_t*)user_data;

  while (true) {
    sp_semaphore_wait(&ex->work_available);

    if (sp_atomic_s32_get(&ex->shutdown)) {
      sp_semaphore_signal(&ex->work_available);  // cascade to next thread
      return 0;
    }

    sp_mutex_lock(&ex->mutex);
    if (sp_rb_empty(ex->ready_queue)) {
      sp_mutex_unlock(&ex->mutex);
      continue;
    }

    spn_bg_id_t cmd_id = *sp_rb_peek(ex->ready_queue);
    sp_rb_pop(ex->ready_queue);
    ex->active_workers++;
    sp_mutex_unlock(&ex->mutex);

    spn_bg_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);
    spn_bg_executor_log_cmd(ex, cmd);

    sp_opt(spn_bg_exec_error_t) error = sp_zero;
    switch (cmd->kind) {
      case SPN_BUILD_CMD_SUBPROCESS: {
        sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
        sp_ps_output_t result = sp_ps_run(scratch.mem, cmd->ps);
        if (result.status.exit_code) {
          sp_opt_set(error, ((spn_bg_exec_error_t) {
            .cmd_id = cmd_id,
            .result = result.status.exit_code
          }));
        }
        sp_mem_end_scratch(scratch);
        break;
      }
      case SPN_BUILD_CMD_FN: {
        if (cmd->fn.on_execute) {
          s32 result = cmd->fn.on_execute(cmd, cmd->fn.user_data);
          if (result) {
            sp_opt_set(error, ((spn_bg_exec_error_t) {
              .cmd_id = cmd_id,
              .result = result
            }));
          }
        }
        break;
      }
    }

    sp_mutex_lock(&ex->mutex);
    ex->active_workers--;
    sp_ht_insert(ex->completed, cmd_id, true);
    sp_atomic_s32_add(&ex->num_completed, 1);
    sp_da_push(ex->ran, cmd_id);

    switch (error.some) {
      case SP_OPT_SOME: {
        sp_da_push(ex->errors, error.value);

        sp_atomic_s32_set(&ex->shutdown, 1);
        sp_semaphore_signal(&ex->work_available);
        sp_mutex_unlock(&ex->mutex);
        return 0;
      }
      case SP_OPT_NONE: {
        // check anything that could be ready now that we're done
        sp_da_for(cmd->produces, i) {
          spn_bg_file_t* file = spn_bg_find_file(ex->graph, cmd->produces[i]);

          sp_da_for(file->consumers, j) {
            spn_bg_id_t downstream = file->consumers[j];
            if (!spn_bg_ht_has(ex->enqueued, downstream)) {
              if (spn_bg_ht_has(ex->dirty->commands, downstream) && spn_bg_cmd_is_ready(ex, downstream)) {
                sp_ht_insert(ex->enqueued, downstream, true);
                sp_rb_push(ex->ready_queue, downstream);
                sp_semaphore_signal(&ex->work_available);
              }
            }
          }
        }

        // if we're the last one, and this was the last task, shut down
        bool is_work_done = sp_rb_empty(ex->ready_queue);
        bool is_last_worker = ex->active_workers == 0;
        if (is_work_done && is_last_worker) {
          sp_atomic_s32_set(&ex->shutdown, 1);
          sp_semaphore_signal(&ex->work_available);
          sp_mutex_unlock(&ex->mutex);
          return 0;
        }

        sp_mutex_unlock(&ex->mutex);
        break;
      }
    }
  }
}

s32 spn_bg_driver_fn(void* user_data) {
  spn_bg_executor_t* ex = (spn_bg_executor_t*)user_data;

  if (sp_ht_empty(ex->dirty->commands)) {
    sp_atomic_s32_set(&ex->shutdown, 1);
    return 0;
  }

  sp_ht_for(ex->dirty->commands, it) {
    spn_bg_id_t cmd_id = *sp_ht_it_getkp(ex->dirty->commands, it);
    if (spn_bg_cmd_is_ready(ex, cmd_id)) {
      sp_ht_insert(ex->enqueued, cmd_id, true);
      sp_rb_push(ex->ready_queue, cmd_id);
      sp_semaphore_signal(&ex->work_available);
    }
  }

  sp_da_reserve(ex->workers, ex->num_threads);
  for (u32 i = 0; i < ex->num_threads; i++) {
    sp_thread_t thread;
    sp_thread_init(&thread, spn_bg_worker_fn, ex);
    sp_da_push(ex->workers, thread);
  }

  sp_da_for(ex->workers, i) {
    sp_thread_join(&ex->workers[i]);
  }

  return 0;
}

spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config) {
  sp_mem_arena_t* arena = sp_mem_arena_new(graph->backing);
  sp_mem_t a = sp_mem_arena_as_allocator(arena);
  spn_bg_executor_t* ex = sp_mem_allocator_alloc_type(a, spn_bg_executor_t);
  ex->arena = arena;
  ex->allocator = a;
  ex->graph = graph;
  ex->dirty = dirty;
  ex->num_threads = config.num_threads ? config.num_threads : 4;
  ex->enable_logging = config.enable_logging;

  sp_mutex_init(&ex->mutex, SP_MUTEX_PLAIN);
  sp_semaphore_init(&ex->work_available);

  sp_rb_init(a, ex->ready_queue);
  sp_ht_init(a, ex->completed);
  sp_ht_init(a, ex->enqueued);
  sp_da_init(a, ex->workers);
  sp_da_init(a, ex->ran);
  sp_da_init(a, ex->errors);

  return ex;
}

void spn_bg_executor_run(spn_bg_executor_t* ex) {
  ex->timer = sp_tm_start_timer();
  sp_thread_init(&ex->driver, spn_bg_driver_fn, ex);
}

void spn_bg_executor_join(spn_bg_executor_t* ex) {
  sp_thread_join(&ex->driver);
  ex->elapsed = sp_tm_read_timer(&ex->timer);
}

void spn_bg_executor_free(spn_bg_executor_t* ex) {
  if (!ex) return;
  sp_mutex_destroy(&ex->mutex);
  sp_semaphore_destroy(&ex->work_available);
  sp_mem_arena_destroy(ex->arena);
}

#endif // SP_GRAPH_IMPLEMENTATION
