#include "error/error.h"
#include "spn/host.h"

#include "ctx/types.h"
#include "event/event.h"
#include "op/build/build.h"
#include "session/session.h"
#include "session/types.h"
#include "unit/types.h"

static spn_err_union_t run_target(spn_session_t* session, spn_target_unit_t* unit) {
  sp_str_t command = spn_target_unit_staged_path(session->mem, unit);
  if (!sp_fs_exists(command)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SCRIPT_MISSING,
      .script = { .name = unit->info->name, .path = command },
    };
  }

  sp_ps_t ps = sp_ps_create(session->mem, (sp_ps_config_t) {
    .command = command,
    .cwd = unit->pkg->paths.source,
    .io = {
      .in =  { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_INHERIT },
      .err = { .mode = SP_PS_IO_MODE_INHERIT },
    },
  });
  sp_ps_status_t status = sp_ps_wait(&ps);

  if (status.exit_code) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_SCRIPT_FAILED,
      .script = { .name = unit->info->name, .code = status.exit_code },
    };
  }

  return spn_result(SPN_OK);
}

spn_err_t spn_op_run_target(spn_session_t* session, spn_target_t* target) {
  return spn_err_emit(session->ctx, run_target(session, sp_ptr_cast(spn_target_unit_t*, target)));
}

static spn_err_union_t run_test(spn_session_t* session, spn_target_unit_t* unit, bool* passed) {
  spn_ctx_t* ctx = session->ctx;
  sp_str_t command = spn_target_unit_staged_path(session->mem, unit);

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_TARGET_RUN,
    .pkg = unit->pkg->info,
    .run = {
      .name = unit->info->name,
      .command = command,
    },
  });

  if (!sp_fs_exists(command)) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_TEST_MISSING,
      .script = { .name = unit->info->name, .path = command },
    };
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t output = sp_ps_run(session->mem, (sp_ps_config_t) {
    .command = command,
    .cwd = unit->pkg->paths.source,
    .io = {
      .in =  { .mode = SP_PS_IO_MODE_NULL },
      .out = { .mode = SP_PS_IO_MODE_CREATE },
      .err = { .mode = SP_PS_IO_MODE_CREATE },
    },
  });
  u64 time = sp_tm_read_timer(&timer);

  *passed = output.status.exit_code == 0;
  if (*passed) {
    spn_event_buffer_push(ctx->events, (spn_build_event_t) {
      .kind = SPN_EVENT_TEST_PASSED,
      .test_passed = {
        .name = unit->info->name,
        .time = time,
      },
    });
  }
  else {
    spn_event_buffer_push(ctx->events, (spn_build_event_t) {
      .kind = SPN_EVENT_TEST_FAILED,
      .test_failed = {
        .name = unit->info->name,
        .code = output.status.exit_code,
        .out = output.out,
        .err = output.err,
        .time = time,
      },
    });
  }

  return spn_result(SPN_OK);
}

spn_err_t spn_op_test(spn_session_t* session) {
  spn_ctx_t* ctx = session->ctx;
  sp_tm_timer_t timer = sp_tm_start_timer();

  u32 passed = 0;
  u32 failed = 0;

  sp_da_for(session->plans, it) {
    spn_build_plan_t* plan = &session->plans[it];
    sp_da_for(plan->roots, jt) {
      spn_target_unit_t* unit = spn_session_get_target_unit(session, plan->roots[jt]);
      if (unit->info->kind != SPN_TARGET_KIND_TEST) {
        continue;
      }
      bool test_passed = sp_zero;
      spn_try(spn_err_emit(ctx, run_test(session, unit, &test_passed)));
      if (test_passed) {
        passed++;
      }
      else {
        failed++;
      }
    }
  }

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_TEST_SUMMARY,
    .test_summary = {
      .passed = passed,
      .failed = failed,
      .time = sp_tm_read_timer(&timer),
    },
  });

  if (failed) {
    return spn_err_emit(ctx, spn_err_reported(SPN_ERR_TEST_FAILED));
  }
  return SPN_OK;
}
