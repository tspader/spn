#include "ctx/types.h"
#include "event/types.h"
#include "spn.h"
#include "target/types.h"
#include "unit/types.h"

#include "app/app.h"
#include "error/types.h"
#include "graph/graph.h"
#include "event/event.h"
#include "session/session.h"
#include "sp/glob.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"
#include "task/task.h"
#include "unit/package.h"


static spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph);
static spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path);
static spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit);

spn_err_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // Add all nodes to the build graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_try(add_package(graph, unit));
  }

  // Link intra-package dependencies
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);
    sp_da_for(target->deps.target, it) {

    }
  }

  // Link inter-package dependencies; we don't do anything more granular than entire packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* pkg = sp_om_at(session->units.packages, it);
    sp_da_for(pkg->deps, it) {

    }
  }

  return SPN_OK;
}

spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph) {
  if (!graph->error.some) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_BUILD_GRAPH,
      .build_graph = {
        .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
      },
    };
  }

  switch (graph->error.value.kind) {
    case SPN_BG_OK: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
        },
      };
    }
    case SPN_BG_ERR_MISSING_INPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_MISSING_INPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.missing_input.file_id),
        },
      };
    }
    case SPN_BG_ERR_DUPLICATE_OUTPUT: {
      return (spn_err_union_t) {
        .kind = SPN_ERR_BUILD_GRAPH,
        .build_graph = {
          .kind = SPN_BUILD_GRAPH_ERR_DUPLICATE_OUTPUT,
          .file = spn_bg_file_id_to_str(graph, graph->error.value.duplicate_output.file),
          .command_a = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.a),
          .command_b = spn_bg_cmd_id_to_str(graph, graph->error.value.duplicate_output.cmds.b),
        },
      };
    }
  }

  return (spn_err_union_t) {
    .kind = SPN_ERR_BUILD_GRAPH,
    .build_graph = {
      .kind = SPN_BUILD_GRAPH_ERR_UNKNOWN,
    },
  };
}

// tomorrow:
// - requested -> resolved -> loaded -> unit
// - then from the units we set up the graph
// - might need various views over the base data of "what units exist"
// - the session should probably own every single unit, when a unit wants to refer to another
//   unit for ergonomics it should hold a pointer, or sp_om(unit*)
// - working on taking deps = ["spum", "sqlite"] and resolving those into the units for the spum target and the sqlite package
// - if it's a target in your package, add the specific link
// - but if it's a whole package just use the package-to-package links
// - can add a little struct for a linkage request and in cc code switch?
//
// for (linkage_requests) {
//   switch (type) {
//     case target: {}
//     case package: {}
//   }
// }
//
// idk, might be easier to just iterate the package's targets here and have an array(target)

spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path) {
  spn_bg_id_t* existing = sp_str_ht_get(ctx->nodes.files, path);
  if (existing) return *existing;

  spn_bg_id_t id = spn_bg_add_file(graph, path);
  sp_str_ht_insert(ctx->nodes.files, path, id);
  sp_da_push(ctx->nodes.build.user, id);
  return id;
}


//                    ┌───────┐   ┌────────────────┐   ┌────────┐
//                    │ foo.c │──▶│ compile::foo.c │──▶│ foo.o  │──┐
//                    └───────┘   └────────────────┘   └────────┘  │
//                                                                 │  ┌──────┐   ┌───────────────────┐   ┌─────────────────┐
//                                                                 ├─▶│ link │──▶│ $STORE/bin/foobar │──▶│ script::package │
//                    ┌───────┐   ┌────────────────┐   ┌────────┐  │  └──────┘   └───────────────────┘   └─────────────────┘
//                    │ bar.c │──▶│ compile::bar.c │──▶│ bar.o  │──┘     ▲
//                    └───────┘   └────────────────┘   └────────┘        │
//                                                                       │
// ┌ ─ ─ ─ ─ ─ ─ ┐    ┌─────────────┐                                    │
//   user graph   ───▶│ user::exit  │────────────────────────────────────┘
// └ ─ ─ ─ ─ ─ ─ ┘    └─────────────┘
spn_err_t add_target(spn_build_graph_t* graph, spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  spn_target_t* info = target->info;

  target->nodes.link = spn_bg_add_fn(graph, link_target, target);
  target->nodes.output = spn_bg_add_file(graph, get_target_output_path(target));

  spn_bg_cmd_add_output(graph, target->nodes.link,  target->nodes.output);
  sp_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, target->nodes.output));

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* object = target->objects[it];
    sp_try(spn_bg_cmd_add_input(graph, target->nodes.link, object->nodes.object));
  }

  if (!sp_da_empty(info->embed)) {
    target->nodes.embed.run = spn_bg_add_fn(graph, compile_embed, target);
    target->nodes.embed.object = spn_bg_add_file(graph, get_embed_object_path(target));
    target->nodes.embed.header = spn_bg_add_file(graph, get_embed_header_path(target));

    sp_try(spn_bg_cmd_add_output(graph, target->nodes.embed.run, target->nodes.embed.object));
    sp_try(spn_bg_cmd_add_output(graph, target->nodes.embed.run, target->nodes.embed.header));
    sp_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, pkg->nodes.build.stamp.exit));

    sp_da_for(info->embed, it) {
      spn_embed_t* embed = &info->embed[it];

      switch (embed->kind) {
        case SPN_EMBED_FILE: {
          spn_bg_id_t input = get_or_put_user_file(pkg, graph, embed->file.path);
          sp_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, input));
          break;
        }
        case SPN_EMBED_MEM:
        case SPN_EMBED_DIR: {
          break;
        }
      }
    }

    sp_try(spn_bg_cmd_add_input(graph, target->nodes.link, target->nodes.embed.object));

    sp_da_for(target->objects, it) {
      spn_compile_unit_t* obj = target->objects[it];
      sp_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, target->nodes.embed.header));
    }
  }

  return SPN_OK;
}

// ┌──────────┐
// │ spn.toml │──┐
// └──────────┘  │  ┌─────────────┐   ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐   ┌─────────────┐   ┌──────────────────┐
//               ├─▶│ user::main  │──▶    user graph     ──▶│ user::exit  │──▶│ script::package  │
// ┌──────────┐  │  └─────────────┘   └ ─ ─ ─ ─ ─ ─ ─ ─ ┘   └─────────────┘   └──────────────────┘
// │  spn.c   │──┘         │                                       ▲
// └──────────┘            └───────────────────────────────────────┘
//
spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_t* pkg = unit->pkg;

  nodes->manifest = spn_bg_add_file(graph, unit->paths.manifest);
  nodes->script = spn_bg_add_file(graph, unit->paths.script);
  nodes->package = spn_bg_add_fn(graph, run_package_hook, unit);
  nodes->stamp.package = spn_bg_add_file(graph, unit->paths.stamp.package);
  nodes->stamp.main = spn_bg_add_file(graph, unit->paths.stamp.main);
  nodes->stamp.exit = spn_bg_add_file(graph, unit->paths.stamp.exit);
  nodes->main = spn_bg_add_fn(graph, stamp_enter, unit);
  nodes->exit = spn_bg_add_fn(graph, stamp_exit, unit);

  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest));
  sp_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->script));
  sp_try(spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main));
  sp_try(spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit));
  sp_try(spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package));


  // user nodes
  // pass 1: create all command nodes
  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    node->id = spn_bg_add_fn(graph, run_user_fn, node);
    sp_da_push(unit->nodes.build.user, node->id);
  }

  // pass 2: create all file in/outputs + create a stamp file for phonies
  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];

    // if no outputs, depend on the exit node's stamp
    if (sp_da_empty(node->outputs)) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    sp_da_for(node->outputs, o) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->outputs[o]);
      sp_try(spn_bg_cmd_add_output(graph, node->id, file));
      sp_try(spn_bg_cmd_add_input(graph, nodes->exit, file));
    }

    // if no inputs, depend on the main node's stamp
    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      sp_try(spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main));
    }

    sp_da_for(node->inputs, i) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->inputs[i]);
      sp_try(spn_bg_cmd_add_input(graph, node->id, file));
    }
  }

  // pass 3: now that all file nodes exist, set up links for command inputs
  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];

    // depend on the outputs of your command inputs
    sp_da_for(node->deps, j) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[j]);
      sp_da_for(dep->outputs, k) {
        spn_bg_id_t output = get_or_put_user_file(unit, graph, dep->outputs[k]);
        sp_try(spn_bg_cmd_add_input(graph, node->id, output));
      }
    }
  }

  // object files
  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* object = sp_om_at(unit->objects, it);
    object->nodes.source = spn_bg_add_file(graph, object->paths.file);
    object->nodes.compile = spn_bg_add_fn(graph, compile_object, object);
    object->nodes.object = spn_bg_add_file(graph, object->paths.object);
    spn_bg_cmd_add_output(graph, object->nodes.compile, object->nodes.object);
    spn_bg_cmd_add_input(graph, object->nodes.compile, object->nodes.source);
    spn_bg_cmd_add_input(graph, object->nodes.compile, unit->nodes.build.stamp.exit);
  }

  sp_str_ht_for_kv(unit->targets, it) {
    spn_target_unit_t* target = *it.val;

    switch (target->info->kind) {
      case SPN_TARGET_STATIC_LIB:
      case SPN_TARGET_SHARED_LIB: {
        if (!sp_da_empty(target->info->source)) {
          sp_try(add_target(graph, unit, target));
        }
        break;
      }
      case SPN_TARGET_OBJECT:
      case SPN_TARGET_EXE:
      case SPN_TARGET_JIT: {
        sp_try(add_target(graph, unit, target));
        break;
      }
      case SPN_TARGET_NONE: {
        break;
      }
    }
  }

  return SPN_OK;
}


static bool has_source_file(sp_da(sp_str_t) source, sp_str_t path) {
  sp_da_for(source, it) {
    if (sp_str_equal(source[it], path)) {
      return true;
    }
  }

  return false;
}

static void collect_source_glob(sp_str_t root, sp_str_t pattern, sp_da(sp_str_t)* source) {
  sp_glob_t* glob = sp_glob_new_str(pattern);
  if (!glob) {
    return;
  }

  sp_da(sp_fs_entry_t) entries = sp_fs_collect_recursive(root);
  sp_da(sp_str_t) matches = SP_NULLPTR;

  sp_da_for(entries, it) {
    sp_fs_entry_t* entry = &entries[it];
    if (!sp_fs_is_file(entry->file_path)) {
      continue;
    }

    sp_str_t relative = sp_str_strip_left(entry->file_path, root);
    relative = sp_str_strip_left(relative, sp_str_lit("/"));
    if (!sp_glob_match(glob, relative)) {
      continue;
    }
    if (has_source_file(matches, relative)) {
      continue;
    }

    sp_da_push(matches, relative);
  }

  sp_dyn_array_sort(matches, sp_str_sort_kernel_alphabetical);

  sp_da_for(matches, it) {
    if (has_source_file(*source, matches[it])) {
      continue;
    }

    sp_da_push(*source, matches[it]);
  }
}

static sp_da(sp_str_t) collect_target_source(spn_pkg_unit_t* pkg, spn_target_unit_t* target) {
  sp_da(sp_str_t) source = SP_NULLPTR;

  sp_da_for(target->info->source, it) {
    sp_str_t path = target->info->source[it];
    if (sp_fs_is_glob(path)) {
      collect_source_glob(pkg->paths.source, path, &source);
      continue;
    }
    if (has_source_file(source, path)) {
      continue;
    }

    sp_da_push(source, path);
  }

  return source;
}

spn_task_result_t spn_task_prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;

  // Now that the configure phase is done, everything about the build is static. We
  // can go through every target and resolve file globs into object files which need
  // to be compiled.
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);
    spn_pkg_unit_t* pkg = target->parent;

    sp_da(sp_str_t) source = collect_target_source(pkg, target);
    sp_da_for(source, j) {
      sp_str_t relative = source[j];
      sp_str_t file = sp_fs_join_path(pkg->paths.source, relative);
      sp_str_t name = spn_intern(sp_fs_get_stem(file));
      sp_str_t extension = sp_fs_get_ext(relative);
      sp_str_t stem = relative;
      if (!sp_str_empty(extension)) {
        stem = sp_str_prefix(relative, relative.len - extension.len - 1);
      }

      sp_str_t object_path = sp_fs_join_path(target->paths.object, sp_format("{}.o", SP_FMT_STR(stem)));

      if (!sp_om_has(pkg->objects, file)) {
        sp_om_insert(pkg->objects, file, ((spn_compile_unit_t) {
          .name = name,
          .package = pkg,
          .target = target,
          .session = target->session,
          .paths = {
            .object = object_path,
            .file = file,
          },
        }));
      }

      spn_compile_unit_t* object = sp_om_get(pkg->objects, file);
      sp_da_push(target->objects, object);
    }
  }

  spn_try_as(prepare_build_graph(app), SPN_TASK_ERROR);

  return SPN_TASK_DONE;
}


void spn_task_init_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn.tui.info.max_name = SP_MAX(spn.tui.info.max_name, unit->pkg->name.len);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .graph_init = {
      .profile = session->profile.name,
      .force = app->config.force,
    }
  });

  session->build.dirty = app->config.force ?
    spn_bg_compute_forced_dirty(&session->build.graph) :
    spn_bg_compute_dirty(&session->build.graph);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_DIRTY_SUMMARY,
    .dirty_summary = {
      .total_commands = sp_da_size(session->build.graph.commands),
      .dirty_commands = sp_ht_size(session->build.dirty->commands),
      .total_files = sp_da_size(session->build.graph.files),
      .dirty_files = sp_ht_size(session->build.dirty->files),
      .forced = app->config.force,
    }
  });

  session->build.executor = spn_bg_executor_new(
    &session->build.graph,
    session->build.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 1,
      .enable_logging = false
    }
  );

  spn_bg_executor_run(app->session.build.executor);
}

spn_task_result_t spn_task_run_build_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_bg_ctx_t* build = &b->build;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    spn_bg_executor_join(build->executor);

    // sp_tui_home();
    // sp_tui_clear_line();

    sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
    if (sp_da_size(build->executor->errors)) {
      sp_opt_set(error, build->executor->errors[0]);
    }

    spn_pkg_unit_t* root = spn_session_find_root(b);
    u32 num_errors = sp_da_size(build->executor->errors);
    u32 dirty_cmds = sp_ht_size(b->build.dirty->commands);

    switch (error.some) {
      case SP_OPT_SOME: {
        sp_str_t first_error = sp_str_lit("");
        spn_bg_cmd_t* err_cmd = spn_bg_find_command(&b->build.graph, error.value.cmd_id);
        if (err_cmd) {
          first_error = err_cmd->tag;
        }

        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_FAILED,
          .pkg = root->pkg,
          .io = &root->logs.io,
          .build_failed = {
            .profile = app->session.profile.name,
            .time = b->build.executor->elapsed,
            .num_errors = num_errors,
            .first_error = first_error,
          }
        });

        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_SUMMARY,
          .pkg = root->pkg,
          .io = &root->logs.io,
          .build_summary = {
            .success = false,
            .num_dirty = dirty_cmds,
            .total_commands = sp_da_size(b->build.graph.commands),
            .time = b->build.executor->elapsed,
            .profile = app->session.profile.name,
          }
        });

        return SPN_TASK_ERROR;
      }
      case SP_OPT_NONE: {
        if (!app->lock.some) {
          spn_app_update_lock_file(app);
        }

        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_PASSED,
          .pkg = root->pkg,
          .io = &root->logs.io,
          .build.passed = {
            .profile = &app->session.profile,
            .time = b->build.executor->elapsed
          }
        });

        spn_event_buffer_push(spn.events, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_SUMMARY,
          .pkg = root->pkg,
          .io = &root->logs.io,
          .build_summary = {
            .success = true,
            .num_dirty = dirty_cmds,
            .total_commands = sp_da_size(b->build.graph.commands),
            .time = b->build.executor->elapsed,
            .profile = app->session.profile.name,
          }
        });

        return SPN_TASK_DONE;
      }
    }

    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}
