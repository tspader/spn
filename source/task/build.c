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
#include "graph/graph.h"
#include "event/event.h"
#include "session/session.h"
#include "sp/sp_glob.h"
#include "target/closure.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"
#include "task/build/target.h"
#include "task/task.h"
#include "unit/package.h"

static spn_err_union_t spn_bg_error_to_union(spn_build_graph_t* graph);
static spn_bg_id_t get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path);
static spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit);
static spn_err_t add_stage(spn_build_graph_t* graph, spn_session_t* session, spn_build_unit_t* build, sp_da(spn_target_unit_t*) targets, sp_str_t dir);
static spn_err_t prepare_build_graph(spn_app_t* app);

static spn_bg_id_t add_build_command(spn_session_t* session, spn_build_unit_t* build, spn_bg_fn_t fn, void* user_data) {
  spn_bg_id_t id = spn_bg_add_fn(&session->build.graph, fn, user_data);
  sp_ht_insert(session->owners, id, build->id);
  return id;
}

static void own_target_commands(spn_session_t* session, spn_target_unit_t* target) {
  if (target->nodes.link.occupied) {
    sp_ht_insert(session->owners, target->nodes.link, target->build->id);
  }
  sp_da_for(target->objects, it) {
    sp_ht_insert(session->owners, target->objects[it]->nodes.compile, target->build->id);
  }
}

typedef enum {
  SPN_BUILD_REPORT_PASSED,
  SPN_BUILD_REPORT_FAILED,
  SPN_BUILD_REPORT_CANCELLED,
} spn_build_report_kind_t;

typedef struct {
  spn_build_unit_t* build;
  spn_build_report_kind_t kind;
  u32 total;
  u32 dirty;
  u32 completed;
  u32 errors;
  sp_str_t first_error;
} spn_build_report_t;

typedef sp_da(spn_build_report_t) spn_build_reports_t;

static spn_build_report_t* find_build_report(spn_build_reports_t reports, spn_build_unit_id_t id) {
  sp_da_for(reports, it) {
    if (reports[it].build->id == id) {
      return &reports[it];
    }
  }
  return SP_NULLPTR;
}

static spn_build_reports_t collect_build_reports(sp_mem_t mem, spn_session_t* session) {
  spn_build_reports_t reports = sp_da_new(mem, spn_build_report_t);
  sp_da_for(session->plan.builds, it) {
    sp_da_push(reports, ((spn_build_report_t) {
      .build = session->plan.builds[it].build,
    }));
  }
  if (session->units.metaprogram) {
    sp_da_push(reports, ((spn_build_report_t) {
      .build = session->units.metaprogram,
    }));
  }

  sp_da_for(session->build.graph.commands, it) {
    spn_bg_cmd_t* command = &session->build.graph.commands[it];
    spn_build_unit_id_t* owner = sp_ht_getp(session->owners, command->id);
    sp_assert(owner);
    spn_build_report_t* report = find_build_report(reports, *owner);
    sp_assert(report);
    report->total++;
    if (spn_bg_is_cmd_dirty(session->build.dirty, command->id)) {
      report->dirty++;
      if (sp_ht_getp(session->build.executor->completed, command->id)) {
        report->completed++;
      }
    }
  }

  sp_da_for(session->build.executor->errors, it) {
    spn_bg_exec_error_t* error = &session->build.executor->errors[it];
    spn_build_unit_id_t* owner = sp_ht_getp(session->owners, error->cmd_id);
    sp_assert(owner);
    spn_build_report_t* report = find_build_report(reports, *owner);
    sp_assert(report);
    report->errors++;
    if (sp_str_empty(report->first_error)) {
      spn_bg_cmd_t* command = spn_bg_find_command(&session->build.graph, error->cmd_id);
      if (command) {
        report->first_error = command->tag;
      }
    }
  }

  sp_da_for(reports, it) {
    spn_build_report_t* report = &reports[it];
    if (report->errors) {
      report->kind = SPN_BUILD_REPORT_FAILED;
    }
    else if (report->completed < report->dirty) {
      report->kind = SPN_BUILD_REPORT_CANCELLED;
    }
    else {
      report->kind = SPN_BUILD_REPORT_PASSED;
    }
  }

  return reports;
}

static void emit_build_report(spn_session_t* session, spn_build_report_t* report) {
  spn_pkg_unit_t* root = spn_session_find_pkg_unit(session, report->build, spn_session_root_pkg(session));
  spn_pkg_info_t* pkg = root ? root->info : session->pkg;
  spn_build_io_t* io = root ? &root->logs.io : SP_NULLPTR;
  spn_profile_info_t* profile = &report->build->profile;
  u64 elapsed = session->build.executor->elapsed;

  switch (report->kind) {
    case SPN_BUILD_REPORT_PASSED: {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_PASSED,
        .pkg = pkg,
        .io = io,
        .build.passed = {
          .profile = profile,
          .time = elapsed,
        },
      });
      break;
    }
    case SPN_BUILD_REPORT_FAILED: {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_FAILED,
        .pkg = pkg,
        .io = io,
        .build_failed = {
          .profile = profile->name,
          .time = elapsed,
          .num_errors = report->errors,
          .first_error = report->first_error,
        },
      });
      break;
    }
    case SPN_BUILD_REPORT_CANCELLED: {
      spn_event_buffer_push(session->events, (spn_build_event_t) {
        .kind = SPN_EVENT_BUILD_CANCELLED,
        .pkg = pkg,
        .io = io,
        .build_cancelled = {
          .profile = profile->name,
          .time = elapsed,
          .num_pending = report->dirty - report->completed,
        },
      });
      break;
    }
  }

  spn_event_buffer_push(session->events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SUMMARY,
    .pkg = pkg,
    .io = io,
    .build_summary = {
      .success = report->kind == SPN_BUILD_REPORT_PASSED,
      .num_dirty = report->dirty,
      .total_commands = report->total,
      .time = elapsed,
      .profile = profile->name,
    },
  });
}

static spn_err_t add_build_stages(spn_build_graph_t* graph, spn_session_t* session, spn_build_plan_t* plan) {
  sp_da(spn_target_unit_t*) targets = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da(spn_target_unit_t*) tests = sp_da_new(session->mem, spn_target_unit_t*);
  sp_da_for(plan->roots, it) {
    spn_target_unit_t* root = spn_session_get_target_unit(session, plan->roots[it]);
    if (root->kind != SPN_CC_OUTPUT_EXE) {
      continue;
    }
    if (root->info->kind == SPN_TARGET_TEST) {
      sp_da_push(tests, root);
    }
    else {
      sp_da_push(targets, root);
    }
  }
  spn_try(add_stage(graph, session, plan->build, targets, plan->build->paths.root));
  return add_stage(graph, session, plan->build, tests, sp_fs_join_path(session->mem, plan->build->paths.root, SP_LIT("test")));
}

static spn_err_t add_root_stages(spn_build_graph_t* graph, spn_session_t* session) {
  sp_da_for(session->plan.builds, it) {
    spn_try(add_build_stages(graph, session, &session->plan.builds[it]));
  }
  return SPN_OK;
}

spn_task_step_t spn_task_build_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_bg_init(&session->build.graph, spn.mem);
  sp_ht_init(session->mem, session->owners);
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

    bool failed = !sp_da_empty(build->executor->errors);
    if (!failed) {
      if (!app->lock.some) {
        spn_app_update_lock_file(app);
      }
    }

    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_build_reports_t reports = collect_build_reports(scratch.mem, session);
    sp_da_for(reports, it) {
      if (!reports[it].total) {
        continue;
      }
      emit_build_report(session, &reports[it]);
    }
    sp_mem_end_scratch(scratch);

    return failed ? spn_task_fail(SPN_ERROR) : spn_task_done();
  }

  return spn_task_continue();
}


static spn_err_t add_target_link_deps(spn_build_graph_t* graph, spn_session_t* session, spn_target_unit_t* target) {
  if (!target || !target->nodes.link.occupied) {
    return SPN_OK;
  }
  sp_da_for(target->deps.target, it) {
    spn_target_unit_t* lib = target->deps.target[it];
    if (!lib->info->no_link && lib->nodes.output.occupied) {
      spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, lib->nodes.output));
    }
  }
  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(session->mem, target);
  sp_da(spn_link_lib_t) libs = spn_closure_link_libs(session->mem, closure, target->pkg);
  sp_da_for(libs, it) {
    if (libs[it].lib->nodes.output.occupied) {
      spn_try(spn_bg_cmd_add_input(graph, target->nodes.link, libs[it].lib->nodes.output));
    }
  }
  return SPN_OK;
}

spn_err_t prepare_build_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* unit = build->packages[it];
      spn_try(add_package(graph, unit));
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* pkg = build->packages[it];
      sp_da_for(pkg->deps, it) {
        spn_pkg_unit_t* dep = pkg->deps[it].unit;
        spn_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.main, dep->nodes.build.stamp.package));
        sp_da_for(pkg->targets, it) {
          spn_target_unit_t* target = pkg->targets[it];
          if (!target->nodes.embed.run.occupied) {
            continue;
          }
          sp_da_for(target->info->embed, it) {
            spn_embed_t* embed = &target->info->embed[it];
            if (embed->kind == SPN_EMBED_DIR && sp_str_starts_with(embed->dir.path, dep->paths.store)) {
              spn_try(spn_bg_cmd_add_input(graph, target->nodes.embed.run, dep->nodes.build.stamp.package));
            }
          }
        }
      }
      sp_da_for(pkg->targets, it) {
        spn_try(add_target_link_deps(graph, session, pkg->targets[it]));
      }
    }
  }

  sp_da_for(session->units.metaprogram->packages, it) {
    spn_pkg_unit_t* pkg = session->units.metaprogram->packages[it];
    spn_try(add_target_link_deps(graph, session, pkg->meta.build.target));
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

spn_err_t add_stage(spn_build_graph_t* graph, spn_session_t* session, spn_build_unit_t* build, sp_da(spn_target_unit_t*) targets, sp_str_t dir) {
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

  spn_bg_id_t node = add_build_command(session, build, stage_targets, stage);
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

  spn_try(spn_bg_cmd_add_input(graph, pkg->nodes.build.package, target->nodes.output));

  if (!sp_da_empty(info->embed)) {
    target->nodes.embed.run = add_build_command(target->session, target->build, compile_embed, target);
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

spn_err_t add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_info_t* pkg = unit->info;

  nodes->manifest = spn_bg_add_file(graph, unit->paths.manifest);
  nodes->script = spn_bg_add_file(graph, unit->paths.script);
  nodes->package = add_build_command(unit->session, unit->build, run_package_hook, unit);
  nodes->stamp.package = spn_bg_add_file(graph, unit->paths.stamp.package);
  nodes->stamp.main = spn_bg_add_file(graph, unit->paths.stamp.main);
  nodes->stamp.exit = spn_bg_add_file(graph, unit->paths.stamp.exit);
  nodes->stamp.profile = spn_bg_add_file(graph, unit->paths.stamp.profile);
  nodes->main = add_build_command(unit->session, unit->build, stamp_enter, unit);
  nodes->exit = add_build_command(unit->session, unit->build, stamp_exit, unit);

  spn_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest));
  spn_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->script));
  spn_try(spn_bg_cmd_add_input(graph, nodes->main, nodes->stamp.profile));
  spn_try(spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main));
  spn_try(spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main));
  spn_try(spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit));
  spn_try(spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit));
  spn_try(spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package));

  sp_assert(unit->program);
  spn_target_unit_t* build = unit->program->meta.build.target;
  if (build) {
    spn_try(spn_build_add_target_nodes(graph, build));
    own_target_commands(unit->session, build);
    sp_da_for(build->objects, it) {
      spn_compile_unit_t* object = build->objects[it];
      spn_try(spn_bg_cmd_add_input(graph, object->nodes.compile, nodes->stamp.main));
      sp_da_for(build->deps.package, it) {
        spn_bg_id_t configured = spn_bg_add_file(graph, build->deps.package[it]->paths.stamp.configure);
        spn_try(spn_bg_cmd_add_input(graph, object->nodes.compile, configured));
      }
    }
    spn_try(spn_bg_cmd_add_input(graph, nodes->package, build->nodes.output));
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];
    node->id = add_build_command(unit->session, unit->build, run_user_fn, node);
    sp_da_push(unit->nodes.build.user, node->id);

    if (build) {
      spn_try(spn_bg_cmd_add_input(graph, node->id, build->nodes.output));
    }
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];

    if (sp_da_empty(node->outputs)) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    sp_da_for(node->outputs, it) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->outputs[it]);
      spn_try(spn_bg_cmd_add_output(graph, node->id, file));
      spn_try(spn_bg_cmd_add_input(graph, nodes->exit, file));
    }

    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      spn_try(spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main));
    }

    sp_da_for(node->inputs, it) {
      spn_bg_id_t file = get_or_put_user_file(unit, graph, node->inputs[it]);
      spn_try(spn_bg_cmd_add_input(graph, node->id, file));
    }
  }

  sp_da_for(unit->nodes.user, it) {
    spn_user_node_t* node = &unit->nodes.user[it];

    sp_da_for(node->deps, it) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[it]);
      sp_da_for(dep->outputs, it) {
        spn_bg_id_t output = get_or_put_user_file(unit, graph, dep->outputs[it]);
        spn_try(spn_bg_cmd_add_input(graph, node->id, output));
      }
    }
  }

  sp_da_for(unit->targets, it) {
    spn_target_unit_t* target = unit->targets[it];
    if (sp_da_empty(target->info->source)) {
      continue;
    }
    if (target->lib_kind == SPN_LIB_KIND_SOURCE) {
      continue;
    }
    if (target->lib_kind == SPN_LIB_KIND_OBJECT) {
      spn_try(spn_build_add_object_nodes(graph, target));
    }
    else {
      spn_try(spn_build_add_target_nodes(graph, target));
    }
    own_target_commands(unit->session, target);
    sp_da_for(target->objects, it) {
      spn_try(spn_bg_cmd_add_input(graph, target->objects[it]->nodes.compile, unit->nodes.build.stamp.exit));
    }
    spn_try(add_target(graph, unit, target));
  }

  return SPN_OK;
}
