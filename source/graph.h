#ifndef SPN_TEST_GRAPH_H
#define SPN_TEST_GRAPH_H

SP_TYPEDEF_FN(void, sp_it_next_fn_t, s64*);
SP_TYPEDEF_FN(bool, sp_it_check_fn_t, s64, s64);

typedef struct {
  s64 begin;
  s64 end;
  sp_it_next_fn_t on_next;
  sp_it_check_fn_t on_check;
  s64 n;
} sp_it_range_t;

void sp_it_dec(s64* it) { *it = *it - 1; }
void sp_it_inc(s64* it) { *it = *it + 1; }
bool sp_it_geq(s64 it, s64 bound) { return it >= bound; }
bool sp_it_ge(s64 it, s64 bound) { return it > bound; }
bool sp_it_leq(s64 it, s64 bound) { return it <= bound; }
bool sp_it_le(s64 it, s64 bound) { return it < bound; }
bool sp_it_eq(s64 it, s64 bound) { return it == bound; }
#define sp_it_for(it, begin, end, on_check, on_next) for (s64 it = begin; on_check(it, end); on_next(it))
#define sp_it_for_range(range) for (range.n = range.begin; range.on_check(range.n, range.end); range.on_next(&range.n))

bool sp_tm_epoch_gt(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s > b.s || (a.s == b.s && a.ns > b.ns);
}

bool sp_tm_epoch_ge(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s > b.s || (a.s == b.s && a.ns >= b.ns);
}

bool sp_tm_epoch_lt(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s < b.s || (a.s == b.s && a.ns < b.ns);
}

bool sp_tm_epoch_le(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s < b.s || (a.s == b.s && a.ns <= b.ns);
}

bool sp_tm_epoch_eq(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return a.s == b.s && a.ns == b.ns;
}

sp_tm_epoch_t sp_tm_epoch_min(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return sp_tm_epoch_lt(a, b) ? a : b;
}

sp_tm_epoch_t sp_tm_epoch_max(sp_tm_epoch_t a, sp_tm_epoch_t b) {
  return sp_tm_epoch_gt(a, b) ? a : b;
}

typedef struct {
  u32 index;
  u8 occupied;
} spn_bg_id_t;

typedef enum {
  SPN_BUILD_GRAPH_NODE_FILE,
  SPN_BUILD_GRAPH_NODE_CMD,
} spn_bg_node_kind_t;

typedef enum {
  SPN_BUILD_CMD_SUBPROCESS = 0,
  SPN_BUILD_CMD_FN = 1,
} spn_build_cmd_kind_t;

typedef struct spn_build_cmd spn_build_cmd_t;
SP_TYPEDEF_FN(void, spn_bg_fn_t, spn_build_cmd_t* cmd, void* user_data);

struct spn_build_cmd {
  spn_build_cmd_kind_t kind;
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
} spn_build_file_t;

typedef struct {
  sp_da(spn_build_file_t) files;
  sp_da(spn_build_cmd_t) commands;
} spn_build_graph_t;

typedef struct {
  u32 depth;
} spn_graph_user_data_t;

SP_TYPEDEF_FN(void, spn_bg_cmd_fn_t, spn_build_graph_t* graph, spn_build_cmd_t* cmd, void* user_data);
SP_TYPEDEF_FN(void, spn_bg_file_fn_t, spn_build_graph_t* graph, spn_build_file_t* file, void* user_data);

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
  sp_ht(spn_bg_id_t, bool) files;
  sp_ht(spn_bg_id_t, bool) commands;
  struct {
    sp_ht(spn_bg_id_t, spn_bg_dirty_file_metadata_t) files;
    sp_ht(spn_bg_id_t, spn_bg_dirty_cmd_metadata_t) commands;
  } metadata;
  sp_da(spn_bg_err_t) errors;
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


spn_build_graph_t* spn_bg_new();
spn_bg_id_t        spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
spn_bg_id_t        spn_bg_add_command(spn_build_graph_t* graph, spn_build_cmd_kind_t kind);
spn_bg_id_t        spn_bg_add_subproces(spn_build_graph_t* graph, sp_ps_config_t ps);
spn_bg_id_t        spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data);
sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph);

// FILE
spn_build_file_t*  spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
bool               spn_bg_is_file_input(spn_build_file_t* file);
void               spn_build_file_set_command(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id);
sp_str_t           spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_file_to_str(spn_build_file_t* file);

// COMMAND
spn_build_cmd_t*   spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
void               spn_bg_tag_command(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag);
void               spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag);
void               spn_build_command_set_fn(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_fn_t fn, void* user_data);
void               spn_build_command_add_output(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id);
void               spn_build_command_add_input(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id);
sp_str_t           spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id);
sp_str_t           spn_bg_cmd_to_str(spn_build_cmd_t* cmd);

// TRAVERSAL
spn_bg_it_t        spn_bg_it_new(spn_bg_it_config_t config);
spn_bg_node_t      spn_bg_it_next(spn_bg_it_t* it);
void               spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
bool               spn_bg_it_done(spn_bg_it_t* it);
sp_str_t           spn_bg_dfs(spn_bg_it_config_t config);
sp_str_t           spn_bg_bfs(spn_bg_it_config_t config);
sp_str_t           spn_bg_all_paths(spn_bg_it_config_t config);
void               spn_bg_to_mermaid(spn_build_graph_t* graph, sp_str_t path);

// DIRTY
spn_bg_dirty_t*    spn_bg_dirty_new();
spn_bg_dirty_t*    spn_bg_compute_dirty(spn_build_graph_t* graph);
bool               spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t file_id);
bool               spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t cmd_id);

// VISITED
spn_bg_visited_t*  spn_bg_visited_new();
bool               spn_bg_visited_visit(spn_bg_visited_t* visited, spn_bg_node_t id);









spn_bg_it_t spn_bg_it_new(spn_bg_it_config_t config) {
  spn_bg_it_t it = {
    .mode = config.mode,
    .direction = config.direction,
    .on_cmd = config.on_cmd,
    .on_file = config.on_file,
    .user_data = config.user_data,
    .graph = config.graph
  };

  sp_da_for(it.graph->files, n) {
    spn_build_file_t* file = it.graph->files + n;

    bool is_leaf;
    switch (it.direction) {
      case SPN_BG_ITER_DIR_IN_TO_OUT: { is_leaf = !file->producer.occupied; break; }
      case SPN_BG_ITER_DIR_OUT_TO_IN: { is_leaf = sp_da_empty(file->consumers); break; }
      default: SP_UNREACHABLE_CASE();
    }

    if (!is_leaf) continue;

    spn_bg_node_t node = {
      .kind = SPN_BUILD_GRAPH_NODE_FILE,
      .id = file->id
    };
    sp_da_push(it.nodes, node);
  }

  sp_da_for(it.graph->commands, n) {
    spn_build_cmd_t* cmd = it.graph->commands + n;

    bool is_leaf;
    switch (it.direction) {
      case SPN_BG_ITER_DIR_IN_TO_OUT: { is_leaf = sp_da_empty(cmd->consumes); break; }
      case SPN_BG_ITER_DIR_OUT_TO_IN: { is_leaf = sp_da_empty(cmd->produces); break; }
      default: SP_UNREACHABLE_CASE();
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

  SP_UNREACHABLE_RETURN(SP_ZERO_STRUCT(spn_bg_node_t));
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
      spn_build_file_t* file = spn_bg_find_file(it->graph, node.id);

      if (it->on_file) {
        it->on_file(it->graph, file, it->user_data);
      }

      break;
    }
    case SPN_BUILD_GRAPH_NODE_CMD: {
      spn_build_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

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
          spn_build_file_t* file = spn_bg_find_file(it->graph, node.id);

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
          spn_build_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

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
          spn_build_file_t* file = spn_bg_find_file(it->graph, node.id);

          sp_da_for(file->consumers, n) {
            spn_bg_node_t node = {
              .kind = SPN_BUILD_GRAPH_NODE_CMD,
              .id = file->consumers[n]
            };
            sp_da_push(it->nodes, node);
          }
          break;
        }
        case SPN_BUILD_GRAPH_NODE_CMD: {
          spn_build_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

          sp_da_for(cmd->produces, n) {
            spn_bg_node_t node = {
              .kind = SPN_BUILD_GRAPH_NODE_FILE,
              .id = cmd->produces[n]
            };
            sp_da_push(it->nodes, node);
          }
          break;
        }
      }
      break;
    }
  }
}

spn_build_graph_t* spn_bg_new() {
  spn_build_graph_t* graph = SP_ALLOC(spn_build_graph_t);
  return graph;
}

spn_bg_id_t spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path) {
  // @spader: obviously a hack
  sp_da_for(graph->files, it) {
    spn_build_file_t file = graph->files[it];
    if (sp_str_equal(file.path, path)) {
      return file.id;
    }
  }

  spn_build_file_t file = {
    .id = {
      .index = sp_da_size(graph->files),
      .occupied = true
    },
    .path = path,
  };
  sp_da_push(graph->files, file);
  return file.id;
}

spn_bg_id_t spn_bg_add_command(spn_build_graph_t* graph, spn_build_cmd_kind_t kind) {
  spn_build_cmd_t cmd = {
    .id = {
      .index = sp_da_size(graph->commands),
      .occupied = true
    },
    .kind = kind,
  };
  sp_da_push(graph->commands, cmd);
  return cmd.id;
}

spn_bg_id_t spn_bg_add_subproces(spn_build_graph_t* graph, sp_ps_config_t ps) {
  spn_bg_id_t id = spn_bg_add_command(graph, SPN_BUILD_CMD_SUBPROCESS);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  cmd->ps = sp_ps_config_copy(&ps);
  return id;
}

spn_bg_id_t spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data) {
  spn_bg_id_t id = spn_bg_add_command(graph, SPN_BUILD_CMD_FN);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  cmd->fn.on_execute = fn;
  cmd->fn.user_data = user_data;
  return id;

}

bool spn_bg_is_file_input(spn_build_file_t* file) {
  return !file->producer.occupied;
}

void spn_build_file_set_command(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id) {
  spn_build_file_t* file = spn_bg_find_file(graph, file_id);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
  file->producer = cmd_id;
  sp_da_push(cmd->produces, file_id);
}

void spn_build_command_set_fn(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_fn_t fn, void* user_data) {
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  SP_ASSERT(cmd->kind == SPN_BUILD_CMD_FN);
  cmd->fn.on_execute = fn;
  cmd->fn.user_data = user_data;
}

void spn_build_command_add_output(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  spn_build_file_t* file = spn_bg_find_file(graph, file_id);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
  SP_ASSERT(!file->producer.occupied);
  file->producer = cmd_id;
  sp_da_push(cmd->produces, file_id);
}

void spn_build_command_add_input(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  spn_build_file_t* file = spn_bg_find_file(graph, file_id);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
  sp_da_push(cmd->consumes, file_id);
  sp_da_push(file->consumers, cmd_id);
}

spn_build_file_t* spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id) {
  return graph->files + id.index;
}

spn_build_cmd_t* spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id) {
  return graph->commands + id.index;
}

void spn_bg_tag_command(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag) {
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  cmd->tag = sp_str_copy(tag);
}

void spn_bg_tag_command_c(spn_build_graph_t* graph, spn_bg_id_t id, const c8* tag) {
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  cmd->tag = sp_str_from_cstr(tag);
}

sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph) {
  sp_da(spn_bg_id_t) ids = SP_ZERO_INITIALIZE();

  sp_da_for(graph->files, it) {
    spn_build_file_t* file = graph->files + it;
    if (sp_da_empty(file->consumers)) {
      sp_da_push(ids, file->id);
    }
  }

  return ids;
}



bool spn_bg_visited_visit(spn_bg_visited_t* visited, spn_bg_node_t node) {
  switch (node.kind) {
    case SPN_BUILD_GRAPH_NODE_CMD: {
      if (sp_ht_key_exists(visited->commands, node.id)) {
        return true;
      }
      sp_ht_insert(visited->commands, node.id, true);
      return false;
    }
    case SPN_BUILD_GRAPH_NODE_FILE: {
      if (sp_ht_key_exists(visited->files, node.id)) {
        return true;
      }
      sp_ht_insert(visited->files, node.id, true);
      return false;
    }
  }

  SP_UNREACHABLE_RETURN(false);
}

spn_bg_visited_t* spn_bg_visited_new() {
  spn_bg_visited_t* visited = SP_ALLOC(spn_bg_visited_t);
  sp_ht_set_fns(visited->files, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(visited->commands, sp_ht_on_hash_key, sp_ht_on_compare_key);
  return visited;
}



sp_str_t spn_bg_traverse(spn_bg_it_config_t config) {
  spn_bg_visited_t* visited = spn_bg_visited_new();
  spn_bg_it_t it = spn_bg_it_new(config);

  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    if (spn_bg_visited_visit(visited, node)) continue;

    spn_bg_it_visit_node(&it, node);
    spn_bg_it_add_children(&it, node);
  }

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

sp_str_t spn_bg_file_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_build_file_t* file = spn_bg_find_file(graph, id);
  SP_ASSERT(file);
  return spn_bg_file_to_str(file);
}

sp_str_t spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  return spn_bg_cmd_to_str(cmd);
}

sp_str_t spn_bg_file_to_str(spn_build_file_t* file) {
  return file->path;
}

sp_str_t spn_bg_cmd_to_str(spn_build_cmd_t* cmd) {
  if (!sp_str_empty(cmd->tag)) {
    return cmd->tag;
  } else if (cmd->kind == SPN_BUILD_CMD_SUBPROCESS && !sp_str_empty(cmd->ps.command)) {
    return cmd->ps.command;
  } else {
    return sp_format("{}", SP_FMT_PTR(cmd));
  }
}

sp_str_t spn_bg_mermaid_class(sp_str_t name, sp_str_t fill, sp_str_t stroke, sp_str_t color) {
  return sp_format(
    "  classDef {} fill:{},stroke:{},color:{}\n",
    SP_FMT_STR(name),
    SP_FMT_STR(fill),
    SP_FMT_STR(stroke),
    SP_FMT_STR(color)
  );
}

void spn_bg_to_mermaid(spn_build_graph_t* graph, sp_str_t path) {
  sp_str_t stroke = sp_str_lit("#1a1a2e");
  sp_str_t color = sp_str_lit("#e0e0e0");
  sp_str_t intermediate = sp_str_lit("#606087");
  sp_str_t input = sp_str_lit("#558a89");
  sp_str_t output = sp_str_lit("#608767");
  sp_str_t cmd = sp_str_lit("#8a5555");

  sp_io_stream_t stream = sp_io_from_file(path, SP_IO_MODE_WRITE);
  sp_io_write_str(&stream, sp_str_lit("graph TD\n"));
  sp_io_write_str(&stream, spn_bg_mermaid_class(sp_str_lit("input"), input, stroke, color));
  sp_io_write_str(&stream, spn_bg_mermaid_class(sp_str_lit("output"), output, stroke, color));
  sp_io_write_str(&stream, spn_bg_mermaid_class(sp_str_lit("intermediate"), intermediate, stroke, color));
  sp_io_write_str(&stream, spn_bg_mermaid_class(sp_str_lit("cmd"), cmd, stroke, color));
  sp_io_write_str(&stream, sp_str_lit("  linkStyle default stroke:#909090,stroke-width:2px\n"));

  sp_da_for(graph->files, it) {
    spn_build_file_t* file = &graph->files[it];
    bool is_input = !file->producer.occupied;
    bool is_output = sp_da_empty(file->consumers);

    sp_str_t cls = sp_str_lit("intermediate");
    if (is_input) {
      cls = sp_str_lit("input");
    } else if (is_output) {
      cls = sp_str_lit("output");
    }

    sp_io_write_str(&stream, sp_format("  F{}[\"{}\"]:::{}\n",
      SP_FMT_U32(file->id.index), SP_FMT_STR(file->path), SP_FMT_STR(cls)));
  }

  sp_da_for(graph->commands, it) {
    spn_build_cmd_t* cmd = &graph->commands[it];
    sp_io_write_str(&stream, sp_format("  C{}[\"{}\"]:::cmd\n",
      SP_FMT_U32(cmd->id.index), SP_FMT_STR(cmd->tag)));

    sp_da_for(cmd->consumes, input_it) {
      sp_io_write_str(&stream, sp_format("  F{} --> C{}\n",
        SP_FMT_U32(cmd->consumes[input_it].index), SP_FMT_U32(cmd->id.index)));
    }

    sp_da_for(cmd->produces, output_it) {
      sp_io_write_str(&stream, sp_format("  C{} --> F{}\n",
        SP_FMT_U32(cmd->id.index), SP_FMT_U32(cmd->produces[output_it].index)));
    }
  }

  sp_io_close(&stream);
}

spn_bg_dirty_t* spn_bg_dirty_new() {
  spn_bg_dirty_t* dirty = SP_ALLOC(spn_bg_dirty_t);
  sp_ht_set_fns(dirty->files, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(dirty->commands, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(dirty->metadata.files, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(dirty->metadata.commands, sp_ht_on_hash_key, sp_ht_on_compare_key);
  return dirty;
}

spn_bg_dirty_t* spn_bg_compute_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = spn_bg_dirty_new();
  spn_bg_it_t it = spn_bg_it_new((spn_bg_it_config_t){
    .graph = graph,
    .mode = SPN_BG_ITER_MODE_BREADTH_FIRST,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
  });

  // get file mod times in a single pass
  sp_da_for(graph->files, it) {
    spn_build_file_t* file = &graph->files[it];
    spn_bg_dirty_file_metadata_t metadata = {
      .mod_time = sp_fs_get_mod_time(file->path),
      .exists = sp_fs_exists(file->path)
    };

    sp_ht_insert(dirty->metadata.files, file->id, metadata);

    if (spn_bg_is_file_input(file)) {
      if (!metadata.exists) {
        spn_bg_err_t err = {
          .kind = SPN_BG_ERR_MISSING_INPUT,
          .missing_input = { .file_id = file->id }
        };
        sp_da_push(dirty->errors, err);
      }
    }
  }

  // first pass: check each command, locally, for inputs newer than outputs
  sp_da_for(graph->commands, it) {
    spn_build_cmd_t* cmd = &graph->commands[it];
    spn_bg_dirty_cmd_metadata_t metadata = {
      .in = SP_LIMIT_EPOCH_MIN,
      .out = SP_LIMIT_EPOCH_MAX,
      .degree = SP_MAX(sp_da_size(cmd->consumes), 1)
    };

    bool missing_output = false;
    sp_da_for(cmd->produces, n) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(dirty->metadata.files, cmd->produces[n]);
      SP_ASSERT(m);
      metadata.out = sp_tm_epoch_min(m->mod_time, metadata.out);
      missing_output |= !m->exists;
    }

    sp_da_for(cmd->consumes, n) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(dirty->metadata.files, cmd->consumes[n]);
      SP_ASSERT(m);
      metadata.in = sp_tm_epoch_max(m->mod_time, metadata.in);
    }

    if (sp_tm_epoch_gt(metadata.in, metadata.out) || missing_output) {
      sp_ht_insert(dirty->commands, cmd->id, true);
    }

    sp_ht_insert(dirty->metadata.commands, cmd->id, metadata);
  }

  // second pass: bootleg kahn's, to propagate dirtiness
  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    switch (node.kind) {
      case SPN_BUILD_GRAPH_NODE_FILE: {
        spn_build_file_t* file = spn_bg_find_file(graph, node.id);
        if (sp_ht_key_exists(dirty->commands, file->producer)) {
          sp_ht_insert(dirty->files, node.id, true);
        }

        spn_bg_it_add_children(&it, node);
        break;
      }
      case SPN_BUILD_GRAPH_NODE_CMD: {
        spn_bg_dirty_cmd_metadata_t* m = sp_ht_getp(dirty->metadata.commands, node.id);
        m->degree--;

        if (!m->degree) {
          spn_build_cmd_t* cmd = spn_bg_find_command(graph, node.id);
          sp_da_for(cmd->consumes, n) {
            if (sp_ht_key_exists(dirty->files, cmd->consumes[n])) {
              sp_ht_insert(dirty->commands, node.id, true);
            }
          }

          spn_bg_it_add_children(&it, node);
        }
        break;
      }
    }

  }

  return dirty;
}

bool spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t file_id) {
  return sp_ht_key_exists(dirty->files, file_id);
}

bool spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t cmd_id) {
  return sp_ht_key_exists(dirty->commands, cmd_id);
}

// ============================================================================
// Executor
// ============================================================================

typedef struct {
  spn_bg_id_t cmd_id;
  sp_ps_output_t output;
} spn_bg_exec_error_t;

typedef struct {
  u32 num_threads;
  bool enable_logging;
} spn_bg_executor_config_t;

typedef struct {
  sp_ring_buffer_t ready_queue;
  sp_mutex_t mutex;
  sp_semaphore_t work_available;

  sp_ht(spn_bg_id_t, bool) completed;
  sp_ht(spn_bg_id_t, bool) enqueued;

  spn_build_graph_t* graph;
  spn_bg_dirty_t* dirty;
  u32 num_threads;
  bool enable_logging;
  sp_da(sp_thread_t) threads;
  sp_da(spn_bg_id_t) ran;
  sp_da(spn_bg_exec_error_t) errors;

  s32 active_workers;
  sp_atomic_s32 shutdown;
} spn_bg_executor_t;

spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config);
void spn_bg_executor_run(spn_bg_executor_t* ex);
void spn_bg_executor_join(spn_bg_executor_t* ex);
void spn_bg_executor_free(spn_bg_executor_t* ex);

void spn_bg_executor_log_cmd(spn_bg_executor_t* ex, spn_build_cmd_t* cmd) {
  if (!ex->enable_logging) return;

  sp_mutex_lock(&ex->mutex);
  if (!sp_str_empty(cmd->tag)) {
    sp_os_print(sp_format("[exec] {}\n", SP_FMT_STR(cmd->tag)));
  } else if (cmd->kind == SPN_BUILD_CMD_SUBPROCESS && !sp_str_empty(cmd->ps.command)) {
    sp_os_print(sp_format("[exec] {}\n", SP_FMT_STR(cmd->ps.command)));
  } else {
    sp_os_print(sp_format("[exec] cmd@{}\n", SP_FMT_PTR(cmd)));
  }
  sp_mutex_unlock(&ex->mutex);
}

bool spn_bg_cmd_is_ready(spn_bg_executor_t* ex, spn_bg_id_t cmd_id) {
  spn_build_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);
  sp_da_for(cmd->consumes, i) {
    spn_build_file_t* file = spn_bg_find_file(ex->graph, cmd->consumes[i]);
    if (!file->producer.occupied) continue;
    if (!sp_ht_key_exists(ex->dirty->commands, file->producer)) continue;
    if (!sp_ht_key_exists(ex->completed, file->producer)) return false;
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
    if (sp_ring_buffer_is_empty(&ex->ready_queue)) {
      sp_mutex_unlock(&ex->mutex);
      continue;
    }

    spn_bg_id_t cmd_id = *(spn_bg_id_t*)sp_ring_buffer_pop(&ex->ready_queue);
    ex->active_workers++;
    sp_mutex_unlock(&ex->mutex);

    spn_build_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);
    spn_bg_executor_log_cmd(ex, cmd);

    sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
    switch (cmd->kind) {
      case SPN_BUILD_CMD_SUBPROCESS: {
        sp_ps_output_t result = sp_ps_run(cmd->ps);
        if (result.status.exit_code) {
          sp_opt_set(error, ((spn_bg_exec_error_t) {
            .cmd_id = cmd_id,
            .output = result
          }));
        }
        break;
      }
      case SPN_BUILD_CMD_FN: {
        if (cmd->fn.on_execute) {
          cmd->fn.on_execute(cmd, cmd->fn.user_data);
        }
        break;
      }
    }

    sp_mutex_lock(&ex->mutex);
    ex->active_workers--;
    sp_ht_insert(ex->completed, cmd_id, true);
    sp_da_push(ex->ran, cmd_id);

    switch (error.some) {
      case SP_OPT_SOME: {
        sp_da_push(ex->errors, error.value);
        break;
      }
      case SP_OPT_NONE: {
        sp_da_for(cmd->produces, i) {
          spn_build_file_t* file = spn_bg_find_file(ex->graph, cmd->produces[i]);
          sp_da_for(file->consumers, j) {
            spn_bg_id_t downstream = file->consumers[j];
            if (sp_ht_key_exists(ex->enqueued, downstream)) continue;

            if (spn_bg_cmd_is_ready(ex, downstream)) {
              sp_ht_insert(ex->enqueued, downstream, true);
              sp_ring_buffer_push(&ex->ready_queue, &downstream);
              sp_semaphore_signal(&ex->work_available);
            }
          }
        }
        break;
      }
    }

    // Termination check
    if (sp_ring_buffer_is_empty(&ex->ready_queue) && ex->active_workers == 0) {
      sp_atomic_s32_set(&ex->shutdown, 1);
      sp_semaphore_signal(&ex->work_available);
    }
    sp_mutex_unlock(&ex->mutex);
  }
}

spn_bg_executor_t* spn_bg_executor_new(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, spn_bg_executor_config_t config) {
  spn_bg_executor_t* ex = SP_ALLOC(spn_bg_executor_t);
  ex->graph = graph;
  ex->dirty = dirty;
  ex->num_threads = config.num_threads ? config.num_threads : 4;
  ex->enable_logging = config.enable_logging;

  sp_mutex_init(&ex->mutex, SP_MUTEX_PLAIN);
  sp_semaphore_init(&ex->work_available);
  sp_ring_buffer_init(&ex->ready_queue, 64, sizeof(spn_bg_id_t));

  sp_ht_set_fns(ex->completed, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(ex->enqueued, sp_ht_on_hash_key, sp_ht_on_compare_key);

  return ex;
}

void spn_bg_executor_run(spn_bg_executor_t* ex) {
  if (sp_ht_empty(ex->dirty->commands)) {
    return;
  }

  sp_ht_for(ex->dirty->commands, it) {
    spn_bg_id_t* cmd_id = sp_ht_it_getkp(ex->dirty->commands, it);
    if (spn_bg_cmd_is_ready(ex, *cmd_id)) {
      sp_ht_insert(ex->enqueued, *cmd_id, true);
      sp_ring_buffer_push(&ex->ready_queue, cmd_id);
      sp_semaphore_signal(&ex->work_available);
    }
  }

  for (u32 i = 0; i < ex->num_threads; i++) {
    sp_thread_t thread;
    sp_thread_init(&thread, spn_bg_worker_fn, ex);
    sp_da_push(ex->threads, thread);
  }
}

void spn_bg_executor_join(spn_bg_executor_t* ex) {
  sp_da_for(ex->threads, i) {
    sp_thread_join(&ex->threads[i]);
  }
}

void spn_bg_executor_free(spn_bg_executor_t* ex) {
  sp_ring_buffer_destroy(&ex->ready_queue);
  sp_mutex_destroy(&ex->mutex);
  sp_semaphore_destroy(&ex->work_available);
}

#endif // SPN_TEST_GRAPH_H
