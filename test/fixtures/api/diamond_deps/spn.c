#include "spn.h"

SPN_EXPORT
s32 gen_base(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_base: creating base.h");
  spn_write_file(spn, "base.h",
    "#ifndef BASE_H\n"
    "#define BASE_H\n"
    "#define BASE_VAL 10\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
s32 gen_left(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_left: creating left.h");
  spn_write_file(spn, "left.h",
    "#ifndef LEFT_H\n"
    "#define LEFT_H\n"
    "#include \"base.h\"\n"
    "#define LEFT_VAL (BASE_VAL * 2)\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
s32 gen_right(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_right: creating right.h");
  spn_write_file(spn, "right.h",
    "#ifndef RIGHT_H\n"
    "#define RIGHT_H\n"
    "#include \"base.h\"\n"
    "#define RIGHT_VAL (BASE_VAL * 3)\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
s32 gen_final(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "gen_final: creating final.h");
  spn_write_file(spn, "final.h",
    "#ifndef FINAL_H\n"
    "#define FINAL_H\n"
    "#include \"left.h\"\n"
    "#include \"right.h\"\n"
    "#define FINAL_VAL (LEFT_VAL + RIGHT_VAL)\n"
    "#endif\n"
  );
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  const c8* base_h = spn_get_subdir(spn, SPN_DIR_WORK, "base.h");
  const c8* left_h = spn_get_subdir(spn, SPN_DIR_WORK, "left.h");
  const c8* right_h = spn_get_subdir(spn, SPN_DIR_WORK, "right.h");
  const c8* final_h = spn_get_subdir(spn, SPN_DIR_WORK, "final.h");

  spn_node_t* base = spn_add_node(config, "gen_base");
  spn_node_set_fn(base, "gen_base");
  spn_node_add_output(base, base_h);

  spn_node_t* left = spn_add_node(config, "gen_left");
  spn_node_set_fn(left, "gen_left");
  spn_node_add_input(left, base_h);
  spn_node_add_output(left, left_h);

  spn_node_t* right = spn_add_node(config, "gen_right");
  spn_node_set_fn(right, "gen_right");
  spn_node_add_input(right, base_h);
  spn_node_add_output(right, right_h);

  spn_node_t* final = spn_add_node(config, "gen_final");
  spn_node_set_fn(final, "gen_final");
  spn_node_add_input(final, left_h);
  spn_node_add_input(final, right_h);
  spn_node_add_output(final, final_h);
  return SPN_OK;
}
