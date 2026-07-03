#include "sp.h"
#include "sp/macro.h"
#include "api/core/types.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/types.h"
#include "forward/types.h"
#include "graph/types.h"
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

  if (!sp_fs_is_file(unit->paths.configure)) return SPN_OK;

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_try(spn_compile_script_module(unit, &unit->info->configure, unit->paths.wasm.configure));
  unit->time.compile = sp_tm_read_timer(&timer);

  spn_try(spn_wasm_script_open(&unit->wasm.configure, unit, unit->paths.wasm.configure));
  spn_try(spn_wasm_script_call_hook(unit->wasm.configure, unit, "configure"));
  return SPN_OK;
}

spn_task_result_t spn_task_init_configure_graph(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, spn.mem);
  spn_pkg_unit_t* root = spn_session_find_root(&app->session);

  if (spn_wasm_init_stupid_global_runtime()) {
    return SPN_TASK_ERROR;
  }

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

