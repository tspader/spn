#include "spn.h"

SPN_EXPORT
s32 phase1_fn(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "phase1: generating intermediate.h...");
  spn_io_write("/work/intermediate.h",
    "#ifndef INTERMEDIATE_H\n"
    "#define INTERMEDIATE_H\n"
    "#define PHASE1_VALUE 100\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
s32 phase2_fn(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "phase2: generating final.h...");
  spn_io_write("/work/final.h",
    "#ifndef FINAL_H\n"
    "#define FINAL_H\n"
    "#include \"intermediate.h\"\n"
    "#define PHASE2_VALUE (PHASE1_VALUE + 50)\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  const c8* intermediate = spn_get_subdir(spn, SPN_DIR_WORK, "intermediate.h");
  const c8* final = spn_get_subdir(spn, SPN_DIR_WORK, "final.h");

  spn_node_t* phase1 = spn_add_node(config, "phase1");
  spn_node_set_fn(phase1, "phase1_fn");
  spn_node_add_output(phase1, intermediate);

  spn_node_t* phase2 = spn_add_node(config, "phase2");
  spn_node_set_fn(phase2, "phase2_fn");
  spn_node_add_input(phase2, intermediate);
  spn_node_add_output(phase2, final);
  return SPN_OK;
}
