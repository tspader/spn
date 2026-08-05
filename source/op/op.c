#include "op/op.h"

#include "op/build/dag.h"
#include "op/stage.h"
#include "session/invocation.h"
#include "session/types.h"
#include "unit/unit.h"

spn_err_union_t spn_op_build(spn_session_t* session) {
  try_union(spn_op_sync_indexes(session->ctx, sp_zero_s(spn_index_refresh_t)));
  bool reresolve = sp_zero;
  do {
    try_union(spn_op_resolve(session));
    try_union(spn_op_sync(session, &reresolve));
  } while (reresolve);
  try_union(spn_op_configure(session));
  try_union(spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET));
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
  return spn_dag_build_session(session);
}
