#include "sp.h"
#include "sp/macro.h"
#include "app/types.h"
#include "ctx/types.h"
#include "error/types.h"
#include "forward/types.h"
#include "pkg/types.h"
#include "session/types.h"
#include "spn.h"
#include "unit/types.h"

#include "external/wasm/wasm.h"
#include "session/session.h"
#include "task/build/dag.h"
#include "task/task.h"
#include "unit/package.h"

static s32 on_configure_package(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_wasm_script_t* configure = &unit->wasm.configure;
  if (configure->state != SPN_WASM_SCRIPT_NONE) {
    spn_try(spn_wasm_script_open(configure, unit));
    if (spn_wasm_script_exports(configure, sp_str_lit("configure"))) {
      spn_try(spn_wasm_script_call(configure, unit, sp_str_lit("configure"), SPN_ABI_KIND_CONFIG, unit));
    }
  }
  spn_try(spn_pkg_unit_publish_headers(unit, false));
  spn_pkg_unit_write_stamp(unit, spn_dag_find_artifact(g, action->produces[0])->path);
  return SPN_OK;
}

static spn_target_unit_t* get_configure_target(spn_pkg_unit_t* unit) {
  if (unit->metaprogram.pkg != unit) {
    return SP_NULLPTR;
  }
  return unit->metaprogram.configure.target;
}

static spn_err_t add_configure_action(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;
  sp_assert(unit->metaprogram.pkg);

  unit->dag.configure.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .execute = on_configure_package,
    .user_data = unit,
    .uncacheable = true,
  });
  unit->dag.configure.stamp = spn_dag_add_file(g, unit->paths.stamp.configure);
  return spn_dag_action_add_output(g, unit->dag.configure.action, unit->dag.configure.stamp);
}

static void add_configure_edges(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  sp_da_for(unit->deps, it) {
    spn_pkg_unit_t* dep = unit->deps[it].unit;
    sp_assert(dep->dag.configure.stamp.occupied);
    spn_dag_action_add_input(g, unit->dag.configure.action, dep->dag.configure.stamp);
  }

  spn_target_unit_t* reactor = unit->metaprogram.configure.target;
  if (reactor) {
    spn_dag_action_add_input(g, unit->dag.configure.action, reactor->dag.output);
  }
}

static void add_reactor_edges(spn_dag_build_t* b, spn_target_unit_t* reactor) {
  spn_dag_t* g = b->graph;

  sp_da_for(reactor->pkg->deps, it) {
    spn_pkg_unit_t* dep = reactor->pkg->deps[it].unit;
    sp_assert(dep->dag.configure.stamp.occupied);
    sp_da_for(reactor->objects, ot) {
      spn_dag_action_add_input(g, reactor->objects[ot]->dag.action, dep->dag.configure.stamp);
    }
  }
}

spn_task_step_t spn_task_configure_graph_init(spn_app_t* app) {
  spn_session_t* session = &app->session;

  if (spn_wasm_init()) {
    return spn_task_fail(SPN_ERR_WASM_INIT_FAILED);
  }

  spn_dag_build_t* dag = spn_dag_build_new(session);
  session->dag.configure = dag;

  sp_om_for(session->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(session->units.packages, it);
    spn_target_unit_t* configure = get_configure_target(unit);
    if (configure) {
      if (spn_dag_build_add_target(dag, configure)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  sp_da_for(session->units.builds, it) {
    spn_build_unit_t* build = session->units.builds[it];
    sp_da_for(build->packages, jt) {
      spn_pkg_unit_t* unit = build->packages[jt];
      if (add_configure_action(dag, unit)) {
        return spn_task_fail(SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure });
      }
    }
  }

  sp_da_for(session->units.builds, it) {
    spn_build_unit_t* build = session->units.builds[it];
    sp_da_for(build->packages, jt) {
      add_configure_edges(dag, build->packages[jt]);
    }
  }

  sp_om_for(session->units.packages, it) {
    spn_target_unit_t* reactor = get_configure_target(sp_om_at(session->units.packages, it));
    if (reactor) {
      add_reactor_edges(dag, reactor);
    }
  }

  spn_dag_build_start(dag, 8);
  return spn_task_continue();
}

spn_task_step_t spn_task_configure_graph_update(spn_app_t* app) {
  spn_dag_build_t* b = app->session.dag.configure;
  if (!spn_dag_build_poll(b)) {
    return spn_task_continue();
  }
  if (b->result) {
    return spn_task_fail(SPN_ERROR, .reported = true);
  }
  return spn_task_done();
}
