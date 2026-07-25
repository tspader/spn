#include "sp.h"
#include "app/types.h"
#include "compiler/driver.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/event.h"
#include "pkg/types.h"
#include "resolve/types.h"
#include "session/session.h"
#include "session/types.h"
#include "spn.h"
#include "task/task.h"
#include "task/types.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "unit/types.h"

typedef struct {
  spn_pkg_unit_t* unit;
  u32 kinds;
} spn_closure_item_t;

static spn_pkg_unit_t* closure_add(spn_session_t* s, spn_build_unit_t* build, spn_pkg_id_t id, u32 kinds, sp_da(spn_closure_item_t)* pending) {
  spn_pkg_unit_t* unit = spn_session_find_pkg_unit(s, build, id);
  if (unit) {
    return unit;
  }

  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
  sp_assert(loaded);
  unit = spn_session_add_pkg_unit(s, build, id, loaded);
  sp_da_push(build->packages, unit);
  sp_da_push(*pending, ((spn_closure_item_t) { .unit = unit, .kinds = kinds }));
  return unit;
}

static void drain_closure(spn_session_t* s, spn_build_unit_t* build, sp_da(spn_closure_item_t)* pending) {
  bool world = build == s->units.metaprogram;
  u32 dep_kinds = world ?
    spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_BUILD) :
    spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE);

  sp_for(it, sp_da_size(*pending)) {
    spn_closure_item_t item = (*pending)[it];
    spn_resolved_pkg_t* resolved = sp_ht_getp(s->resolve, item.unit->id.pkg);
    sp_assert(resolved);

    sp_da_for(resolved->edges, et) {
      spn_resolved_dep_t* edge = &resolved->edges[et];
      if (!(spn_dep_kind_bit(edge->kind) & item.kinds)) {
        continue;
      }
      sp_da_push(item.unit->deps, ((spn_pkg_dep_t) {
        .unit = closure_add(s, build, edge->id, dep_kinds, pending),
        .kind = world ? SPN_DEP_KIND_PACKAGE : edge->kind,
        .private = edge->private,
      }));
    }
  }
}

static void add_build_packages(spn_session_t* s, spn_build_unit_t* build, spn_pkg_id_t root) {
  sp_da(spn_closure_item_t) pending = sp_da_new(s->mem, spn_closure_item_t);
  closure_add(s, build, root, spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_TEST), &pending);
  drain_closure(s, build, &pending);
}

static bool pkg_has_metaprogram(spn_loaded_pkg_t* loaded) {
  return !sp_da_empty(loaded->configure.source) || !sp_da_empty(loaded->build.source);
}

static void add_metaprogram_packages(spn_session_t* s) {
  spn_build_unit_t* world = s->units.metaprogram;
  u32 world_kinds = spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_BUILD);

  sp_da(spn_pkg_id_t) owners = sp_da_new(s->mem, spn_pkg_id_t);
  sp_ht(spn_pkg_id_t, u8) seen = SP_NULLPTR;
  sp_ht_init(s->mem, seen);
  sp_da_for(s->plan.builds, it) {
    spn_build_unit_t* build = s->plan.builds[it].build;
    sp_da_for(build->packages, jt) {
      spn_pkg_id_t id = build->packages[jt]->id.pkg;
      if (sp_ht_getp(seen, id)) {
        continue;
      }
      sp_ht_insert(seen, id, (u8)true);
      if (pkg_has_metaprogram(sp_ht_getp(s->packages, id))) {
        sp_da_push(owners, id);
      }
    }
  }

  sp_da(spn_closure_item_t) pending = sp_da_new(s->mem, spn_closure_item_t);
  sp_da_for(owners, it) {
    spn_resolved_pkg_t* resolved = sp_ht_getp(s->resolve, owners[it]);
    sp_assert(resolved);
    sp_da_for(resolved->edges, et) {
      if (resolved->edges[et].kind == SPN_DEP_KIND_BUILD) {
        closure_add(s, world, resolved->edges[et].id, world_kinds, &pending);
      }
    }
  }
  drain_closure(s, world, &pending);

  sp_da(spn_closure_item_t) hosts = sp_da_new(s->mem, spn_closure_item_t);
  sp_da_for(owners, it) {
    if (spn_session_find_pkg_unit(s, world, owners[it])) {
      continue;
    }
    spn_pkg_unit_t* host = spn_session_add_pkg_unit(s, world, owners[it], sp_ht_getp(s->packages, owners[it]));
    sp_da_push(world->hosts, host);
    sp_da_push(hosts, ((spn_closure_item_t) { .unit = host, .kinds = spn_dep_kind_bit(SPN_DEP_KIND_BUILD) }));
  }
  drain_closure(s, world, &hosts);
}

static spn_err_union_t validate_build_flags(spn_session_t* s) {
  sp_da_for(s->plan.builds, it) {
    spn_build_unit_t* build = s->plan.builds[it].build;
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_flags_t flags = sp_zero;
    spn_err_union_t err = spn_cc_render_flags(scratch.mem, &build->toolchain->cc, &build->profile, &flags);
    sp_mem_end_scratch(scratch);
    if (err.kind) {
      return err;
    }
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_plan(spn_app_t* app) {
  spn_session_t* s = &app->session;
  spn_pkg_id_t root = spn_session_root_pkg(s);

  try_task(validate_build_flags(s));
  sp_da_for(s->plan.builds, it) {
    add_build_packages(s, s->plan.builds[it].build, root);
  }
  add_metaprogram_packages(s);
  try_task(add_metaprogram_units(s));

  // @spader This is a stupid workaround for now
  sp_env_t* env = &s->env;
  spn_toolchain_unit_t* toolchain = s->units.target->toolchain;
  sp_env_init(s->mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.linker));
  if (spn_toolchain_has_cxx(toolchain->info)) {
    sp_env_insert(env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(s->mem, toolchain->cc.cxx));
  }

  return spn_task_done();
}
