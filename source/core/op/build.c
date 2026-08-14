#include "spn/host.h"

#include "error/error.h"
#include "graph/dag.h"
#include "model/model.h"
#include "op/op.h"
#include "session/invocation.h"
#include "session/types.h"

spn_err_t spn_op_build(spn_op_t* op) {
  spn_session_t* session = op->session;
  spn_try(spn_model_establish(op));
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
  return spn_dag_build_session(op);
}

spn_op_t* spn_build(spn_session_t* session) {
  spn_op_t* op = spn_op_new(session->ctx, session, SPN_OP_BUILD);
  spn_op_submit(op);
  return op;
}
