#include "spn.h"

static int setup_ran = 0;

SPN_EXPORT
s32 setup_fn(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "setup: initializing...");
  setup_ran = 1;
  return 0;
}

SPN_EXPORT
s32 codegen_fn(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "codegen: generating header...");
  spn_write_file(spn, "generated.h",
    "#ifndef GENERATED_H\n"
    "#define GENERATED_H\n"
    "#define SETUP_COMPLETE 1\n"
    "#define GENERATED_VALUE 42\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* setup = spn_add_node(config, "setup");
  spn_node_set_fn(setup, "setup_fn");

  spn_node_t* codegen = spn_add_node(config, "codegen");
  spn_node_set_fn(codegen, "codegen_fn");
  spn_node_add_output(codegen, spn_get_subdir(spn, SPN_DIR_WORK, "generated.h"));
  spn_node_link(setup, codegen);
  return SPN_OK;
}
