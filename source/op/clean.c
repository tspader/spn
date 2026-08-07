#include "ctx/types.h"
#include "error/error.h"
#include "project/types.h"
#include "session/types.h"

#include "spn/host.h"
#include "ctx/ctx.h"
#include "profile/profile.h"
#include "sp/os.h"

static spn_err_union_t remove_path(sp_str_t path) {
  if (sp_fs_remove(path) != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_REMOVE, .fs = { .path = path } };
  }

  return spn_result(SPN_OK);
}

static spn_err_union_t clean(spn_ctx_t* ctx) {
  spn_try_union(spn_ctx_require_project(ctx));
  return remove_path(sp_fs_join_path(ctx->heap, ctx->project->paths.root, sp_str_lit("build")));
}

spn_err_t spn_op_clean(spn_ctx_t* ctx) {
  return spn_err_emit(ctx, clean(ctx));
}

spn_err_t spn_op_clean_profile(spn_session_t* session) {
  sp_str_t path = spn_profile_build_path(session->mem, session->paths.build, &session->profile);
  return spn_err_emit(session->ctx, remove_path(path));
}
