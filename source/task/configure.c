#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "graph/types.h"
#include "spn.h"
#include "target/types.h"

#include "event/event.h"
#include "external/cc.h"
#include "graph/graph.h"
#include "intern.h"
#include "sp/glob.h"
#include "session/session.h"
#include "task.h"
#include "unit/package.h"
#include "unit/types.h"

s32 download_toolchain(spn_bg_cmd_t* cmd, void* user_data) {
  spn_toolchain_unit_t* unit = (spn_toolchain_unit_t*)user_data;
  spn_session_t* session = unit->session;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD
  });

  sp_str_t output = sp_fs_join_path(unit->paths.work, sp_fs_get_name(unit->url));

  // This function runs as part of the configure graph, which is a DAG ordered
  // by dependency traversal. Normally, we'd model checks like this in the structure
  // of the graph. However, the configure graph is only a graph for the ordering
  // properties. We don't actually want to gate its execution on dirtiness, because
  // all of its nodes need to run every time (e.g. calling script::configure())
  //
  // But we ALSO need to have the toolchain before we configure. It's probably best
  // to just download outside of the graph, but I figured that if there was *any*
  // work that could be done while we're downloading, it's worthwhile.
  //
  // I hard gate everything on this node, though, so it's functionally sync.
  if (sp_fs_exists(unit->paths.stamp)) return 0;

  sp_str_t curl = sp_env_get(&session->env, sp_str_lit("SPN_CURL"));
  if (sp_str_empty(curl)) curl = sp_str_lit("curl");
  sp_ps_output_t dl = sp_ps_run((sp_ps_config_t) {
    .command = curl,
    .args = {
      sp_str_lit("-fSL"),
      sp_str_lit("-o"), output,
      unit->url
    }
  });
  if (dl.status.exit_code) return SPN_ERROR;

  sp_ps_output_t extract = sp_ps_run((sp_ps_config_t) {
    .command = sp_str_lit("tar"),
    .args = {
      sp_str_lit("xf"), output,
      sp_str_lit("--strip-components=1"),
      sp_str_lit("-C"), unit->paths.store,
    }
  });
  if (extract.status.exit_code) return SPN_ERROR;

  sp_fs_create_file(unit->paths.stamp);

  return SPN_OK;
}

spn_err_t compile_package(spn_session_t* session, spn_pkg_unit_t* unit) {
  spn_pkg_t* pkg = unit->pkg;

  if (!sp_fs_exists(unit->paths.script)) {
    return SPN_OK;
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_tcc_err_ctx_t error_context = SP_ZERO_INITIALIZE();

  spn_tcc_t* tcc = tcc_new();
  sp_try_goto(spn_tcc_prepare_script(tcc, &error_context), fail);

  spn_cc_t cc = SP_ZERO_INITIALIZE();
  spn_cc_set_profile(&cc, session->profile);
  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_TARGET_JIT, pkg->name);
  sp_ht_for_kv(pkg->deps, it) {
    switch (it.val->visibility) {
      case SPN_VISIBILITY_BUILD: {
        spn_cc_target_add_dep(target, spn_session_find_pkg(session, *it.key));
        break;
      }
      case SPN_VISIBILITY_SCRIPT:
      case SPN_VISIBILITY_TEST:
      case SPN_VISIBILITY_PUBLIC: {
        break;
      }
    }
  }

  spn_cc_target_to_tcc(&cc, target, tcc);
  sp_try_goto(spn_tcc_add_file(tcc, unit->paths.script), fail);
  sp_try_goto(tcc_relocate(tcc), fail);

  unit->tcc = tcc;
  unit->on_configure = tcc_get_symbol(tcc, "configure");
  unit->on_package = tcc_get_symbol(tcc, "package");

  unit->time.compile = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE,
    .pkg = unit->pkg,
    .io = &unit->logs.io,
    .script_compile = {
      .script_path = unit->paths.script,
      .time = unit->time.compile,
      .has_configure = unit->on_configure != SP_NULLPTR,
      .has_package = unit->on_package != SP_NULLPTR,
    }
  });

  return SPN_OK;

fail:
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE_FAILED,
    .pkg = unit->pkg,
    .io = &unit->logs.io,
    .compile_failed = {
      .script_path = unit->paths.script,
      .error = error_context.error,
    }
  });
  return SPN_ERROR;
}

spn_err_t configure_package(spn_pkg_unit_t* pkg) {
  if (pkg->on_configure) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE,
      .pkg = pkg->pkg,
      .io = &pkg->logs.io
    });

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(pkg, pkg->on_configure));
    pkg->time.configure = sp_tm_read_timer(&timer);

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
      .pkg = pkg->pkg,
      .io = &pkg->logs.io,
      .configure.time = pkg->time.configure,
    });
  }

  return SPN_OK;
}

s32 on_configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_try(compile_package(unit->session, unit));
  spn_try(configure_package(unit));
  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* b = &app->session;
  spn_build_graph_t* graph = &b->configure.graph;
  spn_session_t* session = &app->session;
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  graph->error.some = SP_OPT_NONE;

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    spn_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));

    if (unit != root) {
      spn_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
    }
  }

  // If we're downloading a toolchain, it needs a node which everything depends on
  if (session->units.toolchain) {
    spn_toolchain_unit_t* toolchain = session->units.toolchain;
    toolchain->nodes.download = spn_bg_add_fn(graph, download_toolchain, toolchain);
    toolchain->nodes.stamp = spn_bg_add_file(graph, toolchain->paths.stamp);
    spn_try(spn_bg_cmd_add_output(graph, toolchain->nodes.download, toolchain->nodes.stamp));

    sp_om_for(session->units.packages, it) {
      spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
      spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, toolchain->nodes.stamp));
    }
  }

  // Add links between packages
  sp_om_for(session->units.packages, p) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, p);

    sp_str_ht_for_kv(unit->deps, it) {
      spn_pkg_unit_t* parent = *it.val;
      spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, parent->nodes.configure.stamp));
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

spn_task_result_t spn_task_update_configure_graph(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    return sp_da_empty(build->executor->errors) ?
      SPN_TASK_DONE :
      SPN_TASK_ERROR;
  }

  return SPN_TASK_CONTINUE;
}

