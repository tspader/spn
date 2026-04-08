#include "spn.h"

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* target = spn_get_target(spn, "spn");
  const spn_t* tcc = spn_get_dep(spn, "spcc");
  const c8* dir = spn_get_subdir(tcc, SPN_DIR_LIB, "tcc");
  spn_target_embed_dir_ex(target, dir, "u8", "u64");
  spn_target_embed_file_ex(target, "include/spn.h", "include_spn_h", "u8", "u64");
  return SPN_OK;
}
