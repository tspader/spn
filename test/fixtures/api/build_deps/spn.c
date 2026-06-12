#include "spn.h"

#include "spum.h"

s32 generate_build_dep_value(spn_t* spn, spn_node_ctx_t* ctx) {
  s32 value = SPUM_MAGIC + 1;
  if (value != 78) {
    spn_log(spn, "unexpected spum value");
    return 1;
  }

  spn_write_file(spn, "build_dep_value.h",
    "#ifndef BUILD_DEP_VALUE_H\n"
    "#define BUILD_DEP_VALUE_H\n"
    "#define BUILD_DEP_VALUE 78\n"
    "#endif\n"
  );

  return 0;
}

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node(config, "generate_build_dep_value");
  spn_node_set_fn(gen, generate_build_dep_value);
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "build_dep_value.h"));
  return SPN_OK;
}
