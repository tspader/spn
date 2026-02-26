#include "spn.h"

s32 gen_alpha(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_alpha");
  spn_write_file(spn_node_ctx_get_build(ctx), "alpha.h",
    "#ifndef ALPHA_H\n#define ALPHA_H\n#define ALPHA 1\n#endif\n");
  return 0;
}

s32 gen_beta(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_beta");
  spn_write_file(spn_node_ctx_get_build(ctx), "beta.h",
    "#ifndef BETA_H\n#define BETA_H\n#define BETA 2\n#endif\n");
  return 0;
}

s32 gen_gamma(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_gamma");
  spn_write_file(spn_node_ctx_get_build(ctx), "gamma.h",
    "#ifndef GAMMA_H\n#define GAMMA_H\n#define GAMMA 3\n#endif\n");
  return 0;
}

s32 gen_delta(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_delta");
  spn_write_file(spn_node_ctx_get_build(ctx), "delta.h",
    "#ifndef DELTA_H\n#define DELTA_H\n#define DELTA 4\n#endif\n");
  return 0;
}

s32 gen_combined(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_combined: merging all inputs");
  spn_write_file(spn_node_ctx_get_build(ctx), "combined.h",
    "#ifndef COMBINED_H\n"
    "#define COMBINED_H\n"
    "#include \"alpha.h\"\n"
    "#include \"beta.h\"\n"
    "#include \"gamma.h\"\n"
    "#include \"delta.h\"\n"
    "#define COMBINED (ALPHA + BETA + GAMMA + DELTA)\n"
    "#endif\n"
  );
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  const c8* alpha_h = spn_get_subdir(ctx, SPN_DIR_WORK, "alpha.h");
  const c8* beta_h = spn_get_subdir(ctx, SPN_DIR_WORK, "beta.h");
  const c8* gamma_h = spn_get_subdir(ctx, SPN_DIR_WORK, "gamma.h");
  const c8* delta_h = spn_get_subdir(ctx, SPN_DIR_WORK, "delta.h");
  const c8* combined_h = spn_get_subdir(ctx, SPN_DIR_WORK, "combined.h");

  spn_node_t* alpha = spn_add_node(ctx, "gen_alpha");
  spn_node_set_fn(alpha, gen_alpha);
  spn_node_add_output(alpha, alpha_h);

  spn_node_t* beta = spn_add_node(ctx, "gen_beta");
  spn_node_set_fn(beta, gen_beta);
  spn_node_add_output(beta, beta_h);

  spn_node_t* gamma = spn_add_node(ctx, "gen_gamma");
  spn_node_set_fn(gamma, gen_gamma);
  spn_node_add_output(gamma, gamma_h);

  spn_node_t* delta = spn_add_node(ctx, "gen_delta");
  spn_node_set_fn(delta, gen_delta);
  spn_node_add_output(delta, delta_h);

  spn_node_t* combined = spn_add_node(ctx, "gen_combined");
  spn_node_set_fn(combined, gen_combined);
  spn_node_add_input(combined, alpha_h);
  spn_node_add_input(combined, beta_h);
  spn_node_add_input(combined, gamma_h);
  spn_node_add_input(combined, delta_h);
  spn_node_add_output(combined, combined_h);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
