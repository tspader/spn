#include "error/error.h"
#include "op/op.h"

#include "op/build/build.h"
#include "session/types.h"
#include "unit/types.h"

static spn_err_union_t run_target(spn_session_t* session, spn_target_unit_t* unit) {
  sp_str_t command = spn_target_staged_path(session->mem, unit);
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

spn_err_t spn_op_run_target(spn_session_t* session, spn_target_unit_t* unit) {
  return spn_err_emit(session->ctx, run_target(session, unit));
}
