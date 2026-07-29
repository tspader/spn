#include "compiler.h"

sp_err_t expect_args(sp_test_t* t, spn_invocation_t* invocation, render_expect_t expect) {
  sp_expect_str_eq_c(t, invocation->program, expect.command);

  u32 count = 0;
  sp_carr_detect_len(expect.args, count, expect.args[count]);
  sp_must_eq(t, count, sp_da_size(invocation->args));
  sp_for(it, count) {
    sp_expect_str_eq_c(t, invocation->args[it], expect.args[it]);
  }

  return SP_OK;
}

spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver) {
  return (spn_cc_toolchain_t) {
    .name = sp_str_lit("test"),
    .driver = driver,
    .compiler = { .program = sp_str_lit("cc") },
    .cxx = { .program = sp_str_lit("c++") },
    .archiver = { .program = sp_str_lit("ar") },
  };
}
