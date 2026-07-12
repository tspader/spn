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

static spn_pkg_unit_t* add_package_units(spn_session_t* s, spn_build_unit_t* build, spn_pkg_id_t id) {
  spn_pkg_unit_t* existing = spn_session_find_pkg_unit(s, build, id);
  if (existing) {
    return existing;
  }

  spn_resolved_pkg_t* pkg = sp_ht_getp(s->resolve, id);
  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
  sp_assert(pkg);
  sp_assert(loaded);

  spn_pkg_unit_t* unit = spn_session_add_pkg_unit(s, build, id, loaded);
  sp_da_for(pkg->edges, it) {
    sp_da_push(unit->deps, ((spn_pkg_dep_t) {
      .unit = add_package_units(s, build, pkg->edges[it].id),
      .kind = pkg->edges[it].kind,
      .private = pkg->edges[it].private,
    }));
  }
  return unit;
}

static void add_target_build(spn_session_t* s, spn_profile_info_t profile) {
  spn_build_unit_t* unit = sp_alloc_type(s->mem, spn_build_unit_t);
  *unit = (spn_build_unit_t) {
    .id = (spn_build_unit_id_t)sp_da_size(s->plan.builds),
    .profile = profile,
    .toolchain = s->units.toolchain,
    .visibility = SPN_SYMBOL_VISIBILITY_DEFAULT,
    .dep_kinds = spn_dep_kind_bit(SPN_DEP_KIND_PACKAGE) | spn_dep_kind_bit(SPN_DEP_KIND_TEST),
    .paths = { .profile = s->paths.profile },
  };
  sp_da_init(s->mem, unit->include);

  spn_build_plan_t build = {
    .build = unit,
    .selection = s->plan.request.targets,
  };
  sp_da_init(s->mem, build.roots);
  sp_da_push(s->plan.builds, build);
}

static spn_err_union_t add_compilation_units(spn_session_t *s) {
  add_target_build(s, s->profile);

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
      add_package_units(s, plan->build, s->plan.requested[rt]);
    }
  }
  return spn_result(SPN_OK);
}

spn_task_step_t spn_task_plan(spn_app_t* app) {
  spn_session_t* session = &app->session;

  session->units.toolchains = sp_da_new(session->mem, spn_toolchain_unit_t*);
  sp_da_init(session->mem, session->units.compile_commands);
  sp_da_init(session->mem, session->plan.builds);
  session->plan.script = SP_NULLPTR;

  sp_da_init(session->mem, session->plan.requested);
  sp_ht_for_kv(session->resolve, it) {
    if (it.val->source == SPN_PKG_SOURCE_ROOT) {
      sp_da_push(session->plan.requested, it.val->id);
    }
  }
  sp_assert(!sp_da_empty(session->plan.requested));
  sp_da_for(app->sync.toolchains, it) {
    spn_sync_toolchain_job_t* job = app->sync.toolchains[it];
    sp_da_push(session->units.toolchains, job->unit);
    if (job->toolchain == session->selection.build) {
      session->units.toolchain = job->unit;
    }
    if (job->toolchain == session->selection.script) {
      session->units.script = job->unit;
    }
  }

  spn_err_union_t err = add_compilation_units(session);
  if (!err.kind) {
    err = add_script_units(session);
  }
  if (err.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = err,
    });
    return spn_task_fail(SPN_ERROR);
  }

  sp_env_t* env = &session->env;
  sp_env_init(session->mem, env);
  sp_env_insert(env, sp_str_lit("CC"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->compiler));
  sp_env_insert(env, sp_str_lit("AR"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->archiver));
  sp_env_insert(env, sp_str_lit("LD"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->linker));
  if (spn_toolchain_has_cxx(session->units.toolchain->toolchain)) {
    sp_env_insert(env, sp_str_lit("CXX"), spn_toolchain_launcher_to_str(session->mem, session->units.toolchain->cxx));
  }

  return spn_task_done();
}
