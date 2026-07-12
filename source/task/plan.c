#include "sp.h"
#include "app/types.h"
#include "compiler/driver.h"
#include "ctx/types.h"
#include "error/types.h"
#include "event/event.h"
#include "log/log.h"
#include "pkg/id.h"
#include "pkg/types.h"
#include "profile/profile.h"
#include "resolve/types.h"
#include "session/session.h"
#include "session/types.h"
#include "spn.h"
#include "task/task.h"
#include "task/types.h"
#include "toolchain/toolchain.h"
#include "toolchain/types.h"
#include "triple/triple.h"
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

static spn_build_unit_t* add_build_unit(spn_session_t* s) {
  spn_build_unit_t* unit = sp_alloc_type(s->mem, spn_build_unit_t);
  unit->id = (spn_build_unit_id_t)sp_da_size(s->units.builds);
  sp_da_push(s->units.builds, unit);
  return unit;
}

static spn_build_unit_t* add_target_build(spn_session_t* s, spn_profile_info_t profile) {
  spn_build_unit_t* unit = add_build_unit(s);
  spn_build_unit_id_t id = unit->id;
  *unit = (spn_build_unit_t) {
    .id = id,
    .profile = profile,
    .toolchain = s->units.toolchain,
    .visibility = SPN_SYMBOL_VISIBILITY_DEFAULT,
    .dep_kinds = spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_TEST),
    .paths = {
      .root = spn_profile_build_path(s->mem, s->paths.build, &profile)
    },
  };
  sp_da_init(s->mem, unit->include);
  sp_da_init(s->mem, unit->packages);

  spn_build_plan_t build = {
    .build = unit,
    .selection = s->plan.request.targets,
  };
  sp_da_init(s->mem, build.roots);
  sp_da_push(s->plan.builds, build);
  return unit;
}

static spn_build_unit_t* add_program_build(spn_session_t* s) {
  spn_build_unit_t* unit = add_build_unit(s);
  spn_build_unit_id_t id = unit->id;
  spn_triple_t target = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_NONE };
  *unit = (spn_build_unit_t) {
    .id = id,
    .profile = {
      .name = sp_str_lit("program"),
      .arch = target.arch,
      .os = target.os,
      .abi = target.abi,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_2,
      .standard = SPN_C99,
      .linkage = SPN_LIB_KIND_STATIC,
    },
    .toolchain = s->units.script,
    .visibility = SPN_SYMBOL_VISIBILITY_HIDDEN,
    .dep_kinds = spn_dep_kind_bit(SPN_DEP_KIND_BUILD),
    .paths = {
      .root = sp_fs_join_path(s->mem, s->paths.build, spn_triple_to_str(s->mem, target)),
    },
  };
  sp_da_init(s->mem, unit->include);
  sp_da_init(s->mem, unit->packages);
  sp_da_push(unit->include, spn.paths.include);
  return unit;
}

static bool find_resolved_pkg(spn_session_t* s, sp_str_t qualified, spn_pkg_id_t* id) {
  spn_pkg_id_t probe = spn_pkg_id(s->intern, qualified);
  sp_ht_for_kv(s->resolve, it) {
    if (it.val->id.qualified == probe.qualified) {
      *id = it.val->id;
      return true;
    }
  }
  return false;
}

static spn_err_t collect_requested_pkgs(spn_session_t* session) {
  sp_da_init(session->mem, session->plan.requested);

  if (sp_da_empty(session->plan.request.packages)) {
    spn_pkg_id_t id = sp_zero;
    sp_assert(find_resolved_pkg(session, session->pkg->qualified, &id));
    sp_da_push(session->plan.requested, id);
    return SPN_OK;
  }

  sp_da_for(session->plan.request.packages, it) {
    sp_str_t requested = session->plan.request.packages[it];
    spn_pkg_id_t id = sp_zero;
    if (!find_resolved_pkg(session, spn_pkg_canonicalize_name(requested), &id)) {
      spn_log_error("requested package {.cyan} is not in the dependency graph", SP_FMT_STR(requested));
      return SPN_ERROR;
    }
    sp_da_push(session->plan.requested, id);
  }
  return SPN_OK;
}

static spn_err_union_t add_compilation_units(spn_session_t *s) {
  sp_da_for(s->plan.builds, it) {
    spn_build_plan_t* plan = &s->plan.builds[it];
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    spn_cc_flags_t flags = SP_ZERO_INITIALIZE();
    spn_err_union_t err = spn_cc_render_flags(scratch.mem, plan->build->toolchain->toolchain->driver, &plan->build->profile, &flags);
    sp_mem_end_scratch(scratch);
    if (err.kind) {
      return err;
    }
    sp_da_for(s->plan.requested, rt) {
      add_package_units(s, plan->build, s->plan.requested[rt], plan->build->dep_kinds);
    }
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_plan(spn_app_t* app) {
  spn_session_t* s = &app->session;

  s->units.toolchains = sp_da_new(s->mem, spn_toolchain_unit_t*);
  s->units.builds = sp_da_new(s->mem, spn_build_unit_t*);
  sp_da_init(s->mem, s->units.compile_commands);
  sp_da_init(s->mem, s->plan.builds);

  if (collect_requested_pkgs(s)) {
    return spn_task_fail(SPN_ERROR);
  }
  sp_da_for(app->sync.toolchains, it) {
    spn_sync_toolchain_job_t* job = app->sync.toolchains[it];
    sp_da_push(s->units.toolchains, job->unit);
    if (job->toolchain == s->selection.build) {
      s->units.toolchain = job->unit;
    }
    if (job->toolchain == s->selection.script) {
      s->units.script = job->unit;
    }
  }

  s->units.target = add_target_build(s, s->profile);
  s->units.metaprogram = add_program_build(s);
  try_task(add_compilation_units(s));
  try_task(add_program_units(s));

  sp_env_t* env = &s->env;
  sp_env_init(s->mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(s->mem, s->units.toolchain->compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(s->mem, s->units.toolchain->archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(s->mem, s->units.toolchain->linker));
  if (spn_toolchain_has_cxx(s->units.toolchain->toolchain)) {
    sp_env_insert(env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(s->mem, s->units.toolchain->cxx));
  }

  return spn_task_done();
}
