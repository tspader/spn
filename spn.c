#include "spn.h"

void build(spn_build_ctx_t* b) {
  spn_target_t* spn = spn_get_target(b, "spn");
  const spn_build_ctx_t* spcc = spn_get_dep(b, "spcc");
  const c8* dir = spn_get_subdir(spcc, SPN_DIR_LIB, "tcc");
  spn_log(b, dir);
  spn_target_embed_dir_ex(spn, dir, "u8", "u64");
}
