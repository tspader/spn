#include "ctx/types.h"
#include "error/error.h"
#include "project/types.h"
#include "session/types.h"

#include "spn/host.h"
#include "ctx/ctx.h"
#include "op/op.h"
#include "profile/profile.h"
#include "sp/os.h"

static spn_err_t remove_path(spn_ctx_t* ctx, sp_str_t path) {
  if (sp_fs_remove(path) != SP_OK) {
    return spn_err_emit(ctx, (spn_err_union_t) { .kind = SPN_ERR_FS_REMOVE, .fs = { .path = path } });
  }

  return SPN_OK;
}

spn_err_t spn_op_clean(spn_op_t* op) {
  spn_ctx_t* ctx = op->ctx;
  spn_try(spn_ctx_require_project(ctx));
  return remove_path(ctx, sp_fs_join_path(ctx->heap, ctx->project->paths.root, sp_str_lit("build")));
}

spn_op_t* spn_clean(spn_ctx_t* ctx) {
  spn_op_t* op = spn_op_new(ctx, SP_NULLPTR, SPN_OP_CLEAN);
  spn_op_submit(op);
  return op;
}

spn_err_t spn_op_clean_profile(spn_op_t* op) {
  spn_session_t* session = op->session;
  sp_str_t path = spn_profile_build_path(session->mem, session->paths.build, &session->profile);
  return remove_path(session->ctx, path);
}

spn_op_t* spn_clean_profile(spn_session_t* session) {
  spn_op_t* op = spn_op_new(session->ctx, session, SPN_OP_CLEAN_PROFILE);
  spn_op_submit(op);
  return op;
}
