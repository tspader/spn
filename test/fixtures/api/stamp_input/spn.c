#include "spn.h"

SPN_EXPORT
s32 gen_data(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_data: creating data.txt");
  spn_write_file(spn, "data.txt", "hello world");
  return 0;
}

SPN_EXPORT
s32 validate(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "validate: checking data.txt exists (stamp-only, no output)");
  return 0;
}

SPN_EXPORT
s32 finalize(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "finalize: creating header after validation stamp");
  spn_write_file(spn, "validated.h",
    "#ifndef VALIDATED_H\n"
    "#define VALIDATED_H\n"
    "#define VALIDATION_PASSED 1\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  const c8* data_txt = spn_get_subdir(spn, SPN_DIR_WORK, "data.txt");
  const c8* validated_h = spn_get_subdir(spn, SPN_DIR_WORK, "validated.h");

  spn_node_t* gen = spn_add_node(config, "gen_data");
  spn_node_set_fn(gen, "gen_data");
  spn_node_add_output(gen, data_txt);

  spn_node_t* validate_node = spn_add_node(config, "validate");
  spn_node_set_fn(validate_node, "validate");
  spn_node_add_input(validate_node, data_txt);

  spn_node_t* final = spn_add_node(config, "finalize");
  spn_node_set_fn(final, "finalize");
  spn_node_add_output(final, validated_h);
  spn_node_link(validate_node, final);
  return SPN_OK;
}
