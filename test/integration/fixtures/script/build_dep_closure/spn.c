#include "spn.h"

#include "alpha.h"

SPN_EXPORT
s32 gen_closure(spn_t* spn, spn_node_ctx_t* ctx) {
  if (alpha_magic() != 7) {
    spn_log(spn, "unexpected alpha value");
    return 1;
  }

  spn_io_write("/work/closure.h",
    "#ifndef CLOSURE_H\n"
    "#define CLOSURE_H\n"
    "#define CLOSURE_VALUE 7\n"
    "#endif\n"
  );

  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node(config, "gen_closure");
  spn_node_set_fn(gen, "gen_closure");
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "closure.h"));
  return SPN_OK;
}
