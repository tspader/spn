#include "ctx/types.h"
#include "error/error.h"
#include "session/types.h"

#include "op/op.h"
#include "profile/profile.h"
#include "sp/os.h"

static spn_err_union_t clean(spn_session_t* session, bool whole_build) {
  sp_str_t path = whole_build ?
    session->paths.build :
    spn_profile_build_path(session->mem, session->paths.build, &session->profile);

  if (sp_fs_remove(path) != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_REMOVE, .fs = { .path = path } };
  }

  return spn_result(SPN_OK);
}

spn_err_t spn_op_clean(spn_session_t* session, bool whole_build) {
  return spn_err_emit(session->ctx, clean(session, whole_build));
}
