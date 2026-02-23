#include "app.h"

#include "cc.h"
#include "gen.h"
#include "graph.h"

#include "sp/io.h"
#include "sp/macro.h"
#include "sp/str.h"

///////////
// TASKS //
///////////
void spn_task_enqueue(spn_task_executor_t* ex, s32 kind) {
  sp_assert(ex->len < SPN_TASK_MAX_QUEUE);
  ex->data[ex->len++] = kind;
}

// TASK: RESOLVE
spn_task_result_t spn_task_resolve(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_session_init(session, &app->package, app->config.profile, sp_str_lit("build"));
  spn_session_set_filter(session, app->config.filter);

  spn_init_pkg_unit_for_session(session, &session->units.root, &app->package, SPN_PACKAGE_KIND_ROOT, app->package.version);

  spn_app_resolve(app);

  sp_ht_for_kv(app->resolver.resolved, it) {
    spn_session_add_pkg_unit(session, *it.val);
  }

  sp_om_for(session->units.packages, it) {
    spn_pkg_t* pkg = sp_om_at(session->units.packages, it)->ctx.pkg;
    spn.tui.info.max_name = SP_MAX(spn.tui.info.max_name, pkg->name.len);
  }

  return SPN_TASK_DONE;
}

// TASK: SYNC
void spn_task_sync_init(spn_app_t* app) {
  spn_session_t* b = &app->session;

  spn_build_graph_t* graph = &b->sync.graph;

  spn_event_buffer_push(spn.events, &spn_session_find_root(b)->ctx, SPN_EVENT_FETCH);

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(b->units.packages, it);

    if (dep->ctx.pkg->kind != SPN_PACKAGE_KIND_INDEX) {
      continue;
    }

    spn_bg_id_t sync = spn_bg_add_fn(graph, spn_executor_sync_repo, dep);
    spn_bg_cmd_set_metadata(graph, sync, sp_format("sync ({})", SP_FMT_STR(dep->ctx.name)), sp_str_lit(""), SPN_BG_VIZ_DEFAULT);
  }

  if (!sp_da_empty(graph->commands)) {
    b->sync.dirty = spn_bg_compute_forced_dirty(graph);
    b->sync.executor = spn_bg_executor_new(graph, b->sync.dirty, (spn_bg_executor_config_t) {
      .num_threads = 3,
      .enable_logging = false
    });

    spn_bg_executor_run(b->sync.executor);
  }
}

spn_task_result_t spn_task_sync_update(spn_app_t* app) {
  spn_bg_ctx_t* sync = &app->session.sync;

  if (!sync->executor) {
    return SPN_TASK_DONE;
  }

  if (sp_atomic_s32_get(&sync->executor->shutdown)) {
    spn_bg_executor_join(sync->executor);
    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}

s32 spn_executor_configure_pkg(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* pkg = (spn_pkg_unit_t*)user_data;

  sp_try(spn_session_compile_pkg(pkg->ctx.session, pkg));

  if (spn_pkg_unit_run_configure_hook(pkg)) {
    spn_push_event_ex((spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_FAILED
    });
    return SPN_ERROR;
  }

  sp_om_for(pkg->targets, it) {
    spn_target_unit_t* target = sp_om_at(pkg->targets, it);
    sp_da_for(target->info->source, j) {
      sp_str_t file = sp_fs_join_path(pkg->ctx.paths.source, target->info->source[j]);
      sp_str_t name = spn_intern(sp_fs_get_stem(file));

      if (!sp_om_has(pkg->objects, file)) {
        sp_om_insert(pkg->objects, file, ((spn_compile_unit_t) {
          .name = name,
          .target = target,
          .pkg = target->pkg,
          .profile = pkg->ctx.profile,
          .session = target->session,
          .paths = {
            .object = sp_fs_join_path(
              target->paths.object,
              sp_format("{}.o", SP_FMT_STR(name))
            ),
            .source = file,
          },
        }));
      }

      spn_compile_unit_t* object = sp_om_get(pkg->objects, file);
      sp_da_push(target->objects, object);
    }
  }

  return SPN_OK;
}

s32 spn_executor_build_pkg(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_build_graph_t* graph = spn_bg_new();
  spn_bg_add_package(graph, unit);

  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);
  spn_bg_executor_t* executor = spn_bg_executor_new(graph, dirty, (spn_bg_executor_config_t) {
    .num_threads = 1
  });
  spn_bg_executor_run(executor);
  spn_bg_executor_join(executor);

  s32 result = SPN_OK;
  if (sp_da_size(executor->errors)) {
    result = SPN_ERROR;
  }

  return result;
}

s32 spn_executor_run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* ctx = (spn_pkg_unit_t*)user_data;

  spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE);

  if (spn_pkg_unit_run_package_hook(ctx)) {
    spn_event_buffer_push(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE_FAILED);
    return SPN_ERROR;
  }

  spn_pkg_unit_write_stamp(ctx, ctx->paths.stamp.package);

  return SPN_OK;
}

s32 spn_executor_write_enter_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.main);
  return SPN_OK;
}

s32 spn_executor_write_exit_stamp(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.exit);
  return SPN_OK;
}

spn_task_result_t spn_task_update_configure_graph(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    return sp_da_empty(build->executor->errors) ?
      SPN_TASK_DONE :
      SPN_TASK_ERROR;
  }

  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_prepare_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  root->nodes.configure.run = spn_bg_add_fn_ex(graph, spn_executor_configure_pkg, root, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
  root->nodes.configure.stamp = spn_bg_add_file(graph, root->paths.stamp.package);
  spn_bg_cmd_add_output(graph, root->nodes.configure.run, root->nodes.configure.stamp);

  sp_ht_for(app->resolver.resolved, it) {
    sp_str_t name = *sp_ht_it_getkp(app->resolver.resolved, it);
    spn_resolved_pkg_t* resolved = sp_ht_it_getp(app->resolver.resolved, it);
    spn_pkg_unit_t* unit = sp_om_get(b->units.packages, name);
    sp_assert(unit);
    unit->nodes.configure.run = spn_bg_add_fn_ex(graph, spn_executor_configure_pkg, unit, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp);
    spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp);
  }

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(b->units.packages, it);
    spn_pkg_t* pkg = dep->ctx.pkg;

    sp_ht_for(pkg->deps, dit) {
      sp_str_t parent_name = *sp_ht_it_getkp(pkg->deps, dit);
      spn_pkg_unit_t* parent = sp_om_get(b->units.packages, parent_name);

      spn_bg_cmd_add_input(graph, dep->nodes.configure.run, parent->nodes.configure.stamp);
    }
  }

  b->configure.dirty = spn_bg_compute_forced_dirty(graph);
  b->configure.executor = spn_bg_executor_new(
    graph,
    b->configure.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 1
    }
  );
  spn_bg_executor_run(b->configure.executor);

  return SPN_TASK_DONE;
}

spn_bg_id_t spn_bg_get_or_put_user_file(spn_pkg_unit_t* ctx, spn_build_graph_t* graph, sp_str_t path) {
  spn_bg_id_t* existing = sp_str_ht_get(ctx->nodes.files, path);
  if (existing) return *existing;

  spn_bg_id_t id = spn_bg_add_file_ex(graph, path, SPN_BG_VIZ_CMD, ctx->ctx.name);
  sp_str_ht_insert(ctx->nodes.files, path, id);
  sp_da_push(ctx->nodes.build.user, id);
  return id;
}

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(ctx->paths.stamp.dir, node->tag);
}

void spn_bg_add_target(spn_build_graph_t* graph, spn_pkg_unit_t* pkg, spn_target_unit_t* unit) {
//                         ┌──────────┐     ┌─────────────────┐     ┌──────────┐
//                         │  foo.c   │────▶│ compile::foo.c  │────▶│  foo.o   │──┐
//                         └──────────┘     └─────────────────┘     └──────────┘  │
//                                                                                │  ┌────────┐     ┌───────────────────────┐     ┌──────────────────┐
//                                                                                ├─▶│  link  │────▶│   $STORE/bin/foobar   │────▶│ script::package  │
//                         ┌──────────┐     ┌─────────────────┐     ┌──────────┐  │  └────────┘     └───────────────────────┘     └──────────────────┘
//                         │  bar.c   │────▶│ compile::bar.c  │────▶│  bar.o   │──┘       ▲
//                         └──────────┘     └─────────────────┘     └──────────┘          │
//                                                                                        │
// ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐     ┌─────────────┐                                                │
//     user graph     ────▶│ user::exit  │────────────────────────────────────────────────┘
// └ ─ ─ ─ ─ ─ ─ ─ ─ ┘     └─────────────┘
  spn_target_t* target = unit->info;

  unit->nodes.link = spn_bg_add_fn_ex(graph, spn_executor_link_target, unit, SPN_BG_VIZ_CMD, pkg->ctx.name, sp_format("link {}", SP_FMT_STR(target->name)));
  unit->nodes.output = spn_bg_add_file_ex(graph, sp_fs_join_path(pkg->ctx.paths.bin, target->name), SPN_BG_VIZ_BINARY, pkg->ctx.name);

  spn_bg_cmd_add_output(graph, unit->nodes.link,  unit->nodes.output);
  //spn_bg_cmd_add_input(graph, unit->nodes.link, pkg->nodes.build.stamp.exit);
  //spn_bg_cmd_add_input(graph, unit->nodes.compile, pkg->nodes.build.stamp.main);
  spn_bg_cmd_add_input(graph, pkg->nodes.build.package, unit->nodes.output);

  sp_da_for(unit->objects, it) {
    spn_compile_unit_t* obj = unit->objects[it];
    spn_bg_cmd_add_input(graph, unit->nodes.link, obj->nodes.object);
  }
}

void spn_bg_add_internal_pkg_nodes(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
}

// ┌──────────┐
// │ spn.toml │──┐
// └──────────┘  │  ┌─────────────┐     ┌ ─ ─ ─ ─ ─ ─ ─ ─ ┐     ┌─────────────┐     ┌──────────────────┐
//               ├─▶│ user::main  │────▶    user graph     ────▶│ user::exit  │────▶│ script::package  │
// ┌──────────┐  │  └─────────────┘     └ ─ ─ ─ ─ ─ ─ ─ ─ ┘     └─────────────┘     └──────────────────┘
// │  spn.c   │──┘         │                                           ▲
// └──────────┘            └───────────────────────────────────────────┘
//
void spn_bg_add_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_nodes_t* nodes = &unit->nodes.build;
  spn_pkg_t* pkg = unit->ctx.pkg;

  nodes->manifest = spn_bg_add_file_ex(graph, pkg->paths.manifest, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->script = spn_bg_add_file_ex(graph, pkg->paths.script, SPN_BG_VIZ_MANIFEST, unit->ctx.name);
  nodes->package = spn_bg_add_fn_ex(graph, spn_executor_run_package_hook, unit, SPN_BG_VIZ_CMD, unit->ctx.name, sp_str_lit("script::package"));
  nodes->stamp.package = spn_bg_add_file_ex(graph, unit->paths.stamp.package, SPN_BG_VIZ_STAMP, unit->ctx.name);
  nodes->stamp.main = spn_bg_add_file_ex(graph, unit->paths.stamp.main, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->stamp.exit = spn_bg_add_file_ex(graph, unit->paths.stamp.exit, SPN_BG_VIZ_CMD, unit->ctx.name);
  nodes->main = spn_bg_add_fn_ex(graph, spn_executor_write_enter_stamp, unit, SPN_BG_VIZ_CMD, pkg->name, sp_str_lit("user::enter"));
  nodes->exit = spn_bg_add_fn_ex(graph, spn_executor_write_exit_stamp, unit, SPN_BG_VIZ_CMD, pkg->name, sp_str_lit("user::exit"));

  spn_bg_cmd_add_input(graph, nodes->main, nodes->manifest);
  spn_bg_cmd_add_input(graph, nodes->main, nodes->script);
  spn_bg_cmd_add_output(graph, nodes->main, nodes->stamp.main);
  spn_bg_cmd_add_input(graph, nodes->exit, nodes->stamp.main);
  spn_bg_cmd_add_output(graph, nodes->exit, nodes->stamp.exit);
  spn_bg_cmd_add_input(graph, nodes->package, nodes->stamp.exit);
  spn_bg_cmd_add_output(graph, nodes->package, nodes->stamp.package);

  // user nodes
  // pass 1: create all command nodes
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];
    node->id = spn_bg_add_fn_ex(graph, spn_executor_user_fn, node, SPN_BG_VIZ_CMD, unit->ctx.name, node->tag);
    sp_da_push(unit->nodes.build.user, node->id);
  }

  // pass 2: create all file in/outputs + create a stamp file for phonies
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // if no outputs, depend on the exit node's stamp
    if (sp_da_empty(node->outputs)) {
      sp_da_push(node->outputs, spn_pkg_unit_get_node_stamp_file(unit, node));
    }

    sp_da_for(node->outputs, o) {
      spn_bg_id_t file = spn_bg_get_or_put_user_file(unit, graph, node->outputs[o]);
      spn_bg_cmd_add_output(graph, node->id, file);
      spn_bg_cmd_add_input(graph, nodes->exit, file);
    }

    // if no inputs, depend on the main node's stamp
    if (sp_da_empty(node->inputs) && sp_da_empty(node->deps)) {
      spn_bg_cmd_add_input(graph, node->id, nodes->stamp.main);
    }

    sp_da_for(node->inputs, i) {
      spn_bg_id_t file = spn_bg_get_or_put_user_file(unit, graph, node->inputs[i]);
      spn_bg_cmd_add_input(graph, node->id, file);
    }
  }

  // pass 3: now that all file nodes exist, set up links for command inputs
  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];

    // depend on the outputs of your command inputs
    sp_da_for(node->deps, dit) {
      spn_user_node_t* dep = spn_find_user_node(node->deps[dit]);
      sp_da_for(dep->outputs, oit) {
        spn_bg_id_t output = spn_bg_get_or_put_user_file(unit, graph, dep->outputs[oit]);
        spn_bg_cmd_add_input(graph, node->id, output);
      }
    }
  }

  // object files
  // @spader gate this on user node exit?
  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* obj = sp_om_at(unit->objects, it);
    obj->nodes.source = spn_bg_add_file_ex(graph, obj->paths.source, SPN_BG_VIZ_DEFAULT, pkg->name);
    obj->nodes.compile = spn_bg_add_fn_ex(graph, spn_executor_compile_object, obj, SPN_BG_VIZ_CMD, pkg->name, sp_format("compile {}", SP_FMT_STR(sp_fs_get_name(obj->paths.object))));
    obj->nodes.object = spn_bg_add_file_ex(graph, obj->paths.object, SPN_BG_VIZ_DEFAULT, pkg->name);
    spn_bg_cmd_add_output(graph, obj->nodes.compile, obj->nodes.object);
    spn_bg_cmd_add_input(graph, obj->nodes.compile, unit->nodes.build.stamp.exit);
  }

  sp_om_for(unit->targets, it) {
    spn_bg_add_target(graph, unit, sp_om_at(unit->targets, it));
  }
}

void spn_session_link_graph(spn_session_t* session, spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->ctx.pkg;
  sp_ht_for(pkg->deps, it) {
    spn_pkg_unit_t* parent = spn_session_find_pkg(session, *sp_ht_it_getkp(pkg->deps, it));
    spn_bg_cmd_add_input(graph, unit->nodes.build.main, parent->nodes.build.stamp.package);
  }
}

spn_task_result_t spn_task_prepare_build_graph_v2(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;

  // phase 1: add each package to the graph
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_bg_add_package(graph, unit);
  }
  spn_bg_add_package(graph, spn_session_find_root(session));

  // phase 2: link dependent packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* parent = sp_om_at(session->units.packages, it);
    spn_session_link_graph(session, graph, sp_om_at(session->units.packages, it));
  }
  spn_session_link_graph(session, graph, spn_session_find_root(session));

  return SPN_TASK_DONE;
}

// TASK: BUILD
void spn_task_run_build_graph_init(spn_app_t* app) {
  spn_session_t* b = &app->session;
  u32 num_packages = 0;
  sp_om_for(b->units.packages, it) {
    num_packages++;
  }

  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_INIT_BUILD_GRAPH,
    .graph_init = {
      .profile = b->profile->name,
      .force = app->config.force,
      .packages = num_packages,
    }
  });

  b->build.dirty = app->config.force ?
    spn_bg_compute_forced_dirty(&b->build.graph) :
    spn_bg_compute_dirty(&b->build.graph);

  b->build.executor = spn_bg_executor_new(&b->build.graph, b->build.dirty, (spn_bg_executor_config_t) {
    .num_threads = 1,
    .enable_logging = false
  });

  spn_bg_executor_run(app->session.build.executor);
}

spn_task_result_t spn_task_run_build_graph_update(spn_app_t* app) {
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

    switch (error.some) {
      case SP_OPT_SOME: {
        return SPN_TASK_ERROR;
      }
      case SP_OPT_NONE: {
        if (!app->lock.some) {
          spn_app_update_lock_file(app);
        }

        spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
          .kind = SPN_EVENT_BUILD_PASSED,
          .build.passed = {
            .profile = app->session.profile,
            .time = b->build.executor->elapsed
          }
        });

        return SPN_TASK_DONE;
      }
    }

    return SPN_TASK_DONE;
  }

  return SPN_TASK_CONTINUE;
}

spn_task_result_t spn_task_run_tests(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_pkg_unit_t* root = &b->units.root;

  sp_ht(sp_str_t, s32) tests = SP_NULLPTR;

  sp_tm_timer_t timer = sp_tm_start_timer();

  sp_om_for(root->targets, it) {
    spn_target_unit_t* unit = sp_om_at(root->targets, it);
    spn_target_t* target = unit->info;

    if (!spn_target_filter_pass(&b->filter, target)) {
      continue;
    }

    sp_fs_create_file(unit->paths.logs.test);
    unit->logs.test = sp_io_writer_from_file(unit->paths.logs.test, SP_IO_WRITE_MODE_OVERWRITE);

    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TEST_RUN
    });
    spn_poll(spn.sp);

    sp_ps_t ps = sp_ps_create((sp_ps_config_t) {
      .command = sp_fs_join_path(unit->paths.bin, target->name),
      .io = {
        .in =  { .mode = SP_PS_IO_MODE_NULL },
        .out = { .mode = SP_PS_IO_MODE_EXISTING, .fd = unit->logs.test.file.fd },
        .err = { .mode = SP_PS_IO_MODE_REDIRECT }
      },
      .cwd = unit->paths.work,
    });
    sp_ps_output_t result = sp_ps_output(&ps);
    sp_ht_insert(tests, target->name, result.status.exit_code);

    // sp_tui_up(1);
    // sp_tui_home();
    // sp_tui_clear_line();
    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, result.status.exit_code ?
      (spn_build_event_t) { .kind = SPN_EVENT_TEST_FAILED } :
      (spn_build_event_t) { .kind = SPN_EVENT_TEST_PASSED }
    );
    spn_poll(spn.sp);
  }
  u64 elapsed = sp_tm_read_timer(&timer);

  bool ok = true;
  sp_ht_for_kv(tests, it) {
    spn_target_unit_t* unit = sp_om_get(root->targets, *it.key);

    if (*it.val) {
      ok = false;
      sp_io_writer_close(&unit->logs.test);
      sp_io_write_str(&spn.logger.err, sp_io_read_file(unit->paths.logs.test));
    }
  }

  spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(b)->ctx, (spn_build_event_t) {
    .kind =  ok ?
      SPN_EVENT_TESTS_PASSED :
      SPN_EVENT_TEST_FAILED,
    .test.passed = {
      .time = elapsed,
      .n = sp_ht_size(tests),
      .profile = app->session.profile
    }
  });

  return SPN_TASK_DONE;
}

void spn_bg_render_file_to_builder(sp_str_builder_t* builder, u32 id, sp_str_t file) {
  sp_str_t prefix = SP_ZERO_INITIALIZE();
  if (sp_str_starts_with(file, spn.paths.cache)) {
    prefix = sp_str_lit("$CACHE");
    file = sp_str_strip_left(file, spn.paths.cache);
  }
  else if (sp_str_starts_with(file, spn.paths.project)) {
    prefix = sp_str_lit("$ROOT");
    file = sp_str_strip_left(file, spn.paths.project);
  }

  sp_mem_scratch_t scratch = sp_mem_begin_scratch();
  if (!sp_str_empty(prefix)) {
    file = sp_str_concat(prefix, file);
  }

  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "F{}({})",
    SP_FMT_U32(id),
    SP_FMT_QSTR(file)
  );

  sp_mem_end_scratch(scratch);
}

void spn_bg_render_file_id_to_builder(spn_build_graph_t* graph, sp_str_builder_t* builder, spn_bg_id_t id) {
  spn_bg_file_t* file = spn_bg_find_file(graph, id);
  spn_bg_render_file_to_builder(builder, id.index, file->path);
}

void spn_bg_render_cmd_id_to_builder(spn_build_graph_t* graph, sp_str_builder_t* builder, spn_bg_id_t id) {
  spn_bg_cmd_t* cmd = spn_bg_find_command(graph, id);

  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "C{}({})",
    SP_FMT_U32(cmd->id.index),
    SP_FMT_QSTR(cmd->tag)
  );
}

void spn_bg_begin_subgraph(sp_str_builder_t* builder, sp_str_t id, sp_str_t name) {
  sp_str_builder_new_line(builder);
  sp_str_builder_append_fmt(builder, "subgraph {}[{}]", SP_FMT_STR(id), SP_FMT_QSTR(name));
  sp_str_builder_indent(builder);
}

void spn_bg_end_subgraph(sp_str_builder_t* builder) {
  sp_str_builder_dedent(builder);
  sp_str_builder_new_line(builder);
  sp_str_builder_append_cstr(builder, "end");
}

void spn_bg_render_pkg_to_mermaid(spn_session_t* b, spn_bg_dirty_t* dirty, spn_pkg_unit_t* unit, sp_io_writer_t* io) {
  spn_build_ctx_t* ctx = &unit->ctx;
  spn_build_graph_t* graph = &b->build.graph;
  spn_pkg_nodes_t* nodes = &unit->nodes.build;

  sp_str_builder_t builder = {
    .writer = io,
    .indent = {
      .level = 1,
      .word = sp_str_lit("  ")
    }
  };

  spn_bg_begin_subgraph(&builder, ctx->name, sp_str_lit(" "));
  // sp_str_builder_new_line(&builder);
  // sp_str_builder_append_fmt(&builder, "T_{}({}):::title", SP_FMT_STR(ctx->name), SP_FMT_QSTR(ctx->name));
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->manifest);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->script);
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->package);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.package);

  // spn_bg_begin_subgraph(&builder, sp_format("{}::user", SP_FMT_STR(ctx->name)), sp_str_lit(" "));
  // sp_str_builder_new_line(&builder);
  // sp_str_builder_append_fmt(&builder, "T_{}_user(\"user\"):::title", SP_FMT_STR(ctx->name));
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->main);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.main);
  spn_bg_render_cmd_id_to_builder(graph, &builder, nodes->exit);
  spn_bg_render_file_id_to_builder(graph, &builder, nodes->stamp.exit);

  sp_da_for(unit->nodes.all, it) {
    spn_user_node_t* node = &unit->nodes.all[it];
    spn_bg_render_cmd_id_to_builder(graph, &builder, node->id);
  }

  sp_str_ht_for(unit->nodes.files, it) {
    spn_bg_id_t file_id = *sp_str_ht_it_getp(unit->nodes.files, it);
    spn_bg_render_file_id_to_builder(graph, &builder, file_id);
  }

  // spn_bg_begin_subgraph(&builder, sp_format("{}::{}", SP_FMT_STR(ctx->name), SP_FMT_CSTR("source")), sp_str_lit("source"));
  // sp_om_for(unit->targets, it) {
  //   spn_target_unit_t* target = sp_om_at(unit->targets, it);
  //   sp_da_for(target->nodes.source, s) {
  //     spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.source[s]);
  //   }
  // }
  //
  // spn_bg_end_subgraph(&builder);

  sp_om_for(unit->objects, it) {
    spn_compile_unit_t* object = sp_om_at(unit->objects, it);
    spn_bg_render_file_id_to_builder(graph, &builder, object->nodes.source);
    spn_bg_render_cmd_id_to_builder(graph, &builder, object->nodes.compile);
    spn_bg_render_file_id_to_builder(graph, &builder, object->nodes.object);
  }

  sp_om_for(unit->targets, it) {
    spn_target_unit_t* target = sp_om_at(unit->targets, it);

    // spn_bg_begin_subgraph(&builder, sp_format("{}::{}", SP_FMT_STR(ctx->name), SP_FMT_STR(target->target->name)), sp_str_lit(" "));
      // sp_str_builder_new_line(&builder);
      // sp_str_builder_append_fmt(&builder, "T_{}_{}({}):::title", SP_FMT_STR(ctx->name), SP_FMT_STR(target->target->name), SP_FMT_QSTR(target->target->name));
      spn_bg_render_cmd_id_to_builder(graph, &builder, target->nodes.link);
      spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.output);
      sp_da_for(target->nodes.source, s) {
        spn_bg_render_file_id_to_builder(graph, &builder, target->nodes.source[s]);
      }
    // spn_bg_end_subgraph(&builder);
  }
  // spn_bg_end_subgraph(&builder);
  spn_bg_end_subgraph(&builder);

  sp_str_builder_new_line(&builder);
}

void spn_render_to_mermaid(spn_app_t* app, sp_io_writer_t* io) {
  struct {
    sp_str_t text;
    sp_str_t stroke;
    sp_str_t link;
    sp_str_t clean;
    sp_str_t dirty;
    sp_str_t outer_bg;
    sp_str_t inner_bg;
  } color = {
    .text     = sp_str_lit("#ffffff"),
    .stroke   = sp_str_lit("#101010"),
    .link     = sp_str_lit("#505050"),
    .clean    = sp_str_lit("#3d633d"),
    .dirty    = sp_str_lit("#694141"),
    .outer_bg = sp_str_lit("#191924"),
    .inner_bg = sp_str_lit("#28283b"),
  };

  sp_io_write_str(io, sp_str_lit("%%{init: {'theme': 'base', 'themeVariables': { 'fontSize': '96px', 'fontFamily': 'monospace'}}}%%\n"));
  sp_io_write_str(io, sp_str_lit("graph TD\n"));

  sp_io_write_str(io, spn_bg_mermaid_class_ex(sp_str_lit("dirty"), color.dirty, color.stroke, color.text, sp_str_lit("32px")));
  sp_io_write_str(io, spn_bg_mermaid_class_ex(sp_str_lit("clean"), color.clean, color.stroke, color.text, sp_str_lit("32px")));
  sp_io_write_str(io, sp_format("  classDef title fill:{},stroke-width:0,color:{},font-size:96px\n",
    SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.text)));
  sp_io_write_str(io, sp_format("  linkStyle default stroke:{},stroke-width:2px\n", SP_FMT_STR(color.link)));

  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->build.graph;
  spn_bg_dirty_t* dirty = spn_bg_compute_dirty(graph);

  // packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_bg_render_pkg_to_mermaid(session, dirty, unit, io);
  }

  spn_bg_render_pkg_to_mermaid(session, dirty, spn_session_find_root(session), io);

  // edges
  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];

    sp_da_for(cmd->consumes, i) {
      u32 id = cmd->consumes[i].index;
      sp_io_write_str(io, sp_format("  F{} --> C{}\n",
        SP_FMT_U32(id), SP_FMT_U32(cmd->id.index)));
    }

    sp_da_for(cmd->produces, i) {
      u32 id = cmd->produces[i].index;
      sp_io_write_str(io, sp_format("  C{} --> F{}\n",
        SP_FMT_U32(cmd->id.index), SP_FMT_U32(id)));
    }
  }

  // subgraph styles
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    sp_str_t name = unit->ctx.name;
    sp_io_write_str(io, sp_format("  style {} fill:{},stroke:{},color:{}\n",
      SP_FMT_STR(name), SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.stroke), SP_FMT_STR(color.text)));
    sp_io_write_str(io, sp_format("  style {}::user fill:{},stroke:{}\n",
      SP_FMT_STR(name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
    sp_om_for(unit->targets, t) {
      spn_target_unit_t* target = sp_om_at(unit->targets, t);
      sp_io_write_str(io, sp_format("  style {}::{} fill:{},stroke:{}\n",
        SP_FMT_STR(name), SP_FMT_STR(target->info->name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
    }
  }

  spn_pkg_unit_t* root = spn_session_find_root(session);
  sp_str_t root_name = root->ctx.name;
  sp_io_write_str(io, sp_format("  style {} fill:{},stroke:{},color:{}\n",
    SP_FMT_STR(root_name), SP_FMT_STR(color.outer_bg), SP_FMT_STR(color.stroke), SP_FMT_STR(color.text)));
  sp_io_write_str(io, sp_format("  style {}::user fill:{},stroke:{}\n",
    SP_FMT_STR(root_name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
  sp_om_for(root->targets, t) {
    spn_target_unit_t* target = sp_om_at(root->targets, t);
    sp_io_write_str(io, sp_format("  style {}::{} fill:{},stroke:{}\n",
      SP_FMT_STR(root_name), SP_FMT_STR(target->info->name), SP_FMT_STR(color.inner_bg), SP_FMT_STR(color.outer_bg)));
  }

  sp_da_for(graph->commands, it) {
    spn_bg_cmd_t* cmd = &graph->commands[it];
    sp_io_write_str(io, sp_format("class C{} {}",
      SP_FMT_U32(cmd->id.index),
      SP_FMT_CSTR(spn_bg_is_cmd_dirty(dirty, cmd->id) ? "dirty" : "clean")
    ));
    sp_io_write_new_line(io);

  }

  sp_da_for(graph->files, it) {
    spn_bg_file_t* file = &graph->files[it];
    sp_io_write_str(io, sp_format("class F{} {}",
      SP_FMT_U32(file->id.index),
      SP_FMT_CSTR(spn_bg_is_file_dirty(dirty, file->id) ? "dirty" : "clean")
    ));
    sp_io_write_new_line(io);
  }
}

spn_task_result_t spn_task_graph(spn_app_t* app) {
  spn_bg_dirty_t* dirty = spn.cli.graph.dirty ? spn_bg_compute_dirty(&app->session.build.graph) : NULL;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);
  sp_str_t work_dir = root->ctx.paths.work;
  sp_str_t store_dir = root->ctx.paths.store;

  if (sp_str_valid(spn.cli.graph.output)) {
    sp_io_writer_t writer = sp_io_writer_from_file(spn.cli.graph.output, SP_IO_WRITE_MODE_OVERWRITE);
    //spn_bg_to_mermaid(&app->builder.build.graph, dirty, &writer, app->paths.dir, spn.paths.cache, work_dir, store_dir);
    spn_render_to_mermaid(app, &writer);
    sp_io_writer_close(&writer);
  }
  else {
    //spn_bg_to_mermaid(&app->builder.build.graph, dirty, &spn.logger.out, app->paths.dir, spn.paths.cache, work_dir, store_dir);
    spn_render_to_mermaid(app, &spn.logger.out);
  }

  return SPN_TASK_DONE;
}

spn_task_result_t spn_task_generate(spn_app_t* app) {
  spn_cli_generate_t* command = &spn.cli.generate;

  spn_generator_t gen = {
    .kind = spn_gen_kind_from_str(command->generator),
    .compiler = spn_cc_kind_from_str(command->compiler)
  };
  gen.include = spn_gen_build_entries_for_all(SPN_GEN_INCLUDE, gen.compiler);
  gen.lib_include = spn_gen_build_entries_for_all(SPN_GEN_LIB_INCLUDE, gen.compiler);
  gen.libs = spn_gen_build_entries_for_all(SPN_GEN_LIBS, gen.compiler);
  gen.rpath = spn_gen_build_entries_for_all(SPN_GEN_RPATH, gen.compiler);

  spn_gen_format_context_t fmt = {
    .kind = SPN_GEN_SYSTEM_LIBS,
    .compiler = gen.compiler
  };
  sp_dyn_array(sp_str_t) entries = sp_str_map(app->resolver.system_deps, sp_dyn_array_size(app->resolver.system_deps), &fmt, spn_gen_format_entry_kernel);
  gen.system_libs = sp_str_join_n(entries, sp_dyn_array_size(entries), sp_str_lit(" "));

  switch (gen.kind) {
    case SPN_GEN_KIND_RAW: {
      gen.file_name = SP_LIT("spn.txt");
      gen.output = sp_format(
        "{} {} {} {} {}",
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }
    case SPN_GEN_KIND_SHELL: {
      gen.file_name = SP_LIT("spn.sh");
      const c8* template =
        "export SPN_INCLUDES=\"{}\"\n"
        "export SPN_LIB_INCLUDES=\"{}\"\n"
        "export SPN_LIBS=\"{}\"\n"
        "export SPN_SYSTEM_LIBS=\"{}\"\n"
        "export SPN_RPATH=\"{}\"\n"
        "export SPN_FLAGS=\"$SPN_INCLUDES $SPN_LIB_INCLUDES $SPN_LIBS $SPN_SYSTEM_LIBS $SPN_RPATH\"\n";
      gen.output = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    case SPN_GEN_KIND_MAKE: {
      gen.file_name = SP_LIT("spn.mk");
      const c8* template =
        "SPN_INCLUDES := {}\n"
        "SPN_LIB_INCLUDES := {}\n"
        "SPN_LIBS := {}\n"
        "SPN_SYSTEM_LIBS := {}\n"
        "SPN_RPATH := {}\n"
        "SPN_FLAGS := $(SPN_INCLUDES) $(SPN_LIB_INCLUDES) $(SPN_LIBS) $(SPN_SYSTEM_LIBS) $(SPN_RPATH)\n";
      gen.output = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    case SPN_GEN_KIND_CMAKE: {
      gen.file_name = SP_LIT("spn.cmake");
      const c8* template =
        "set(SPN_INCLUDES \"{}\")\n"
        "set(SPN_LIB_INCLUDES \"{}\")\n"
        "set(SPN_LIBS \"{}\")\n"
        "set(SPN_SYSTEM_LIBS \"{}\")\n"
        "set(SPN_RPATH \"{}\")\n"
        "set(SPN_FLAGS \"$";
      sp_str_t template_end = sp_str_lit(
        "{SPN_INCLUDES} $"
        "{SPN_LIB_INCLUDES} $"
        "{SPN_LIBS} $"
        "{SPN_SYSTEM_LIBS} $"
        "{SPN_RPATH}\")\n");
      sp_str_t formatted = sp_format(template,
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      gen.output = sp_str_concat(formatted, template_end);
      break;
    }

    case SPN_GEN_KIND_PKGCONFIG: {
      gen.file_name = SP_LIT("spn.pc");
      const c8* template =
        "Name: {}\n"
        "Description: spn-managed dependencies for {}\n"
        "Version: {}.{}.{}\n"
        "Cflags: {} {}\n"
        "Libs: {} {} {}\n";
      gen.output = sp_format(template,
        SP_FMT_STR(app->package.name),
        SP_FMT_STR(app->package.name),
        SP_FMT_U32(app->package.version.major),
        SP_FMT_U32(app->package.version.minor),
        SP_FMT_U32(app->package.version.patch),
        SP_FMT_STR(gen.include),
        SP_FMT_STR(gen.lib_include),
        SP_FMT_STR(gen.libs),
        SP_FMT_STR(gen.system_libs),
        SP_FMT_STR(gen.rpath)
      );
      break;
    }

    default: {
      SP_UNREACHABLE();
    }
  }

  if (sp_str_valid(command->path)) {
    sp_str_t destination = sp_fs_normalize_path(command->path);
    if (!sp_str_starts_with(destination, sp_str_lit("/"))) {
      destination = sp_fs_join_path(spn.paths.cwd, destination);
    }
    sp_fs_create_dir(destination);

    sp_str_t file_path = sp_fs_join_path(destination, gen.file_name);
    sp_io_writer_t file = sp_io_writer_from_file(file_path, SP_IO_WRITE_MODE_OVERWRITE);
    if (sp_io_write_str(&file, gen.output) != gen.output.len) {
      SP_FATAL("Failed to write {}", SP_FMT_STR(file_path));
    }
    sp_io_writer_close(&file);

    spn_event_buffer_push_ctx(spn.events, &spn_session_find_root(&app->session)->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_GENERATE,
      .generate.path = file_path
    });
  }
  else {
    // Write directly to stdout without treating as format string
    sp_io_write_str(&spn.logger.out, gen.output);
  }

  return SPN_TASK_DONE;
}

spn_task_result_t spn_task_which(spn_app_t* app) {
  spn_cli_which_t* cmd = &spn.cli.which;

  spn_pkg_dir_t kind = SPN_DIR_STORE;
  if (sp_str_valid(cmd->dir)) {
    kind = spn_cache_dir_kind_from_str(cmd->dir);
  }

  if (sp_str_valid(cmd->package)) {
    spn_pkg_unit_t* dep = spn_cli_assert_unit_exists(cmd->package);
    sp_str_t dir = spn_build_ctx_get_dir(&dep->ctx, kind);
    spn_log_info("{}", SP_FMT_STR(dir));
  }
  else {
    spn_log_info("{}", SP_FMT_STR(spn_cache_dir_kind_to_path(kind)));
  }

  return SPN_TASK_DONE;
}
