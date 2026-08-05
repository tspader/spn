#include "op/op.h"

#include "app/types.h"
#include "ctx/types.h"
#include "session/invocation.h"
#include "session/types.h"
#include "unit/unit.h"

spn_err_union_t spn_op_resolve(spn_session_t* session);
spn_err_union_t spn_op_sync(spn_session_t* session, bool* reresolve);
spn_err_union_t spn_op_configure(spn_session_t* session);
spn_err_union_t spn_op_build(spn_session_t* session);

static spn_err_union_t prepare(spn_session_t* session) {
  sp_assert(session->pkg);

  try_union(spn_op_sync_indexes(session->ctx, sp_zero_s(spn_index_refresh_t)));
  bool reresolve = sp_zero;
  do {
    try_union(spn_op_resolve(session));
    try_union(spn_op_sync(session, &reresolve));
  } while (reresolve);
  try_union(spn_op_configure(session));
  try_union(spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET));
  spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
  return spn_result(SPN_OK);
}

spn_err_union_t spn_op_exec(spn_ctx_t* ctx, spn_op_desc_t* desc) {
  switch (desc->kind) {
    case SPN_OP_BUILD: {
      try_union(prepare(&ctx->app->session));
      return spn_op_build(&ctx->app->session);
    }
    case SPN_OP_ADD:          return spn_op_add(ctx, &desc->add);
    case SPN_OP_CLEAN:        return spn_op_clean(&ctx->app->session, desc->clean.whole);
    case SPN_OP_PUBLISH:      return spn_op_publish(ctx, &desc->publish);
    case SPN_OP_SYNC_INDEXES: return spn_op_sync_indexes(ctx, desc->refresh);
    case SPN_OP_NONE:         break;
  }
  sp_unreachable_return(spn_result(SPN_ERROR));
}

static s32 op_main(void* data) {
  spn_op_t* op = (spn_op_t*)data;
  op->result = spn_op_exec(op->ctx, &op->desc);
  sp_atomic_s32_set(&op->done, 1);
  return 0;
}

spn_op_t* spn_op_start(sp_mem_t mem, spn_ctx_t* ctx, spn_op_desc_t desc) {
  spn_op_t* op = sp_alloc_type(mem, spn_op_t);
  op->ctx = ctx;
  op->desc = desc;
  sp_thread_init(&op->thread, op_main, op);
  return op;
}

bool spn_op_poll(spn_op_t* op) {
  return sp_atomic_s32_get(&op->done) != 0;
}

spn_err_union_t spn_op_result(spn_op_t* op) {
  sp_thread_join(&op->thread);
  return op->result;
}
