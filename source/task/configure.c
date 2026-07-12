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
#include "task/build/nodes/nodes.h"
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

static spn_target_unit_t* find_configure_target(spn_session_t* session, spn_pkg_unit_t* unit) {
  spn_target_unit_t* target = spn_session_find_target_in_pkg(session, unit, sp_str_lit("configure"));
  if (!target || target->info->kind != SPN_TARGET_MODULE) {
    return SP_NULLPTR;
  }
  return target;
}

static spn_err_t add_configure_module(spn_build_graph_t* graph, spn_session_t* session, spn_target_unit_t* target, spn_bg_id_t module) {
  spn_bg_id_t link = spn_bg_add_fn(graph, link_target, target);
  spn_try(spn_bg_cmd_add_output(graph, link, module));

  sp_da_for(target->objects, it) {
    spn_compile_unit_t* object = target->objects[it];
    object->nodes.source = spn_bg_add_file(graph, object->paths.file);
    object->nodes.compile = spn_bg_add_fn(graph, compile_object, object);
    object->nodes.object = spn_bg_add_file(graph, object->paths.object);
    spn_try(spn_bg_cmd_add_input(graph, object->nodes.compile, object->nodes.source));
    spn_try(spn_bg_cmd_add_output(graph, object->nodes.compile, object->nodes.object));
    spn_try(spn_bg_cmd_add_input(graph, link, object->nodes.object));
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

  // Add a graph node for each package; configure modules compile and link in
  // this graph so the run node consumes a pipeline-built artifact. Every unit
  // with a configure module runs it with its own mounts; the module itself
  // compiles once, wired where the target lives
  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);

    unit->nodes.configure.run = spn_bg_add_fn(graph, on_configure_package, unit);
    unit->nodes.configure.stamp = spn_bg_add_file(graph, unit->paths.stamp.configure);
    if (spn_bg_cmd_add_output(graph, unit->nodes.configure.run, unit->nodes.configure.stamp)) {
      return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
    }
    if (unit->wasm.configure.state != SPN_WASM_SCRIPT_NONE) {
      spn_bg_id_t module = spn_bg_add_file(graph, unit->wasm.configure.path);
      if (spn_bg_cmd_add_input(graph, unit->nodes.configure.run, module)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->wasm.configure.path });
      }
      spn_target_unit_t* target = find_configure_target(session, unit);
      if (target && add_configure_module(graph, session, target, module)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->wasm.configure.path });
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

    // The configure module compiles against its target-level deps' published
    // headers, so its objects wait on those packages' configure
    spn_target_unit_t* configure = find_configure_target(session, unit);
    if (!configure) {
      continue;
    }
    sp_da_for(configure->deps.package, j) {
      spn_pkg_unit_t* parent = configure->deps.package[j];
      sp_da_for(configure->objects, ot) {
        if (!configure->objects[ot]->nodes.compile.occupied) {
          continue;
        }
        if (spn_bg_cmd_add_input(graph, configure->objects[ot]->nodes.compile, parent->nodes.configure.stamp)) {
          return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = parent->paths.stamp.configure });
        }
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
