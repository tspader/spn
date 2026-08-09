#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"
#include "error/types.h"
#include "core/types.h"
#include "pkg/types.h"
#include "session/types.h"
#include "spn/core.h"
#include "unit/types.h"

#include "external/wasm/wasm.h"
#include "graph/build.h"
#include "graph/dag.h"
#include "op/types.h"
#include "session/session.h"
#include "unit/package.h"
#include "unit/unit.h"

static s32 on_configure_package(spn_dag_t* g, spn_dag_action_t* action, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;
  spn_pkg_unit_create_layout(unit);
  spn_wasm_script_t* configure = &unit->wasm.configure;
  if (configure->state != SPN_WASM_SCRIPT_NONE) {
    spn_try(spn_wasm_script_open(configure, unit));
    if (spn_wasm_script_exports(configure, sp_str_lit("configure"))) {
      spn_try(spn_wasm_script_call(configure, unit, sp_str_lit("configure"), SPN_ABI_KIND_CONFIG, unit));
    }
  }
  spn_try(spn_pkg_unit_publish_headers(unit, false));
  spn_try(spn_build_publish_copies(unit, unit->paths.include, false, SP_NULLPTR));
  spn_pkg_unit_write_stamp(unit, spn_dag_find_artifact(g, action->produces[0])->path);
  return SPN_OK;
}

static spn_err_t add_configure_action(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  spn_dag_pkg_ids_t ids = sp_zero;
  ids.action = spn_dag_add_action(g, (spn_dag_action_config_t) {
    .execute = on_configure_package,
    .user_data = unit,
    .uncacheable = true,
  });
  ids.stamp = spn_dag_add_file(g, unit->paths.stamp.configure);
  spn_try(spn_dag_action_add_output(g, ids.action, ids.stamp));

  sp_ht_insert(b->ids.packages, unit, ids);
  return SPN_OK;
}

static void add_configure_edges(spn_dag_build_t* b, spn_pkg_unit_t* unit) {
  spn_dag_t* g = b->graph;

  spn_dag_pkg_ids_t* unit_ids = sp_ht_getp(b->ids.packages, unit);
  sp_assert(unit_ids);
  spn_dag_id_t action = unit_ids->action;

  sp_da_for(unit->deps, it) {
    spn_dag_pkg_ids_t* dep = sp_ht_getp(b->ids.packages, unit->deps[it].unit);
    sp_assert(dep);
    spn_dag_action_add_input(g, action, dep->stamp);
  }

  if (unit->metaprogram && unit->metaprogram->scripts.configure) {
    spn_dag_target_ids_t* ids = sp_ht_getp(b->ids.targets, unit->metaprogram->scripts.configure);
    sp_assert(ids);
    spn_dag_action_add_input(g, action, ids->output);
  }
}

static void add_reactor_edges(spn_dag_build_t* b, spn_target_unit_t* reactor) {
  spn_dag_t* g = b->graph;

  sp_da_for(reactor->pkg->deps, it) {
    if (!spn_dep_kind_applies(reactor->pkg->deps[it].kind, reactor->info->kind)) {
      continue;
    }
    spn_dag_pkg_ids_t* dep = sp_ht_getp(b->ids.packages, reactor->pkg->deps[it].unit);
    sp_assert(dep);
    spn_dag_id_t stamp = dep->stamp;

    sp_da_for(reactor->objects, ot) {
      spn_dag_object_ids_t* object = sp_ht_getp(b->ids.objects, reactor->objects[ot]);
      sp_assert(object);
      spn_dag_action_add_input(g, object->action, stamp);
    }
  }
}

spn_err_union_t configure(spn_op_t* op) {
  spn_session_t* s = op->session;
  if (spn_wasm_init()) {
    return (spn_err_union_t) { .kind = SPN_ERR_WASM_INIT_FAILED };
  }

  spn_try_union(spn_units_add_packages(s));
  spn_try_union(spn_units_add_targets(s, SPN_UNIT_SCOPE_METAPROGRAM));

  spn_dag_build_t* dag = spn_dag_build_new(op);
  s->dag.configure = dag;

  sp_da_for(s->units.metaprogram->packages, it) {
    spn_target_unit_t* configure = s->units.metaprogram->packages[it]->scripts.configure;
    if (!configure) {
      continue;
    }
    spn_try_union(spn_dag_build_add_target(dag, configure));
  }

  sp_om_for(s->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(s->units.packages, it);
    if (spn_pkg_unit_is_script_host(unit)) {
      continue;
    }
    if (add_configure_action(dag, unit)) {
      return (spn_err_union_t) { .kind = SPN_ERR_BUILD_GRAPH, .build_graph = { .file = unit->paths.stamp.configure } };
    }
  }

  sp_om_for(s->units.packages, it) {
    spn_pkg_unit_t* unit = sp_om_at(s->units.packages, it);
    if (spn_pkg_unit_is_script_host(unit)) {
      continue;
    }
    add_configure_edges(dag, unit);
  }

  sp_da_for(s->units.metaprogram->packages, it) {
    spn_target_unit_t* configure = s->units.metaprogram->packages[it]->scripts.configure;
    if (configure) {
      add_reactor_edges(dag, configure);
    }
  }

  spn_try_union(spn_dag_build_run(dag, 8));
  return spn_result(SPN_OK);
}
