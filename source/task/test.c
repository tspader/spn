#include "sp.h"
#include "sp/macro.h"
#include "spn.h"
#include "app/app.h"
#include "ctx/types.h"
#include "event/event.h"
#include "filter/filter.h"
#include "log/log.h"
#include "pkg/types.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/task.h"
#include "unit/types.h"

// @spader
// This file was put back together after the big refactor somewhat hastily. I
// don't know if it's exactly right; I just wanted to get it basically working
// and move on.

static spn_task_step_t spn_task_run_tests(spn_app_t* app);

static spn_task_step_t run_script(spn_app_t* app) {
  spn_session_t* session = &app->session;

  spn_pkg_unit_t* root = spn_session_find_root(session);
  if (!root) {
    spn_log_error("script {.yellow} was not built", SP_FMT_STR(app->config.run.target));
    return spn_task_fail(SPN_ERROR);
  }

  spn_target_unit_t* unit = spn_session_find_target_in_pkg(session, root, app->config.run.target);
  if (!unit) {
    spn_log_error("script {.yellow} was not built", SP_FMT_STR(app->config.run.target));
    return spn_task_fail(SPN_ERROR);
  }

  sp_str_t command = get_target_output_path(session->mem, unit);
  if (!sp_fs_exists(command)) {
    spn_log_error("script binary {.yellow} does not exist", SP_FMT_STR(command));
    return spn_task_fail(SPN_ERROR);
  }

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_RUN,
    .pkg = root->info,
    .run = {
      .name = unit->info->name,
      .command = command,
    }
  });
  spn_poll(spn.sp);

  sp_ps_t ps = sp_ps_create(app->session.mem, (sp_ps_config_t) {
    .command = command,
    .cwd = root->paths.source,
    .io = {
      .in =  { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_INHERIT },
      .err = { .mode = SP_PS_IO_MODE_INHERIT },
    },
  });
  sp_ps_status_t status = sp_ps_wait(&ps);

  if (status.exit_code) {
    spn_log_error("script {.yellow} failed with exit code {}",
      SP_FMT_STR(unit->info->name),
      SP_FMT_S32(status.exit_code)
    );
    return spn_task_fail(SPN_ERROR);
  }

  return spn_task_done();
}

static spn_task_step_t run_source(spn_app_t* app) {
  spn_log_error("{.yellow} cannot run native sources; build scripts are wasm", SP_FMT_STR(app->config.run.target));
  return spn_task_fail(SPN_ERROR);
}

spn_task_step_t spn_task_run(spn_app_t* app) {
  switch (app->config.run.kind) {
    case SPN_RUN_KIND_NONE: {
      return spn_task_done();
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

  SP_UNREACHABLE_RETURN(spn_task_fail(SPN_ERROR));
}

static spn_task_step_t spn_task_run_tests(spn_app_t* app) {
  return spn_task_done();
}
