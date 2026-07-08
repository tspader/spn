#include "sp.h"
#include "sp/macro.h"
#include "api/core/types.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "graph/types.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "session/registry/types.h"
#include "spn.h"
#include "target/types.h"
#include "unit/types.h"

#include "event/event.h"
#include "external/wasm/wasm.h"
#include "graph/graph.h"
#include "sp/sp_glob.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/task.h"
#include "unit/package.h"

s32 on_configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_wasm_script_t* configure = &unit->wasm.configure;
  if (configure->state != SPN_WASM_SCRIPT_NONE) {
    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_compile_script_module(unit, &unit->info->configure, configure->path));
    unit->time.compile = sp_tm_read_timer(&timer);

    spn_try(spn_wasm_script_open(configure, unit));
    if (spn_wasm_script_exports(configure, sp_str_lit("configure"))) {
      spn_try(spn_wasm_script_call(configure, unit, sp_str_lit("configure"), SPN_ABI_KIND_CONFIG, unit));
    }
  }

  spn_try(spn_pkg_unit_publish_headers(unit, false));
  return SPN_OK;
}

spn_task_step_t spn_task_configure_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, spn.mem);
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  if (spn_wasm_init()) {
    return spn_task_fail(SPN_ERR_WASM_INIT_FAILED);
  }

  // Add a graph node for each package
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    if (spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp)) {
      return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
    }
  }

  // The root configures last, after every other package; only now do all units
  // have their nodes, so this can't be folded into the loop above
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_loaded_pkg_t* pkg = sp_ht_getp(session->packages, unit->id);

    if (pkg->source != SPN_PKG_SOURCE_ROOT) {
      if (spn_bg_cmd_add_input(graph, root->nodes.configure.run, unit->nodes.configure.stamp)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  // Add links between packages
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, unit);
    sp_da_for(deps, j) {
      spn_pkg_unit_t* parent = deps[j].unit;
      if (spn_bg_cmd_add_input(graph, unit->nodes.configure.run, parent->nodes.configure.stamp)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = parent->paths.stamp.configure });
      }
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

  return spn_task_continue();
}

spn_task_step_t spn_task_configure_graph_update(spn_app_t* app) {
  spn_bg_ctx_t* build = &app->session.configure;

  if (sp_atomic_s32_get(&build->executor->shutdown)) {
    if (sp_da_empty(build->executor->errors)) {
      return spn_task_done();
    }
    return spn_task_fail(SPN_ERROR);
  }

  return spn_task_continue();
}

