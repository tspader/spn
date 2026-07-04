#include "spn.h"

typedef struct {
  s32 base_value;
  s32 multiplier;
} codegen_config_t;

SPN_EXPORT
s32 generate_with_config(spn_t* spn, spn_node_ctx_t* ctx) {
  codegen_config_t* cfg = (codegen_config_t*)spn_node_ctx_get_user_data(ctx);
  if (!cfg) {
    spn_log(spn, "ERROR: user_data is null!");
    return 1;
  }

  s32 result = cfg->base_value * cfg->multiplier;

  if (result != 42) {
    spn_log(spn, "ERROR: expected result 42");
    return 1;
  }

  spn_io_write("/work/config.h",
    "#ifndef CONFIG_H\n"
    "#define CONFIG_H\n"
    "#define MY_BASE 7\n"
    "#define MY_MULT 6\n"
    "#define MY_RESULT 42\n"
    "#endif\n"
  );
  spn_log(spn, "generated config.h with user_data");
  return 0;
}

static codegen_config_t g_config = {
  .base_value = 7,
  .multiplier = 6,
};

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node(config, "gen_config");
  spn_node_set_fn(gen, "generate_with_config");
  spn_node_set_user_data(gen, &g_config);
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "config.h"));
  return SPN_OK;
}
