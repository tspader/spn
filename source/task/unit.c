#include "sp.h"
#include "app/types.h"
#include "ctx/types.h"
#include "session/invocation.h"
#include "session/session.h"
#include "task/task.h"
#include "unit/unit.h"

spn_task_step_t spn_task_create_units(spn_ctx_t* ctx) {
  spn_app_t* app = ctx->app;
  spn_session_t* session = &app->session;

  spn_try_step(spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET));
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));

  return spn_task_done();
}
