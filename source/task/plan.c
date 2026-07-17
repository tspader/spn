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

spn_pkg_unit_t* add_package_units(spn_session_t* s, spn_build_unit_t* build, spn_pkg_id_t id, u32 kinds) {
  spn_resolved_pkg_t* pkg = sp_ht_getp(s->resolve, id);
  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
  sp_assert(pkg);
  sp_assert(loaded);

  spn_pkg_unit_t* unit = spn_session_find_pkg_unit(s, build, id);
  if (!unit) {
    unit = spn_session_add_pkg_unit(s, build, id, loaded);
  }

  u32 missing = kinds & ~unit->materialized_dep_kinds;
  unit->materialized_dep_kinds |= missing;
  sp_da_for(pkg->edges, it) {
    if (!(missing & spn_dep_kind_bit(pkg->edges[it].kind))) {
      continue;
    }
    sp_da_push(unit->deps, ((spn_pkg_dep_t) {
      .unit = add_package_units(s, build, pkg->edges[it].id, spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE)),
      .kind = pkg->edges[it].kind,
      .private = pkg->edges[it].private,
    }));
  }
  return unit;
}

static spn_err_union_t add_compilation_units(spn_session_t *s) {
  spn_pkg_id_t root = spn_session_root_pkg(s);
  sp_da_for(s->plan.builds, it) {
    spn_build_plan_t* plan = &s->plan.builds[it];
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_toolchain_t* compiler = &plan->build->toolchain->cc;
    spn_cc_flags_t flags = sp_zero;
    spn_err_union_t err = spn_cc_render_flags(scratch.mem, compiler, &plan->build->profile, &flags);
    sp_mem_end_scratch(scratch);
    if (err.kind) {
      return err;
    }
    add_package_units(s, plan->build, root, plan->build->dep_kinds);
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_plan(spn_app_t* app) {
  spn_session_t* s = &app->session;

  try_task(add_compilation_units(s));
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
