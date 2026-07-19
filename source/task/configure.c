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
#include "session/types.h"
#include "spn.h"
#include "unit/types.h"

#include "external/wasm/wasm.h"
#include "graph/graph.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/build/target.h"
#include "task/task.h"
#include "unit/package.h"

s32 on_configure_package(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_wasm_script_t* configure = &unit->wasm.configure;
  if (configure->state != SPN_WASM_SCRIPT_NONE) {
    spn_try(spn_wasm_script_open(configure, unit));
    if (spn_wasm_script_exports(configure, sp_str_lit("configure"))) {
      spn_try(spn_wasm_script_call(configure, unit, sp_str_lit("configure"), SPN_ABI_KIND_CONFIG, unit));
    }
  }
  spn_try(spn_pkg_unit_publish_headers(unit, false));
  return SPN_OK;
}

static spn_err_t add_configure_package(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  if (unit->nodes.configure.run.occupied) {
    return SPN_OK;
  }
  unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
  unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
  spn_try(spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp));

  sp_assert(unit->metaprogram.pkg);
  spn_target_unit_t* target = unit->metaprogram.configure.target;
  if (target) {
    spn_try(spn_build_add_target_nodes(graph, target));
    spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, target->nodes.output));
  }
  return SPN_OK;
}

static spn_err_t add_configure_package_edges(spn_build_graph_t* graph, spn_pkg_unit_t* unit) {
  sp_da_for(unit->deps, it) {
    spn_pkg_unit_t* dep = unit->deps[it].unit;
    sp_assert(dep->nodes.configure.stamp.occupied);
    spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, dep->nodes.configure.stamp));
  }

  spn_target_unit_t* target = unit->metaprogram.configure.target;
  if (!target) {
    return SPN_OK;
  }
  sp_da_for(target->pkg->deps, it) {
    spn_pkg_unit_t* dep = target->pkg->deps[it].unit;
    sp_assert(dep->nodes.configure.stamp.occupied);
    sp_da_for(target->objects, ot) {
      spn_try(spn_bg_cmd_add_input(graph, target->objects[ot]->nodes.compile, dep->nodes.configure.stamp));
    }
  }
  return SPN_OK;
}

spn_task_step_t spn_task_configure_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, spn.mem);

  if (spn_wasm_init()) {
    return spn_task_fail(SPN_ERR_WASM_INIT_FAILED);
  }

  sp_da_for(session->units.builds, it) {
    spn_build_unit_t* build = session->units.builds[it];
    sp_da_for(build->packages, jt) {
      spn_pkg_unit_t* unit = build->packages[jt];
      if (add_configure_package(graph, unit)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  sp_da_for(session->units.builds, it) {
    spn_build_unit_t* build = session->units.builds[it];
    sp_da_for(build->packages, jt) {
      spn_pkg_unit_t* unit = build->packages[jt];
      if (add_configure_package_edges(graph, unit)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  session->configure.dirty = spn_bg_compute_forced_dirty(graph);
  session->configure.executor = spn_bg_executor_new(
    graph,
    session->configure.dirty,
    (spn_bg_executor_config_t) {
      .num_threads = 8,
      .on_worker_exit = spn_wasm_thread_exit,
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
    return spn_task_fail(SPN_ERROR, .reported = true);
  }
  return spn_task_continue();
}
