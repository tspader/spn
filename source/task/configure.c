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
    if (unit->deps[it].kind == SPN_DEP_KIND_BUILD) {
      continue;
    }
    spn_pkg_unit_t* dep = unit->deps[it].unit;
    spn_try(spn_bg_cmd_add_input(graph, unit->nodes.configure.run, dep->nodes.configure.stamp));
  }
  return SPN_OK;
}

static spn_err_t add_configure_target_edges(spn_build_graph_t* graph, spn_target_unit_t* target) {
  if (!target->nodes.link.occupied) {
    return SPN_OK;
  }
  sp_da_for(target->deps.package, it) {
    spn_pkg_unit_t* dep = target->deps.package[it];
    sp_assert(dep->nodes.configure.stamp.occupied);
    sp_da_for(target->objects, it) {
      spn_try(spn_bg_cmd_add_input(graph, target->objects[it]->nodes.compile, dep->nodes.configure.stamp));
    }
  }
  return SPN_OK;
}

static void collect_metaprogram_dependencies(sp_da(spn_pkg_unit_t*)* dependencies, spn_target_unit_t* target) {
  if (!target) {
    return;
  }
  sp_da_for(target->deps.package, it) {
    spn_pkg_unit_t* dependency = target->deps.package[it];
    bool present = false;
    sp_da_for(*dependencies, it) {
      if ((*dependencies)[it] == dependency) {
        present = true;
        break;
      }
    }
    if (!present) {
      sp_da_push(*dependencies, dependency);
    }
  }
}

spn_task_step_t spn_task_configure_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;
  spn_build_graph_t* graph = &session->configure.graph;
  spn_bg_init(graph, spn.mem);

  if (spn_wasm_init()) {
    return spn_task_fail(SPN_ERR_WASM_INIT_FAILED);
  }

  sp_da(spn_pkg_unit_t*) dependencies = sp_da_new(session->mem, spn_pkg_unit_t*);
  sp_da_for(session->units.metaprogram->packages, it) {
    spn_pkg_unit_t* unit = session->units.metaprogram->packages[it];
    collect_metaprogram_dependencies(&dependencies, unit->metaprogram.configure.target);
    collect_metaprogram_dependencies(&dependencies, unit->metaprogram.build.target);
  }

  sp_da_for(dependencies, it) {
    spn_pkg_unit_t* unit = dependencies[it];
    if (add_configure_package(graph, unit)) {
      return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* unit = build->packages[it];
      if (add_configure_package(graph, unit)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  sp_da_for(dependencies, it) {
    spn_pkg_unit_t* unit = dependencies[it];
    if (add_configure_package_edges(graph, unit)) {
      return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
    }
  }

  sp_da_for(session->plan.builds, it) {
    spn_build_unit_t* build = session->plan.builds[it].build;
    sp_da_for(build->packages, it) {
      spn_pkg_unit_t* unit = build->packages[it];
      if (add_configure_package_edges(graph, unit)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  sp_da_for(session->units.metaprogram->packages, it) {
    spn_target_unit_t* target = session->units.metaprogram->packages[it]->metaprogram.configure.target;
    if (!target) {
      continue;
    }
    if (add_configure_target_edges(graph, target)) {
      return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = get_target_output_path(session->mem, target) });
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
