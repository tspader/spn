#include "spn/host.h"

#include "error/error.h"
#include "op/build/dag.h"
#include "op/stage.h"
#include "session/invocation.h"
#include "session/types.h"
#include "unit/unit.h"

spn_err_t spn_op_build(spn_session_t* session) {
  spn_ctx_t* ctx = session->ctx;
  spn_try(spn_op_sync_indexes(ctx, sp_zero_s(spn_sync_request_t)));
  bool reresolve = sp_zero;
  do {
    spn_try(spn_err_emit(ctx, spn_op_resolve(session)));
    spn_try(spn_err_emit(ctx, spn_op_sync(session, &reresolve)));
  } while (reresolve);
  spn_try(spn_err_emit(ctx, spn_op_configure(session)));
  spn_try(spn_err_emit(ctx, spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET)));
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
  return spn_err_emit(ctx, spn_dag_build_session(session));
}
