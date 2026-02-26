#include "graph.h"

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
    spn_bg_file_t* file = it.graph->files + n;

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
    spn_bg_cmd_t* cmd = it.graph->commands + n;

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
            spn_bg_node_t node = {
              .kind = SPN_BUILD_GRAPH_NODE_CMD,
              .id = file->consumers[n]
            };
            sp_da_push(it->nodes, node);
          }
          break;
        }
        case SPN_BUILD_GRAPH_NODE_CMD: {
          spn_bg_cmd_t* cmd = spn_bg_find_command(it->graph, node.id);

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
    spn_bg_file_t file = graph->files[it];
    if (sp_str_equal(file.path, path)) {
      return file.id;
    }
  }

  spn_bg_file_t file = {
    .id = {
      .index = sp_da_size(graph->files),
      .occupied = true
    },
    .path = path,
  };
  sp_da_push(graph->files, file);
  return file.id;
}


spn_bg_id_t spn_bg_add_file_c(spn_build_graph_t* graph, spn_bg_file_config_t config) {
  spn_bg_id_t id = spn_bg_add_file(graph, config.path);
  spn_bg_file_set_metadata_ex(graph, id, config.metadata);
  return id;
}

spn_bg_id_t spn_bg_add_file_ex(spn_build_graph_t* graph, sp_str_t path, spn_bg_viz_kind_t viz, sp_str_t package) {
  return spn_bg_add_file_c(graph, (spn_bg_file_config_t) {
    .path = path,
    .metadata = {
      .package = package,
      .viz = viz
    }
  });
}

void spn_bg_file_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata) {
  spn_bg_file_set_metadata(graph, id, metadata.package, metadata.viz);
}

void spn_bg_file_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t package, spn_bg_viz_kind_t viz) {
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  sp_assert(file);

  file->package = package;
  file->viz = viz;
}

spn_bg_id_t spn_bg_add_fn_ex(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data, spn_bg_viz_kind_t viz, sp_str_t package, sp_str_t tag) {
  return spn_bg_add_fn_c(graph, (spn_bg_cmd_config_t) {
    .fn = fn,
    .user_data = user_data,
    .metadata = {
      .viz = viz,
      .package = package,
      .tag = tag
    }
  });
}

spn_bg_id_t spn_bg_add_fn_c(spn_build_graph_t* graph, spn_bg_cmd_config_t config) {
  spn_bg_id_t id = spn_bg_add_fn(graph, config.fn, config.user_data);
  spn_bg_cmd_set_metadata_ex(graph, id, config.metadata);
  return id;
}

spn_bg_id_t spn_bg_add_fn(spn_build_graph_t* graph, spn_bg_fn_t fn, void* user_data) {
  spn_bg_id_t id = {
    .index = sp_da_size(graph->commands),
    .occupied = true
  };

  sp_da_push(graph->commands, ((spn_bg_cmd_t) {
    .id = id,
    .fn = {
      .on_execute = fn,
      .user_data = user_data
    },
    .kind = SPN_BUILD_CMD_FN,
  }));

  return id;
}

void spn_bg_cmd_set_metadata_ex(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_node_metadata_t metadata) {
  spn_bg_cmd_set_metadata(graph, id, metadata.tag, metadata.package, metadata.viz);
}

void spn_bg_cmd_set_metadata(spn_build_graph_t* graph, spn_bg_id_t id, sp_str_t tag, sp_str_t package, spn_bg_viz_kind_t viz) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  sp_assert(cmd);
  sp_assert(!sp_str_empty(tag));

  if (sp_str_empty(package)) {
    cmd->tag = sp_str_copy(tag);
  }
  else {
    cmd->tag = sp_str_join(package, tag, sp_str_lit("::"));
  }

  cmd->package = sp_str_copy(package);
  cmd->viz = viz;
}

bool spn_bg_is_file_input(spn_bg_file_t* file) {
  return !file->producer.occupied;
}

spn_err_t spn_bg_file_set_producer(spn_build_graph_t* graph, spn_bg_id_t file_id, spn_bg_id_t cmd_id) {
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

    return SPN_ERROR;
  }

  file->producer = cmd_id;
  sp_da_push(cmd->produces, file_id);
  return SPN_OK;
}

void spn_bg_cmd_set_fn(spn_build_graph_t* graph, spn_bg_id_t id, spn_bg_fn_t fn, void* user_data) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  SP_ASSERT(cmd->kind == SPN_BUILD_CMD_FN);
  cmd->fn.on_execute = fn;
  cmd->fn.user_data = user_data;
}

spn_err_t spn_bg_cmd_add_output(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  sp_assert(cmd_id.occupied);
  sp_assert(file_id.occupied);
  sp_try(spn_bg_file_set_producer(graph, file_id, cmd_id));
  return SPN_OK;
}

spn_err_t spn_bg_cmd_add_input(spn_build_graph_t* graph, spn_bg_id_t cmd_id, spn_bg_id_t file_id) {
  sp_assert(cmd_id.occupied);
  sp_assert(file_id.occupied);
  spn_bg_file_t* file = spn_bg_find_file(graph, file_id);
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, cmd_id);
  sp_da_push(cmd->consumes, file_id);
  sp_da_push(file->consumers, cmd_id);
  return SPN_OK;
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
  cmd->tag = sp_str_from_cstr(tag);
}

sp_da(spn_bg_id_t) spn_bg_find_outputs(spn_build_graph_t* graph) {
  sp_da(spn_bg_id_t) ids = SP_ZERO_INITIALIZE();

  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = graph->files + it;
    if (sp_da_empty(file->consumers)) {
      sp_da_push(ids, file->id);
    }
  }

  return ids;
}



bool spn_bg_visited_mark(spn_bg_visited_t* visited, spn_bg_node_t node) {
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

    if (spn_bg_visited_mark(visited, node)) continue;

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
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  SP_ASSERT(file);
  return spn_bg_file_to_str(file);
}

sp_str_t spn_bg_cmd_id_to_str(spn_build_graph_t* graph, spn_bg_id_t id) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);
  SP_ASSERT(cmd);
  return spn_bg_cmd_to_str(cmd);
}

sp_str_t spn_bg_file_to_str(spn_bg_file_t* file) {
  return file->path;
}

sp_str_t spn_bg_cmd_to_str(spn_bg_cmd_t* cmd) {
  if (!sp_str_empty(cmd->tag)) {
    return cmd->tag;
  } else if (cmd->kind == SPN_BUILD_CMD_SUBPROCESS && !sp_str_empty(cmd->ps.command)) {
    return cmd->ps.command;
  } else {
    return sp_format("{}", SP_FMT_PTR(cmd));
  }
}

sp_str_t spn_bg_err_to_str(spn_build_graph_t* graph, spn_bg_err_t err) {
  switch (err.kind) {
    case SPN_BG_OK: {
      return sp_str_lit("ok");
    }
    case SPN_BG_ERR_MISSING_INPUT: {
      return sp_format("missing build graph input {}", SP_FMT_STR(spn_bg_file_id_to_str(graph, err.missing_input.file_id)));
    }
    case SPN_BG_ERR_DUPLICATE_OUTPUT: {
      return sp_format(
        "{:fg brightred}: {:fg brightblack}\nfile: {}\n{}\n{}",
        SP_FMT_CSTR("error"),
        SP_FMT_CSTR("two graph nodes output the same file"),
        SP_FMT_STR(spn_bg_file_id_to_str(graph, err.duplicate_output.file)),
        SP_FMT_STR(spn_bg_cmd_id_to_str(graph, err.duplicate_output.cmds.a)),
        SP_FMT_STR(spn_bg_cmd_id_to_str(graph, err.duplicate_output.cmds.b))
      );
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit("unknown build graph error"));
}

sp_str_t spn_bg_mermaid_class(sp_str_t name, sp_str_t fill, sp_str_t stroke, sp_str_t color) {
  return sp_format(
    "  classDef {} fill:{},stroke:{},color:{},white-space:nowrap,rx:16,ry:16\n",
    SP_FMT_STR(name),
    SP_FMT_STR(fill),
    SP_FMT_STR(stroke),
    SP_FMT_STR(color)
  );
}

sp_str_t spn_bg_mermaid_class_ex(sp_str_t name, sp_str_t fill, sp_str_t stroke, sp_str_t color, sp_str_t font_size) {
  return sp_format(
    "  classDef {} fill:{},stroke:{},color:{},font-size:{},white-space:nowrap,rx:16,ry:16\n",
    SP_FMT_STR(name),
    SP_FMT_STR(fill),
    SP_FMT_STR(stroke),
    SP_FMT_STR(color),
    SP_FMT_STR(font_size)
  );
}

sp_str_t spn_bg_viz_kind_to_class(spn_bg_viz_kind_t kind) {
  switch (kind) {
    case SPN_BG_VIZ_MANIFEST: return sp_str_lit("manifest");
    case SPN_BG_VIZ_STAMP:    return sp_str_lit("stamp");
    case SPN_BG_VIZ_BINARY:   return sp_str_lit("binary");
    case SPN_BG_VIZ_SOURCE:   return sp_str_lit("source");
    case SPN_BG_VIZ_CMD:      return sp_str_lit("cmd");
    case SPN_BG_VIZ_DEFAULT:  return sp_str_lit("source");
  }
  return sp_str_lit("source");
}

sp_str_t spn_bg_file_dirty_class(spn_bg_dirty_t* dirty, spn_bg_id_t id) {
  return spn_bg_is_file_dirty(dirty, id) ? sp_str_lit("dirty") : sp_str_lit("clean");
}

sp_str_t spn_bg_cmd_dirty_class(spn_bg_dirty_t* dirty, spn_bg_id_t id) {
  return spn_bg_is_cmd_dirty(dirty, id) ? sp_str_lit("dirty") : sp_str_lit("clean");
}

sp_str_t spn_bg_mermaid_shorten_path(sp_str_t path, sp_str_t project_dir, sp_str_t cache_dir, sp_str_t work_dir, sp_str_t store_dir) {
  // Try most specific paths first (work/store are subdirs of project)
  if (sp_str_valid(work_dir) && sp_str_starts_with(path, work_dir)) {
    return sp_format("$WORK{}", SP_FMT_STR(sp_str_suffix(path, path.len - work_dir.len)));
  } else if (sp_str_valid(store_dir) && sp_str_starts_with(path, store_dir)) {
    return sp_format("$STORE{}", SP_FMT_STR(sp_str_suffix(path, path.len - store_dir.len)));
  } else if (sp_str_valid(project_dir) && sp_str_starts_with(path, project_dir)) {
    return sp_format("$PROJECT{}", SP_FMT_STR(sp_str_suffix(path, path.len - project_dir.len)));
  } else if (sp_str_valid(cache_dir) && sp_str_starts_with(path, cache_dir)) {
    return sp_format("$CACHE{}", SP_FMT_STR(sp_str_suffix(path, path.len - cache_dir.len)));
  }
  return path;
}

void spn_bg_to_mermaid(spn_build_graph_t* graph, spn_bg_dirty_t* dirty, sp_io_writer_t* io, sp_str_t project_dir, sp_str_t cache_dir, sp_str_t work_dir, sp_str_t store_dir) {
  sp_str_t stroke = sp_str_lit("#1a1a2e");
  sp_str_t text = sp_str_lit("#e0e0e0");

  sp_io_write_str(io, sp_str_lit("graph TD\n"));

  if (dirty) {
    // Dirty mode: color by dirtiness
    sp_str_t dirty_color = sp_str_lit("#a36565");  // muted red - needs rebuild
    sp_str_t clean_color = sp_str_lit("#65a365");  // muted green - up to date
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("dirty"), dirty_color, stroke, text));
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("clean"), clean_color, stroke, text));
  } else {
    // Default mode: color by viz kind
    sp_str_t manifest = sp_str_lit("#65a3a3");  // manifests + build scripts (cyan)
    sp_str_t cmd_color = sp_str_lit("#a36565"); // commands (muted red)
    sp_str_t stamp = sp_str_lit("#8565a3");     // stamps (purple)
    sp_str_t binary = sp_str_lit("#65a365");    // target binaries (green)
    sp_str_t source = sp_str_lit("#a39a65");    // source files (yellow/orange)
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("manifest"), manifest, stroke, text));
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("cmd"), cmd_color, stroke, text));
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("stamp"), stamp, stroke, text));
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("binary"), binary, stroke, text));
    sp_io_write_str(io, spn_bg_mermaid_class(sp_str_lit("source"), source, stroke, text));
  }
  sp_io_write_str(io, sp_str_lit("  linkStyle default stroke:#909090,stroke-width:2px\n"));

  // Collect unique package names
  sp_ht(sp_str_t, bool) packages = SP_ZERO_INITIALIZE();
  sp_ht_set_fns(packages, sp_ht_on_hash_str_key, sp_ht_on_compare_str_key);

  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = &graph->files[it];
    if (!sp_str_empty(file->package) && !sp_ht_key_exists(packages, file->package)) {
      sp_ht_insert(packages, file->package, true);
    }
  }
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];
    if (!sp_str_empty(cmd->package) && !sp_ht_key_exists(packages, cmd->package)) {
      sp_ht_insert(packages, cmd->package, true);
    }
  }

  // Emit subgraphs per package
  sp_ht_for(packages, pkg_it) {
    sp_str_t pkg_name = *sp_ht_it_getkp(packages, pkg_it);
    sp_io_write_str(io, sp_format("  subgraph {}[{}]\n", SP_FMT_STR(pkg_name), SP_FMT_STR(pkg_name)));

    // Emit files belonging to this package
    sp_da_for(graph->files, it) {
      spn_bg_file_t* file = &graph->files[it];
      if (!sp_str_equal(file->package, pkg_name)) {
        continue;
      }
      sp_str_t cls = dirty ? spn_bg_file_dirty_class(dirty, file->id) : spn_bg_viz_kind_to_class(file->viz);
      sp_str_t path = spn_bg_mermaid_shorten_path(file->path, project_dir, cache_dir, work_dir, store_dir);
      sp_io_write_str(io, sp_format("    F{}[\"{}\"]:::{}\n",
        SP_FMT_U32(file->id.index), SP_FMT_STR(path), SP_FMT_STR(cls)));
    }

    // Emit commands belonging to this package
    sp_da_for(graph->commands, it) {
      spn_bg_cmd_t* cmd = &graph->commands[it];
      if (!sp_str_equal(cmd->package, pkg_name)) {
        continue;
      }
      sp_str_t cls = dirty ? spn_bg_cmd_dirty_class(dirty, cmd->id) : spn_bg_viz_kind_to_class(cmd->viz);
      sp_io_write_str(io, sp_format("    C{}[\"{}\"]:::{}\n",
        SP_FMT_U32(cmd->id.index), SP_FMT_STR(cmd->tag), SP_FMT_STR(cls)));
    }

    sp_io_write_str(io, sp_str_lit("  end\n"));
  }

  // Emit orphan nodes (no package assigned)
  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = &graph->files[it];
    if (!sp_str_empty(file->package)) {
      continue;
    }
    sp_str_t cls = dirty ? spn_bg_file_dirty_class(dirty, file->id) : spn_bg_viz_kind_to_class(file->viz);
    sp_str_t path = spn_bg_mermaid_shorten_path(file->path, project_dir, cache_dir, work_dir, store_dir);
    sp_io_write_str(io, sp_format("  F{}[\"{}\"]:::{}\n",
      SP_FMT_U32(file->id.index), SP_FMT_STR(path), SP_FMT_STR(cls)));
  }
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];
    if (!sp_str_empty(cmd->package)) {
      continue;
    }
    sp_str_t cls = dirty ? spn_bg_cmd_dirty_class(dirty, cmd->id) : spn_bg_viz_kind_to_class(cmd->viz);
    sp_io_write_str(io, sp_format("  C{}[\"{}\"]:::{}\n",
      SP_FMT_U32(cmd->id.index), SP_FMT_STR(cmd->tag), SP_FMT_STR(cls)));
  }

  // Emit all edges (outside subgraphs)
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];

    sp_da_for(cmd->consumes, input_it) {
      sp_io_write_str(io, sp_format("  F{} --> C{}\n",
        SP_FMT_U32(cmd->consumes[input_it].index), SP_FMT_U32(cmd->id.index)));
    }

    sp_da_for(cmd->produces, output_it) {
      sp_io_write_str(io, sp_format("  C{} --> F{}\n",
        SP_FMT_U32(cmd->id.index), SP_FMT_U32(cmd->produces[output_it].index)));
    }
  }
}

spn_bg_dirty_t* spn_bg_dirty_new() {
  spn_bg_dirty_t* dirty = SP_ALLOC(spn_bg_dirty_t);
  return dirty;
}

spn_bg_dirty_t* spn_bg_compute_forced_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = spn_bg_dirty_new();

  sp_da_for(graph->files, it) {
    sp_ht_insert(dirty->files, graph->files[it].id, true);
  }

  sp_da_for(graph->commands, it) {
    sp_ht_insert(dirty->commands, graph->commands[it].id, true);
  }

  return dirty;
}

spn_bg_dirty_t* spn_bg_compute_dirty(spn_build_graph_t* graph) {
  spn_bg_dirty_t* dirty = spn_bg_dirty_new();
  spn_bg_dirty_metadata_t* metadata = &dirty->metadata;

  spn_bg_it_t it = spn_bg_it_new((spn_bg_it_config_t){
    .graph = graph,
    .mode = SPN_BG_ITER_MODE_BREADTH_FIRST,
    .direction = SPN_BG_ITER_DIR_IN_TO_OUT,
  });

  // get file mod times in a single pass
  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = &graph->files[it];
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
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];
    spn_bg_dirty_cmd_metadata_t metadatum = {
      .in = SP_LIMIT_EPOCH_MIN,
      .out = SP_LIMIT_EPOCH_MAX,
      .degree = SP_MAX(sp_da_size(cmd->consumes), 1)
    };

    bool missing_output = false;
    sp_da_for(cmd->produces, n) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(metadata->files, cmd->produces[n]);
      SP_ASSERT(m);
      metadatum.out = sp_tm_epoch_min(m->mod_time, metadatum.out);
      missing_output |= !m->exists;
    }

    sp_da_for(cmd->consumes, n) {
      spn_bg_dirty_file_metadata_t* m = sp_ht_getp(metadata->files, cmd->consumes[n]);
      SP_ASSERT(m);
      metadatum.in = sp_tm_epoch_max(m->mod_time, metadatum.in);
    }

    if (sp_tm_epoch_gt(metadatum.in, metadatum.out) || missing_output) {
      sp_ht_insert(dirty->commands, cmd->id, true);
    }

    sp_ht_insert(metadata->commands, cmd->id, metadatum);
  }

  // second pass: bootleg kahn's, to propagate dirtiness
  while (!spn_bg_it_done(&it)) {
    spn_bg_node_t node = spn_bg_it_next(&it);

    switch (node.kind) {
      case SPN_BUILD_GRAPH_NODE_FILE: {
        spn_bg_file_t* file = spn_bg_find_file(graph, node.id);
        if (sp_ht_key_exists(dirty->commands, file->producer)) {
          sp_ht_insert(dirty->files, node.id, true);
        }

        spn_bg_it_add_children(&it, node);
        break;
      }
      case SPN_BUILD_GRAPH_NODE_CMD: {
        spn_bg_dirty_cmd_metadata_t* m = sp_ht_getp(metadata->commands, node.id);
        m->degree--;

        if (!m->degree) {
          spn_bg_cmd_t* cmd = spn_bg_find_command(graph, node.id);
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



void spn_bg_executor_log_cmd(spn_bg_executor_t* ex, spn_bg_cmd_t* cmd) {
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
  spn_bg_cmd_t* cmd = spn_bg_find_command(ex->graph, cmd_id);
  sp_da_for(cmd->consumes, i) {
    spn_bg_file_t* file = spn_bg_find_file(ex->graph, cmd->consumes[i]);
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

    sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
    switch (cmd->kind) {
      case SPN_BUILD_CMD_SUBPROCESS: {
        sp_ps_output_t result = sp_ps_run(cmd->ps);
        if (result.status.exit_code) {
          sp_opt_set(error, ((spn_bg_exec_error_t) {
            .cmd_id = cmd_id,
            .result = result.status.exit_code
          }));
        }
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
            if (!sp_ht_key_exists(ex->enqueued, downstream)) {
              if (spn_bg_cmd_is_ready(ex, downstream)) {
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
  ex->timer = sp_tm_start_timer();

  if (sp_ht_empty(ex->dirty->commands)) {
    sp_atomic_s32_set(&ex->shutdown, 1);
    return 0;
  }

  sp_ht_for_kv(ex->dirty->commands, it) {
    if (spn_bg_cmd_is_ready(ex, *it.key)) {
      sp_ht_insert(ex->enqueued, *it.key, true);
      sp_rb_push(ex->ready_queue, *it.key);
      sp_semaphore_signal(&ex->work_available);
    }
  }

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
  spn_bg_executor_t* ex = SP_ALLOC(spn_bg_executor_t);
  ex->graph = graph;
  ex->dirty = dirty;
  ex->num_threads = config.num_threads ? config.num_threads : 4;
  ex->enable_logging = config.enable_logging;

  sp_mutex_init(&ex->mutex, SP_MUTEX_PLAIN);
  sp_semaphore_init(&ex->work_available);

  sp_ht_set_fns(ex->completed, sp_ht_on_hash_key, sp_ht_on_compare_key);
  sp_ht_set_fns(ex->enqueued, sp_ht_on_hash_key, sp_ht_on_compare_key);

  return ex;
}

void spn_bg_executor_run(spn_bg_executor_t* ex) {
  sp_thread_init(&ex->driver, spn_bg_driver_fn, ex);
}

void spn_bg_executor_join(spn_bg_executor_t* ex) {
  ex->elapsed = sp_tm_read_timer(&ex->timer);
  sp_thread_join(&ex->driver);
}

void spn_bg_executor_free(spn_bg_executor_t* ex) {
  sp_rb_free(ex->ready_queue);
  sp_mutex_destroy(&ex->mutex);
  sp_semaphore_destroy(&ex->work_available);
}
