#include "cc.h"
#include "ctx/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "sp/sp_om.h"
#include "spn.h"
#include "target/types.h"
#include "unit/types.h"

#include "app/app.h"
#include "error/types.h"
#include "external/wasm/wasm.h"
#include "graph/graph.h"
#include "event/event.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"
#include "task/task.h"
#include "unit/package.h"

static spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph);
static spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path);
static spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit);
static spn_err_t add_stage(spn_build_graph_t* graph, spn_session_t* session, sp_da(spn_target_unit_t*) targets, sp_str_t dir);
static spn_err_t prepare_build_graph(spn_app_t* app);

static spn_err_t add_build_stages(spn_build_graph_t* graph, spn_session_t* session, spn_build_unit_t* build) {
  sp_da(spn_target_unit_t*) targets = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da(spn_target_unit_t*) tests = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da_for(session->units.roots, it) {
    spn_target_unit_t* root = session->units.roots[it];
    if (root->pkg->build != build || root->kind != SPN_CC_OUTPUT_EXE) {
      continue;
    }
    if (root->info->kind == SPN_TARGET_TEST) {
      sp_da_push(tests, root);
    }
    else {
      sp_da_push(targets, root);
    }
  }
  spn_try(add_stage(graph, session, targets, build->paths.profile));
  return add_stage(graph, session, tests, sp_fs_join_path(session->mem, build->paths.profile, SP_LIT("test")));
}

static spn_err_t add_root_stages(spn_build_graph_t* graph, spn_session_t* session) {
  sp_da_for(session->plan.builds, it) {
    spn_try(add_build_stages(graph, session, session->plan.builds[it]));
  }
  return SPN_OK;
}

spn_task_step_t spn_task_build_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_bg_init(&session->build.graph, spn.mem);
  prepare_build_graph(app);

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
      .num_threads = 16,
      .enable_logging = false
    }
  );

  spn_bg_executor_run(app->session.build.executor);

  return spn_task_continue();
}

spn_task_step_t spn_task_build_graph_update(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_bg_ctx_t* build = &session->build;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    spn_bg_executor_join(build->executor);

    // sp_tui_home();
    // sp_tui_clear_line();

    sp_opt(spn_bg_exec_error_t) error = SP_ZERO_INITIALIZE();
    if (sp_da_size(build->executor->errors)) {
      sp_opt_set(error, build->executor->errors[0]);
    }

    spn_pkg_unit_t* root = spn_session_find_requested_pkg(session, session->plan.builds[0]);
    sp_assert(root);
    u32 num_errors = sp_da_size(build->executor->errors);
    u32 dirty_cmds = sp_ht_size(session->build.dirty->commands);

    if (error.some) {
      sp_str_t first_error = sp_str_lit("");
      spn_bg_cmd_t* err_cmd = spn_bg_find_command(&session->build.graph, error.value.cmd_id);
      if (err_cmd) {
        first_error = err_cmd->tag;
      }

      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_FAILED,
        .pkg = root->info,
        .io = &root->logs.io,
        .build_failed = {
          .profile = session->profile.name,
          .time = session->build.executor->elapsed,
          .num_errors = num_errors,
          .first_error = first_error,
        }
      });
    }
    else {
      if (!app->lock.some) {
        spn_app_update_lock_file(app);
      }

      spn_event_buffer_push(spn.events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_PASSED,
        .pkg = root->info,
        .io = &root->logs.io,
        .build.passed = {
          .profile = &session->profile,
          .time = session->build.executor->elapsed
        }
      });
    }

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SUMMARY,
      .pkg = root->info,
      .io = &root->logs.io,
      .build_summary = {
        .success = !error.some,
        .num_dirty = dirty_cmds,
        .total_commands = sp_da_size(session->build.graph.commands),
        .time = session->build.executor->elapsed,
        .profile = session->profile.name,
      }
    });

    return error.some ? spn_task_fail(SPN_ERROR) : spn_task_done();
  }

  return spn_task_continue();
}


spn_err_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // Add all nodes to the build graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_try(add_package(graph, unit));
  }

  // Order each package's build after its direct dependencies, and sequence any
  // directory embeds after the dep step that populates the store they read.
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* pkg = sp_om_at(session->units.packages, it);
    sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, pkg);

    sp_da_for(deps, d) {
      spn_pkg_unit_t* dep = deps[d].unit;
      if (!dep || dep == pkg) continue;

      spn_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.main, dep->nodes.build.stamp.package));

      // A directory embed reads a dep's store directory wholesale. Unlike a file
      // embed it declares no per-file graph input, so order its embed node after
      // the dep's package step that populates the directory.
      sp_da_for(pkg->targets, t) {
        spn_target_unit_t* target = pkg->targets[t];
        if (!target->nodes.embed.run.occupied) continue;

        sp_da_for(target->info->embed, e) {
          spn_embed_t* embed = &target->info->embed[e];
          if (embed->kind != SPN_EMBED_DIR) continue;
          if (!sp_str_starts_with(embed->dir.path, dep->paths.store)) continue;

          spn_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, dep->nodes.build.stamp.package));
        }
      }
    }
  }

  // Link each product against its transitive closure, rooted at the link unit
  // and cut at shared-library boundaries, so a static dependency two hops away
  // still lands on the link line.
  sp_om_for(session->units.targets, it) {
    spn_target_unit_t* target = sp_om_at(session->units.targets, it);
    if (!target->nodes.link.occupied) continue;

    sp_da_for(target->deps.target, l) {
      spn_target_unit_t* lib = target->deps.target[l];
      if (lib->info->no_link) continue;
      if (lib->nodes.output.occupied) {
        spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, lib->nodes.output));
      }
    }

    sp_da(spn_closure_entry_t) closure = spn_target_link_closure(session->mem, target);
    sp_da(spn_link_lib_t) libs = spn_closure_link_libs(session->mem, closure, target->pkg);
    sp_da_for(libs, l) {
      if (libs[l].lib->nodes.output.occupied) {
        spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, libs[l].lib->nodes.output));
      }
    }
  }

  spn_try(add_root_stages(graph, session));

  return SPN_OK;
}

static void stage_push_file(spn_stage_unit_t* stage, sp_str_t from, sp_str_t to, spn_bg_id_t input) {
  sp_da_for(stage->files, it) {
    if (sp_str_equal(stage->files[it].to, to)) return;
  }
  sp_da_push(stage->files, ((spn_stage_file_t) {
    .from = from,
    .to = to,
    .input = input,
  }));
}

spn_err_t add_stage(spn_build_graph_t* graph, spn_session_t* session, sp_da(spn_target_unit_t*) targets, sp_str_t dir) {
  sp_mem_t mem = session->mem;

  spn_stage_unit_t* stage = sp_alloc_type(mem, spn_stage_unit_t);
  stage->dir = dir;
  sp_da_init(mem, stage->files);

  sp_da_for(targets, it) {
    spn_target_unit_t* target = targets[it];
    if (!target->nodes.output.occupied) continue;

    stage_push_file(stage, get_target_output_path(mem, target), get_target_staged_path(mem, target), target->nodes.output);

    sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(mem, target);
    sp_da_for(libs, lt) {
      spn_target_unit_t* lib = libs[lt];
      if (!lib->nodes.output.occupied) continue;

      sp_str_t from = get_target_output_path(mem, lib);
      stage_push_file(stage, from, sp_fs_join_path(mem, dir, sp_fs_get_name(from)), lib->nodes.output);
    }
  }

  if (sp_da_empty(stage->files)) return SPN_OK;

  spn_bg_id_t node = spn_bg_add_fn(graph, stage_targets, stage);
  sp_da_for(stage->files, it) {
    spn_stage_file_t* file = &stage->files[it];
    spn_try(spn_bg_cmd_add_input(graph, node, file->input));
    spn_try(spn_bg_cmd_add_output(graph, node, spn_bg_add_file(graph, file->to)));
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
//   unit for ergonomics it should hold a pointer, or sp_str_om(unit*)
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
  spn_target_info_t* info = target->info;

  switch (target->lib_kind) {
    // Source libs build nothing; consumers compile against their sources directly
    case SPN_LIB_KIND_SOURCE: {
      return SPN_OK;
    }
    // Object libs have no link step; their objects are the artifacts, so the
    // package step waits on them directly
    case SPN_LIB_KIND_OBJECT: {
      sp_da_for(target->objects, it) {
        spn_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, target->objects[it]->nodes.object));
      }
      return SPN_OK;
    }
    case SPN_LIB_KIND_STATIC:
    case SPN_LIB_KIND_SHARED:
    case SPN_LIB_KIND_NONE: {
      break;
    }
  }

  target->nodes.link = spn_bg_add_fn(graph, link_target, target);
  target->nodes.output = spn_bg_add_file(graph, get_target_output_path(spn.mem, target));

  spn_bg_cmd_add_output(graph, target->nodes.link,  target->nodes.output);
  spn_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, target->nodes.output));

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* object = target->objects[it];
    spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, object->nodes.object));
  }

  if (!sp_da_empty(info->embed)) {
    target->nodes.embed.run = spn_bg_add_fn(graph, compile_embed, target);
    target->nodes.embed.object = spn_bg_add_file(graph, get_embed_object_path(spn.mem, target));
    target->nodes.embed.header = spn_bg_add_file(graph, get_embed_header_path(spn.mem, target));

    spn_try(spn_bg_cmd_add_output(graph, target->nodes.embed.run, target->nodes.embed.object));
    spn_try(spn_bg_cmd_add_output(graph, target->nodes.embed.run, target->nodes.embed.header));
    spn_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, pkg->nodes.build.stamp.exit));

    sp_da_for(info->embed, it) {
      spn_embed_t* embed = &info->embed[it];

      switch (embed->kind) {
        case SPN_EMBED_FILE: {
          spn_bg_id_t input = get_or_put_user_file(pkg, graph, embed->file.path);
          spn_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, input));
          break;
        }
        case SPN_EMBED_MEM:
        case SPN_EMBED_DIR: {
          break;
        }
      }
    }

    spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, target->nodes.embed.object));

    sp_da_for(target->objects, it) {
      spn_compile_unit_t* obj = target->objects[it];
      spn_try(spn_bg_cmd_add_input(graph, obj->nodes.compile, target->nodes.embed.header));
    }
  }

  return SPN_OK;
}

// ┌──────────┐
// │ spn.toml │──┐
// └──────────┘  │  ┌─────────────┐    ┌─────────┐  ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐   ┌─────────────┐   ┌──────────────────┐
//               ├─▶│ user::main  │───▶│ build.c │─▶    user graph     ──▶│ user::exit  │──▶│ script::package  │
// ┌──────────┐  │  └─────────────┘    └─────────┘  └ ─ ─ ─ ─ ─ ─ ─ ─ ┘   └─────────────┘   └──────────────────┘
// │  spn.c   │──┘         │                                                    ▲
// └──────────┘            └────────────────────────────────────────────────────┘
//
spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_info_t* pkg = unit->info;

  nodes->manifest = spn_bg_add_file(graph, unit->paths.manifest);
  nodes->script = spn_bg_add_file(graph, unit->paths.script);
  nodes->package = spn_bg_add_fn(graph, run_package_hook, unit);
  nodes->stamp.package = spn_bg_add_file(graph, unit->paths.stamp.package);
  nodes->stamp.main = spn_bg_add_file(graph, unit->paths.stamp.main);
  nodes->stamp.exit = spn_bg_add_file(graph, unit->paths.stamp.exit);
  nodes->main = spn_bg_add_fn(graph, stamp_enter, unit);
  nodes->exit = spn_bg_add_fn(graph, stamp_exit, unit);

  spn_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest));
  spn_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->script));
  spn_try(spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main));
  spn_try(spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main));
  spn_try(spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit));
  spn_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit));
  spn_try(spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package));

  bool has_build_script = unit->wasm.build.state != SPN_WASM_SCRIPT_NONE;
  if (has_build_script) {
    nodes->build_script.run = spn_bg_add_fn(graph, compile_build_script, unit);
    nodes->build_script.module = spn_bg_add_file(graph, unit->wasm.build.path);

    spn_try(spn_bg_cmd_add_output(graph, nodes->build_script.run, nodes->build_script.module));
    spn_try(spn_bg_cmd_add_input(graph, nodes->build_script.run, nodes->stamp.main));
    sp_da_for(unit->script.build.source, it) {
      spn_bg_id_t source = get_or_put_user_file(unit, graph, unit->script.build.source[it]);
      spn_try(spn_bg_cmd_add_input(graph, nodes->build_script.run, source));
    }

    spn_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->build_script.module));
  }


  // user nodes
  // pass 1: create all command nodes
  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    node->id = spn_bg_add_fn(graph, run_user_fn, node);
    sp_da_push(unit->nodes.build.user, node->id);

    if (has_build_script) {
      spn_try(spn_bg_cmd_add_input(graph, node->id, nodes->build_script.module));
    }
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
      spn_try(spn_bg_cmd_add_output(graph, node->id, file));
      spn_try(spn_bg_cmd_add_input(graph, nodes->exit, file));
    }

    // if no inputs, depend on the main node's stamp
    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      spn_try(spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main));
    }

    sp_da_for(node->inputs, i) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->inputs[i]);
      spn_try(spn_bg_cmd_add_input(graph, node->id, file));
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
        spn_try(spn_bg_cmd_add_input(graph, node->id, output));
      }
    }
  }

  // object files
  sp_da_for(unit->objects, it) {
    spn_compile_unit_t* object = unit->objects[it];
    object->nodes.source = spn_bg_add_file(graph, object->paths.file);
    object->nodes.compile = spn_bg_add_fn(graph, compile_object, object);
    object->nodes.object = spn_bg_add_file(graph, object->paths.object);
    spn_bg_cmd_add_output(graph, object->nodes.compile, object->nodes.object);
    spn_bg_cmd_add_input(graph, object->nodes.compile, object->nodes.source);
    spn_bg_cmd_add_input(graph, object->nodes.compile, unit->nodes.build.stamp.exit);
  }

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (!sp_da_empty(target->info->source)) {
      spn_try(add_target(graph, unit, target));
    }
  }

  return SPN_OK;
}
