#include "ctx/types.h"
#include "session/types.h"

#include "op/op.h"
#include "profile/profile.h"
#include "sp/os.h"

spn_err_union_t spn_op_clean(spn_session_t* session, bool whole_build) {
  sp_str_t path = whole_build ?
    session->paths.build :
    spn_profile_build_path(session->mem, session->paths.build, &session->profile);

  if (sp_fs_remove(path) != SP_OK) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_REMOVE, .fs = { .path = path } };
  }

  return spn_result(SPN_OK);
}
