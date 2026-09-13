#include "spn.h"

SPN_EXPORT
s32 gen_source(spn_t* spn) {
  spn_io_write("/work/generated/answer.c",
    "int answer(void) {\n"
    "  return 42;\n"
    "}\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* node = spn_add_node(config, "gen_source");
  spn_node_set_fn(node, "gen_source");
  spn_node_add_output(node, SPN_DIR_WORK, "generated/answer.c");
  spn_target_add_source(spn_get_target(spn, "generated_source"), spn_get_subdir(spn, SPN_DIR_WORK, "generated/answer.c"));
  return SPN_OK;
}
