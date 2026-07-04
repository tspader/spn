#include "spn.h"

SPN_EXPORT
s32 gen_all_headers(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_all_headers: creating 3 headers from single node");

  spn_io_write("/work/types.h",
    "#ifndef TYPES_H\n"
    "#define TYPES_H\n"
    "typedef int my_int_t;\n"
    "typedef float my_float_t;\n"
    "#endif\n"
  );

  spn_io_write("/work/constants.h",
    "#ifndef CONSTANTS_H\n"
    "#define CONSTANTS_H\n"
    "#define MAX_SIZE 1024\n"
    "#define MIN_SIZE 16\n"
    "#endif\n"
  );

  spn_io_write("/work/macros.h",
    "#ifndef MACROS_H\n"
    "#define MACROS_H\n"
    "#define SQUARE(x) ((x) * (x))\n"
    "#define CUBE(x) ((x) * (x) * (x))\n"
    "#endif\n"
  );

  return 0;
}

SPN_EXPORT
s32 gen_combined(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_combined: aggregating all headers");
  spn_io_write("/work/all.h",
    "#ifndef ALL_H\n"
    "#define ALL_H\n"
    "#include \"types.h\"\n"
    "#include \"constants.h\"\n"
    "#include \"macros.h\"\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  const c8* types_h = spn_get_subdir(spn, SPN_DIR_WORK, "types.h");
  const c8* constants_h = spn_get_subdir(spn, SPN_DIR_WORK, "constants.h");
  const c8* macros_h = spn_get_subdir(spn, SPN_DIR_WORK, "macros.h");
  const c8* all_h = spn_get_subdir(spn, SPN_DIR_WORK, "all.h");

  spn_node_t* gen = spn_add_node(config, "gen_all_headers");
  spn_node_set_fn(gen, "gen_all_headers");
  spn_node_add_output(gen, types_h);
  spn_node_add_output(gen, constants_h);
  spn_node_add_output(gen, macros_h);

  spn_node_t* combined = spn_add_node(config, "gen_combined");
  spn_node_set_fn(combined, "gen_combined");
  spn_node_add_input(combined, types_h);
  spn_node_add_input(combined, constants_h);
  spn_node_add_input(combined, macros_h);
  spn_node_add_output(combined, all_h);
  return SPN_OK;
}
