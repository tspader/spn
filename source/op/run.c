#include "error/error.h"
#include "spn/host.h"

#include "op/build/build.h"
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

static spn_err_union_t run_test(spn_session_t* session, spn_target_unit_t* unit, spn_test_run_t* run) {
  sp_str_t command = spn_target_unit_staged_path(session->mem, unit);
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

  *run = (spn_test_run_t) {
    .code = output.status.exit_code,
    .out = output.out,
    .err = output.err,
    .time = sp_tm_read_timer(&timer),
  };
  return spn_result(SPN_OK);
}

spn_err_t spn_op_run_test(spn_session_t* session, spn_target_t* target, spn_test_run_t* run) {
  return spn_err_emit(session->ctx, run_test(session, sp_ptr_cast(spn_target_unit_t*, target), run));
}
