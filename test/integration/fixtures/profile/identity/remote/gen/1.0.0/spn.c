#include "spn.h"

SPN_EXPORT
s32 gen_header(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_profile_t* profile = spn_get_profile(spn);
  const c8* content = "#define GEN_VALUE 1\n";
  if (spn_profile_get_opt(profile) == SPN_OPT_LEVEL_2) {
    content = "#define GEN_VALUE 2\n";
  }
  if (spn_profile_get_sanitizers(profile) & SPN_SANITIZER_ADDRESS) {
    content = "#define GEN_VALUE 3\n";
  }
  spn_io_write("/store/include/gen.h", content);
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* node = spn_add_node(config, "gen_header");
  spn_node_set_fn(node, "gen_header");
  spn_node_add_output(node, spn_get_subdir(spn, SPN_DIR_STORE, "include/gen.h"));
  return SPN_OK;
}
