#include "spn.h"

typedef struct {
  s32 base_value;
  s32 multiplier;
} codegen_config_t;

spn_err_t generate_with_config(spn_node_ctx_t* ctx) {
  codegen_config_t* cfg = (codegen_config_t*)ctx->user_data;
  if (!cfg) {
    spn_log(ctx->build, "ERROR: user_data is null!");
    return SPN_ERROR;
  }

  s32 result = cfg->base_value * cfg->multiplier;
  
  if (result != 42) {
    spn_log(ctx->build, "ERROR: expected result 42");
    return SPN_ERROR;
  }
  
  spn_write_file(ctx->build, "config.h",
    "#ifndef CONFIG_H\n"
    "#define CONFIG_H\n"
    "#define MY_BASE 7\n"
    "#define MY_MULT 6\n"
    "#define MY_RESULT 42\n"
    "#endif\n"
  );
  spn_log(ctx->build, "generated config.h with user_data");
  return SPN_OK;
}

static codegen_config_t g_config = {
  .base_value = 7,
  .multiplier = 6,
};

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, "build/debug/work");

  spn_node_t gen = spn_add_node(ctx, "gen_config");
  spn_node_set_fn(gen, generate_with_config);
  spn_node_set_user_data(gen, &g_config);
  spn_node_add_output(gen, spn_get_subdir(ctx, SPN_DIR_WORK, "config.h"));
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
