#include "sp.h"
#include "sp/macro.h"
#include "spn.h"
#include "app/app.h"
#include "ctx/types.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/tcc/tcc.h"
#include "filter/filter.h"
#include "log/log.h"
#include "pkg/types.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/task.h"
#include "unit/types.h"

#include "libtcc.h"

// @spader
// This file was put back together after the big refactor somewhat hastily. I
// don't know if it's exactly right; I just wanted to get it basically working
// and move on.

static bool run_path_is_absolute(sp_str_t path) {
  return sp_str_starts_with(path, sp_str_lit("/")) ||
    sp_str_starts_with(path, sp_str_lit("\\")) ||
    (path.len > 1 && path.data[1] == ':');
}

static sp_str_t run_resolve_source_path(sp_str_t path) {
  if (run_path_is_absolute(path) && sp_fs_exists(path)) {
    return sp_fs_canonicalize_path(spn.mem, path);
  }

  sp_str_t project_path = sp_fs_join_path(spn.mem, spn.paths.project, path);
  if (sp_fs_exists(project_path)) {
    return sp_fs_canonicalize_path(spn.mem, project_path);
  }

  return sp_str_lit("");
}

static spn_task_result_t run_script(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_pkg_unit_t* root = spn_session_find_root(session);
  if (!root) {
    spn_log_error("script {.yellow} was not built", SP_FMT_STR(app->config.run.target));
    return SPN_TASK_ERROR;
  }

  spn_target_unit_t* unit = spn_session_find_target_in_pkg(session, root, app->config.run.target);
  if (!unit) {
    spn_log_error("script {.yellow} was not built", SP_FMT_STR(app->config.run.target));
    return SPN_TASK_ERROR;
  }

  sp_str_t command = get_target_output_path(session->mem, unit);
  if (!sp_fs_exists(command)) {
    spn_log_error("script binary {.yellow} does not exist", SP_FMT_STR(command));
    return SPN_TASK_ERROR;
  }

  sp_ps_output_t result = sp_ps_run(app->session.mem, (sp_ps_config_t) {
    .command = command,
    .cwd = root->paths.source,
    .io = {
      .in =  { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_INHERIT },
      .err = { .mode = SP_PS_IO_MODE_INHERIT },
    },
  });

  if (result.status.exit_code) {
    spn_log_error("script {.yellow} failed with exit code {}",
      SP_FMT_STR(unit->info->name),
      SP_FMT_S32(result.status.exit_code)
    );
    return SPN_TASK_ERROR;
  }

  return SPN_TASK_DONE;
}

static void add_build_deps(spn_app_t* app, spn_cc_target_t* target) {
  spn_session_t* session = &app->session;
  if (!session->pkg) return;

  sp_ht_for_kv(session->pkg->deps, it) {
    spn_requested_pkg_t* dep = it.val;
    if (dep->kind != SPN_DEP_KIND_BUILD) continue;

    spn_pkg_unit_t* unit = spn_session_find_pkg_by_qualified(session, dep->qualified);
    if (!unit) continue;

    spn_cc_target_add_absolute_include(target, unit->paths.include);
    spn_cc_target_add_absolute_include(target, unit->paths.source);
  }
}

static spn_task_result_t run_source(spn_app_t* app) {
  sp_str_t path = run_resolve_source_path(app->config.run.target);
  if (sp_str_empty(path) || !sp_fs_exists(path)) {
    spn_log_error("source file {.yellow} does not exist", SP_FMT_STR(app->config.run.target));
    return SPN_TASK_ERROR;
  }

  spn_cc_t cc;
  spn_cc_init(&cc, app->session.mem);
  spn_cc_add_runtime(&cc, spn.paths.runtime, spn.paths.include);
  spn_cc_set_profile(&cc, app->session.profile);

  spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_OUTPUT_JIT, sp_str_lit("run"));
  add_build_deps(app, target);
  spn_cc_target_add_absolute_source(target, path);

  spn_tcc_t* tcc = sp_alloc_type(app->session.mem, spn_tcc_t);
  spn_tcc_init(app->session.mem, tcc);
  if (spn_cc_target_to_tcc(&cc, target, tcc)) {
    spn_log_error("failed to compile {.yellow}", SP_FMT_STR(path));
    return SPN_TASK_ERROR;
  }

  c8* argv[] = {
    (c8*)sp_str_to_cstr(app->session.mem, path),
    SP_NULLPTR,
  };

  sp_str_t cwd = sp_fs_get_cwd(app->session.mem);
  if (sp_sys_chdir_s(spn.paths.project)) {
    spn_log_error("failed to change directory to {.yellow}", SP_FMT_STR(spn.paths.project));
    return SPN_TASK_ERROR;
  }

  s32 result = tcc_run(tcc->s, 1, argv);
  sp_sys_chdir_s(cwd);

  if (result) {
    spn_log_error("source run failed for {.yellow} with exit code {}",
      SP_FMT_STR(path),
      SP_FMT_S32(result)
    );
    return SPN_TASK_ERROR;
  }

  return SPN_TASK_DONE;
}

spn_task_result_t spn_task_run(spn_app_t* app) {
  switch (app->config.run.kind) {
    case SPN_RUN_KIND_NONE: {
      return SPN_TASK_DONE;
    }
    case SPN_RUN_KIND_TESTS: {
      return spn_task_run_tests(app);
    }
    case SPN_RUN_KIND_SCRIPT: {
      return run_script(app);
    }
    case SPN_RUN_KIND_SOURCE: {
      return run_source(app);
    }
  }

  SP_UNREACHABLE_RETURN(SPN_TASK_ERROR);
}

spn_task_result_t spn_task_run_tests(spn_app_t* app) {
  return SPN_TASK_DONE;
}
