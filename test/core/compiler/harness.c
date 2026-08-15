#include "compiler.h"

spn_path_t test_arg_path(const c8* value) {
  return (spn_path_t) { .sub = sp_cstr_as_str(value) };
}

sp_err_t expect_args(sp_test_t* t, spn_invocation_t* invocation, render_expect_t expect) {
  sp_mem_t mem = sp_test_arena(t);
  const spn_path_roots_t* roots = &spn.roots;
  sp_da(sp_str_t) args = spn_invocation_args(roots, mem, invocation);
  sp_expect_str_eq_c(t, spn_arg_str(roots, mem, invocation->program), expect.command);
  sp_must_strs_eq(t, args, sp_da_size(args), expect.args);
  return SP_OK;
}

spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver) {
  return (spn_cc_toolchain_t) {
    .name = sp_str_lit("test"),
    .driver = driver,
    .compiler = { .program = spn_arg_lit(sp_str_lit("cc")) },
    .cxx = { .program = spn_arg_lit(sp_str_lit("c++")) },
    .archiver = { .program = spn_arg_lit(sp_str_lit("ar")) },
  };
}
