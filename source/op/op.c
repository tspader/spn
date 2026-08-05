#include "op/op.h"

#include "app/types.h"
#include "ctx/types.h"
#include "session/invocation.h"
#include "session/types.h"
#include "unit/unit.h"

spn_err_union_t spn_op_reach(spn_session_t* session, spn_phase_t target);
spn_err_union_t spn_op_add(spn_ctx_t* ctx, spn_add_request_t* request);
spn_err_union_t spn_op_clean(spn_session_t* session, bool whole_build);
spn_err_union_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t* request);
spn_err_union_t spn_op_sync_indexes(spn_ctx_t* ctx, spn_index_refresh_t refresh);
spn_err_union_t spn_phase_resolve(spn_session_t* session);
spn_err_union_t spn_phase_sync(spn_session_t* session);
spn_err_union_t spn_phase_build(spn_session_t* session);
spn_err_union_t spn_phase_configure(spn_session_t* s);

spn_err_union_t spn_op_exec(spn_ctx_t* ctx, spn_op_desc_t* desc) {
  switch (desc->kind) {
    case SPN_OP_REACH:        return spn_op_reach(&ctx->app->session, desc->reach);
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

spn_err_union_t spn_op_reach(spn_session_t* session, spn_phase_t target) {
  sp_assert(session->pkg);
  //
  // try_union(spn_op_sync_indexes(session->ctx, sp_zero_s(spn_index_refresh_t)));
  // do {
  //   session->gates.reresolve = false;
  //   try_union(spn_phase_resolve(session));
  //   try_union(spn_phase_sync(session));
  // } while (session->gates.reresolve);
  // try_union(spn_phase_configure(session));
  // try_union(spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET));
  // spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
  // try_union(spn_phase_build(session));
  //
  while (session->phase < target) {
    if (sp_atomic_s32_get(&session->ctx->aborted)) {
      return spn_err_reported(SPN_ERROR);
    }

    switch (session->phase + 1) {
      case SPN_PHASE_PACKAGES: {
        try_union(spn_op_sync_indexes(session->ctx, sp_zero_s(spn_index_refresh_t)));
        do {
          session->gates.reresolve = false;
          try_union(spn_phase_resolve(session));
          try_union(spn_phase_sync(session));
        } while (session->gates.reresolve);
        break;
      }
      case SPN_PHASE_CONFIGURED: {
        try_union(spn_phase_configure(session));
        break;
      }
      case SPN_PHASE_UNITS: {
        try_union(spn_units_add_targets(session, SPN_UNIT_SCOPE_TARGET));
        spn_session_write_compile_commands(session, spn_session_compile_commands_path(session));
        break;
      }
      case SPN_PHASE_BUILT: {
        try_union(spn_phase_build(session));
        break;
      }
      case SPN_PHASE_NONE: {
        sp_unreachable_case();
      }
    }

    session->phase++;
  }
  return spn_result(SPN_OK);
}
