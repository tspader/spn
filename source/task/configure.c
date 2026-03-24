#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "graph/graph.h"
#include "intern.h"
#include "target/types.h"
#include "log/log.h"
#include "session/session.h"
#include "task.h"
#include "unit/package.h"

s32 configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* pkg = (spn_pkg_unit_t*)user_data;

  sp_try(spn_session_compile_pkg(pkg->ctx.session, pkg));

  if (pkg->on_configure) {
    spn_event_buffer_push(spn.events, &pkg->ctx, SPN_EVENT_BUILD_SCRIPT_CONFIGURE);

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(pkg, pkg->on_configure));
    pkg->time.configure = sp_tm_read_timer(&timer);

    spn_event_buffer_push_ctx(spn.events, &pkg->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
      .configure.time = pkg->time.configure,
    });
  }

  sp_om_for(pkg->targets, it) {
    spn_target_unit_t* target = sp_om_at(pkg->targets, it);
    sp_da_for(target->info->source, j) {
      sp_str_t relative = target->info->source[j];
      sp_str_t file = sp_fs_join_path(pkg->ctx.paths.source, target->info->source[j]);
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
          .target = target,
          .pkg = target->pkg,
          .profile = pkg->ctx.profile,
          .session = target->session,
          .paths = {
            .object = object_path,
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

spn_task_result_t spn_task_update_configure_graph(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    return sp_da_empty(build->executor->errors) ?
      SPN_TASK_DONE :
      SPN_TASK_ERROR;
  }

  return SPN_TASK_CONTINUE;
}

spn_err_t init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  root->nodes.configure.run = spn_bg_add_fn_ex(graph, configure_package, root, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
  root->nodes.configure.stamp = spn_bg_add_file(graph, root->paths.stamp.package);
  sp_try(spn_bg_cmd_add_output(graph, root->nodes.configure.run, root->nodes.configure.stamp));

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(b->units.packages, it);
    unit->nodes.configure.run = spn_bg_add_fn_ex(graph, configure_package, unit, SPN_BG_VIZ_DEFAULT, app->package.name, sp_str_lit("configure"));
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    sp_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));
    sp_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
  }

  sp_om_for(b->units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(b->units.packages, it);
    spn_pkg_t* pkg = dep->ctx.pkg;

    sp_ht_for(pkg->deps, dit) {
      sp_str_t parent_name = *sp_ht_it_getkp(pkg->deps, dit);
      spn_pkg_unit_t* parent = sp_om_get(b->units.packages, parent_name);
      if (!parent) continue;

      sp_try(spn_bg_cmd_add_input(graph, dep->nodes.configure.run, parent->nodes.configure.stamp));
    }
  }

  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;

  graph->error.some = SP_OPT_NONE;
  if (init_configure_graph(app)) {
    switch (graph->error.some) {
      case SP_OPT_SOME: {
        spn_log_error("{}", SP_FMT_STR(spn_bg_err_to_str(graph, graph->error.value)));
        break;
      }
      case SP_OPT_NONE: {
        spn_log_error("failed to prepare configure graph");
        break;
      }
    }

    return SPN_TASK_ERROR;
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
