#include "api/core/types.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "graph/types.h"
#include "pkg/types.h"
#include "session/registry/types.h"
#include "target/types.h"
#include "unit/types.h"

#include "event/event.h"
#include "external/cc.h"
#include "external/tcc/tcc.h"
#include "graph/graph.h"
#include "sp/sp_glob.h"
#include "session/session.h"
#include "task/task.h"
#include "unit/package.h"

#include <setjmp.h>

s32 download_toolchain(spn_bg_cmd_t* cmd, void* user_data) {
  spn_toolchain_unit_t* unit = (spn_toolchain_unit_t*)user_data;
  spn_session_t* session = unit->session;

  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_BUILD
  });

  if (sp_str_empty(unit->url)) {
    return SPN_OK;
  }

  sp_str_t output = sp_fs_join_path(spn.mem, unit->paths.work, sp_fs_get_name(unit->url));

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
  sp_ps_output_t dl = sp_ps_run(spn.mem, (sp_ps_config_t) {
    .command = curl,
    .args = {
      sp_str_lit("-fSL"),
      sp_str_lit("-o"), output,
      unit->url
    }
  });
  if (dl.status.exit_code) return SPN_ERROR;

  sp_ps_output_t extract = sp_ps_run(spn.mem, (sp_ps_config_t) {
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
  if (!sp_fs_exists(unit->paths.script)) {
    return SPN_OK;
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_tcc_err_ctx_t error_context = SP_ZERO_INITIALIZE();

  spn_cc_t cc = SP_ZERO_INITIALIZE();
  spn_cc_add_runtime(&cc, spn.paths.runtime, spn.paths.include);
  spn_cc_set_profile(&cc, session->profile);
  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_OUTPUT_JIT, unit->info->name);
  spn_cc_target_add_absolute_source(target, unit->paths.script);

  // Build deps are usable from the build script itself, so compile against them
  sp_ht_for_kv(unit->info->deps, it) {
    spn_requested_pkg_t* dep = it.val;
    if (dep->kind != SPN_DEP_KIND_BUILD) continue;

    spn_pkg_unit_t* dep_unit = spn_session_find_pkg_by_qualified(session, dep->qualified);
    if (!dep_unit) continue;

    spn_cc_target_add_absolute_include(target, dep_unit->paths.include);
    spn_cc_target_add_absolute_include(target, dep_unit->paths.source);
  }

  unit->tcc = sp_alloc_type(session->mem, spn_tcc_t);
  spn_tcc_init(unit->tcc);
  s32 try_err = 0;
  spn_try_goto(spn_cc_target_to_tcc(&cc, target, unit->tcc), try_err, fail);
  spn_try_goto(tcc_relocate(unit->tcc->s), try_err, fail);

  unit->on_configure = tcc_get_symbol(unit->tcc->s, "configure");
  unit->on_package = tcc_get_symbol(unit->tcc->s, "package");

  unit->time.compile = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_COMPILE,
    .pkg = unit->info,
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
    .pkg = unit->info,
    .io = &unit->logs.io,
    .compile_failed = {
      .script_path = unit->paths.script,
      .error = error_context.error,
    }
  });
  return SPN_ERROR;
}

spn_err_t configure_package(spn_pkg_unit_t* unit) {
  if (!unit->on_configure) return SPN_OK;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE,
    .pkg = unit->info,
    .io = &unit->logs.io
  });

  sp_tm_timer_t timer = sp_tm_start_timer();
  jmp_buf jump;
  int status = tcc_setjmp(unit->tcc->s, jump, unit->on_configure);
  if (!status) {
    spn_t* spn = (spn_t*)unit;
    spn_config_t* configure = (spn_config_t*)unit;
    unit->on_configure(spn, configure);
  }
  else {
    // @spader @log
    // What else can we get from TCC here?
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .pkg = unit->info,
      .io = &unit->logs.io,
      .crashed.path = sp_str_lit("")
    });
    return SPN_ERROR;
  }

  unit->time.configure = sp_tm_read_timer(&timer);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_CONFIGURE_OK,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .configure.time = unit->time.configure,
  });

  return SPN_OK;
}

s32 on_configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_try(compile_package(unit->session, unit));
  spn_try(configure_package(unit));
  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, app->session.mem);
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  // Add a graph node for each package
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    spn_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));
  }

  // The root configures last, after every other package; only now do all units
  // have their nodes, so this can't be folded into the loop above
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_loaded_pkg_t* pkg = sp_str_ht_get(session->packages, unit->info->qualified);

    if (pkg->source != SPN_PKG_SOURCE_ROOT) {
      spn_try(spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp));
    }
  }

  // Add links between packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_str_om_at(session->units.packages, it);

    sp_da(spn_pkg_unit_t*) deps = *sp_om_get(session->units.graph, unit->id);
    sp_da_for(deps, j) {
      spn_pkg_unit_t* parent = deps[j];
      spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, parent->nodes.configure.stamp));
    }
  }

  // If we're downloading a toolchain, it needs a node which everything depends on
  if (session->units.toolchain) {
    spn_toolchain_unit_t* toolchain = session->units.toolchain;
    toolchain->nodes.download = spn_bg_add_fn(graph, download_toolchain, toolchain);
    toolchain->nodes.stamp = spn_bg_add_file(graph, toolchain->paths.stamp);
    spn_try(spn_bg_cmd_add_output(graph, toolchain->nodes.download, toolchain->nodes.stamp));

    sp_str_om_for(session->units.packages, it) {
      spn_pkg_unit_t* unit = sp_str_om_at(session->units.packages, it);
      spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, toolchain->nodes.stamp));
    }
  }

  session->configure.dirty = spn_bg_compute_forced_dirty(graph);
  session->configure.executor = spn_bg_executor_new(
    graph,
    session->configure.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 16
    }
  );
  spn_bg_executor_run(session->configure.executor);

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

