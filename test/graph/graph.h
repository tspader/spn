#ifndef SPN_TEST_GRAPH_H
#define SPN_TEST_GRAPH_H

#include "sp.h"
#include <limits.h>
#include <signal.h>
#include <stdbool.h>

#define SP_ALLOC(T) (T*)sp_alloc(sizeof(T))
#define sp_da_rfor(__ARR, __IT) for (u32 __IT = sp_dyn_array_size(__ARR); __IT-- > 0; )

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

typedef struct {
  u32 index;
  u8 occupied;
} spn_bg_id_t;

typedef enum {
  SPN_BUILD_GRAPH_NODE_FILE,
  SPN_BUILD_GRAPH_NODE_CMD,
} spn_bg_node_kind_t;

typedef enum {
  SPN_BUILD_CMD_DEP = 0,
  SPN_BUILD_CMD_SUBPROCESS = 1,
} spn_build_cmd_kind_t;

typedef struct {
  spn_build_cmd_kind_t kind;
  spn_bg_id_t id;
  sp_ps_config_t ps;
  sp_da(spn_bg_id_t) consumes;
  sp_da(spn_bg_id_t) produces;

  union {
    struct {} dep;
    struct {} subprocess;
  };
} spn_build_cmd_t;

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
  SPN_BG_ERR_MISSING_INPUT,
} spn_bg_err_kind_t;

typedef struct {
  spn_bg_err_kind_t kind;
  union {
    struct { spn_bg_id_t file_id; } missing_input;
  };
} spn_bg_err_t;

typedef struct {
  sp_ht(spn_bg_id_t, bool) files;
  sp_ht(spn_bg_id_t, bool) commands;
  sp_ht(spn_bg_id_t, sp_tm_epoch_t) mod_times;
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
  spn_bg_it_mode_t mode;
  spn_bg_it_dir_t direction;
  spn_bg_cmd_fn_t on_cmd;
  spn_bg_file_fn_t on_file;
  void* user_data;
  spn_build_graph_t* graph;
  sp_da(spn_bg_node_t) nodes;
  u32 index;
} spn_bg_it_t;


spn_bg_visited_t* spn_bg_visited_new();
bool spn_bg_visited_visit(spn_bg_visited_t* visited, spn_bg_node_t id);

// iterator struct w/ visited list on it
// clean up all these:
/*
        if (sp_ht_key_exists(visited.files, node.id)) continue;
        sp_ht_insert(visited.files, node.id, true);
*/
// also add the queue to that struct
// all paths is just dfs + dont check visited

void spn_bg_all_paths_file(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);
void spn_bg_all_paths_cmd(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);

spn_bg_it_t spn_bg_it_new(spn_build_graph_t* graph, spn_bg_it_mode_t mode, spn_bg_it_dir_t dir, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);
spn_bg_node_t spn_bg_it_next(spn_bg_it_t* it);
void spn_bg_it_add_children(spn_bg_it_t* it, spn_bg_node_t node);
bool spn_bg_it_done(spn_bg_it_t* it);
spn_build_graph_t* spn_bg_new();
spn_bg_id_t spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path);
spn_bg_id_t spn_bg_add_command(spn_build_graph_t* graph, spn_build_cmd_kind_t kind);
void spn_build_file_set_command(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id);
void spn_build_command_add_input(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id);
spn_build_file_t* spn_bg_find_file(spn_build_graph_t* graph, spn_bg_id_t id);
spn_build_cmd_t* spn_bg_find_command(spn_build_graph_t* graph, spn_bg_id_t id);
sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph);
sp_str_t spn_bg_dfs(spn_build_graph_t* graph, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);
sp_str_t spn_bg_bfs(spn_build_graph_t* graph, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);
sp_str_t spn_bg_all_paths(spn_build_graph_t* graph, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data);
void spn_bg_to_mermaid(spn_build_graph_t* graph, sp_str_t path);

spn_bg_dirty_t* spn_bg_compute_dirty(spn_build_graph_t* graph);
bool spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t file_id);
bool spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t cmd_id);










spn_bg_it_t spn_bg_it_new(spn_build_graph_t* graph, spn_bg_it_mode_t mode, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data) {
  spn_bg_it_t it = {
    .mode = mode,
    .direction = direction,
    .on_cmd = on_cmd,
    .on_file = on_file,
    .user_data = user_data,
    .graph = graph
  };

  sp_da_for(graph->files, n) {
    spn_build_file_t* file = graph->files + n;

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

  sp_da_for(graph->commands, n) {
    spn_build_cmd_t* cmd = graph->commands + n;

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
              .id = file->producer.index
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
  spn_build_graph_t* graph = calloc(1, sizeof(spn_build_graph_t));
  return graph;
}

spn_bg_id_t spn_bg_add_file(spn_build_graph_t* graph, sp_str_t path) {
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

void spn_build_file_set_command(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id) {
  spn_build_file_t* file = spn_bg_find_file(graph, file_id);
  spn_build_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
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



sp_str_t spn_bg_traverse(spn_build_graph_t* graph, spn_bg_it_mode_t mode, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data) {
  spn_bg_visited_t* visited = spn_bg_visited_new();
  spn_bg_it_t it = spn_bg_it_new(graph, mode, direction, on_cmd, on_file, user_data);

  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    if (spn_bg_visited_visit(visited, node)) continue;

    spn_bg_it_visit_node(&it, node);
    spn_bg_it_add_children(&it, node);
  }

  return sp_str_lit("");
}

sp_str_t spn_bg_dfs(spn_build_graph_t* graph, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data) {
  spn_bg_traverse(graph, SPN_BG_ITER_MODE_DEPTH_FIRST, direction, on_cmd, on_file, user_data);
}

sp_str_t spn_bg_bfs(spn_build_graph_t* graph, spn_bg_it_dir_t direction, spn_bg_cmd_fn_t on_cmd, spn_bg_file_fn_t on_file, void* user_data) {
  spn_bg_traverse(graph, SPN_BG_ITER_MODE_BREADTH_FIRST, direction, on_cmd, on_file, user_data);
}



void spn_bg_to_mermaid(spn_build_graph_t* graph, sp_str_t path) {
  sp_io_stream_t stream = sp_io_from_file(path, SP_IO_MODE_WRITE);

  sp_io_write_str(&stream, sp_str_lit("graph TD\n"));
  sp_io_write_str(&stream, sp_str_lit("  classDef input fill:#4a3828,stroke:#3a2818,color:#e8e8e8\n"));
  sp_io_write_str(&stream, sp_str_lit("  classDef output fill:#2e4038,stroke:#1e3028,color:#e8e8e8\n"));
  sp_io_write_str(&stream, sp_str_lit("  classDef intermediate fill:#4a4428,stroke:#3a3418,color:#e8e8e8\n"));
  sp_io_write_str(&stream, sp_str_lit("  classDef cmd fill:#2e3848,stroke:#1e2838,color:#e8e8e8\n"));
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
      SP_FMT_U32(cmd->id.index), SP_FMT_STR(cmd->ps.command)));

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

spn_bg_dirty_t* spn_bg_compute_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = SP_ALLOC(spn_bg_dirty_t);
  sp_ht_set_fns(dirty->files, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(dirty->commands, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(dirty->mod_times, sp_ht_on_hash_key, sp_ht_on_compare_key);

  spn_bg_visited_t* visited = spn_bg_visited_new();
  spn_bg_it_t it = spn_bg_it_new(graph, SPN_BG_ITER_MODE_BREADTH_FIRST, SPN_BG_ITER_DIR_IN_TO_OUT, SP_NULLPTR, SP_NULLPTR, SP_NULLPTR);

  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    if (spn_bg_visited_visit(visited, node)) continue;

    switch (node.kind) {
      case SPN_BUILD_GRAPH_NODE_FILE: {
        spn_build_file_t* file = spn_bg_find_file(graph, node.id);

        if (!file->producer.occupied) {
          // Source file - must exist
          if (!sp_os_does_path_exist(file->path)) {
            spn_bg_err_t err = {
              .kind = SPN_BG_ERR_MISSING_INPUT,
              .missing_input = { .file_id = node.id }
            };
            sp_da_push(dirty->errors, err);
          } else {
            sp_tm_epoch_t mtime = sp_os_file_mod_time_precise(file->path);
            sp_ht_insert(dirty->mod_times, node.id, mtime);
          }
        } else {
          // Produced file - dirty if producer is dirty
          if (sp_ht_key_exists(dirty->commands, file->producer)) {
            sp_ht_insert(dirty->files, node.id, true);
          }
        }
        break;
      }
      case SPN_BUILD_GRAPH_NODE_CMD: {
        spn_build_cmd_t* cmd = spn_bg_find_command(graph, node.id);
        bool is_dirty = false;

        // Check if any input is dirty
        sp_da_for(cmd->consumes, i) {
          if (sp_ht_key_exists(dirty->files, cmd->consumes[i])) {
            is_dirty = true;
            break;
          }
        }

        // Check if any output is missing, and cache mod times
        if (!is_dirty) {
          sp_da_for(cmd->produces, i) {
            spn_build_file_t* output = spn_bg_find_file(graph, cmd->produces[i]);
            if (!sp_os_does_path_exist(output->path)) {
              is_dirty = true;
              break;
            }
            sp_tm_epoch_t mtime = sp_os_file_mod_time_precise(output->path);
            sp_ht_insert(dirty->mod_times, cmd->produces[i], mtime);
          }
        }

        // Check if any input is newer than any output
        if (!is_dirty) {
          sp_da_for(cmd->consumes, i) {
            sp_tm_epoch_t* input_mtime = sp_ht_getp(dirty->mod_times, cmd->consumes[i]);
            if (!input_mtime) continue;

            sp_da_for(cmd->produces, j) {
              sp_tm_epoch_t* output_mtime = sp_ht_getp(dirty->mod_times, cmd->produces[j]);
              if (!output_mtime) continue;

              if (input_mtime->s > output_mtime->s ||
                  (input_mtime->s == output_mtime->s && input_mtime->ns > output_mtime->ns)) {
                is_dirty = true;
                break;
              }
            }
            if (is_dirty) break;
          }
        }

        if (is_dirty) {
          sp_ht_insert(dirty->commands, node.id, true);
        }
        break;
      }
    }

    spn_bg_it_add_children(&it, node);
  }

  return dirty;
}

bool spn_bg_is_file_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t file_id) {
  return sp_ht_key_exists(dirty->files, file_id);
}

bool spn_bg_is_cmd_dirty(spn_bg_dirty_t* dirty, spn_bg_id_t cmd_id) {
  return sp_ht_key_exists(dirty->commands, cmd_id);
}

#endif // SPN_TEST_GRAPH_H
