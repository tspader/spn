#include "spn.h"

s32 generate_header(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "generating version.h...");
  spn_write_file(spn, "version.h",
    "#ifndef VERSION_H\n"
    "#define VERSION_H\n"
    "#define VERSION_MAJOR 69\n"
    "#define VERSION_MINOR 2\n"
    "#define VERSION_PATCH 3\n"
    "#endif\n"
  );
  return 0;
}

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node(config, "gen_version");
  spn_node_set_fn(gen, generate_header);

  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "version.h"));
  return SPN_OK;
}

spn_err_t package(spn_t* spn) {
  spn_copy(spn, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "");
  return SPN_OK;
}
