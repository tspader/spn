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

spn_profile_info_t test_profile(test_profile_t desc) {
  spn_profile_info_t profile = {
    .arch = desc.arch,
    .os = desc.os,
    .abi = desc.abi,
    .linkage = desc.linkage,
    .standard = desc.standard,
    .mode = desc.mode,
    .opt = desc.opt,
    .sanitizers = desc.sanitizers,
  };
  if (desc.sysroot) {
    profile.sysroot.sub = sp_cstr_as_str(desc.sysroot);
  }
  return profile;
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

sp_err_t expect_rsp(sp_test_t* t, const rsp_test_t* it, spn_rsp_style_t style) {
  sp_mem_t mem = sp_test_arena(t);
  const spn_path_roots_t* roots = &spn.roots;

  spn_invocation_t invocation = {
    .program = spn_arg_lit(sp_cstr_as_str(it->program)),
    .launcher = it->launcher,
  };
  sp_da_init(mem, invocation.args);
  u32 len = 0;
  sp_carr_detect_len(it->args, len, it->args[len]);
  sp_for(at, len) {
    sp_da_push(invocation.args, spn_arg_lit(sp_cstr_as_str(it->args[at])));
  }

  spn_rsp_t rsp = spn_rsp_render(mem, roots, &invocation, style, test_arg_path("A.rsp"));
  sp_expect_str_eq_c(t, rsp.content, it->expect.content);
  sp_expect_str_eq_c(t, spn_arg_str(roots, mem, rsp.invocation.program), it->program);
  sp_expect_eq(t, rsp.invocation.launcher, it->launcher);
  sp_da(sp_str_t) args = spn_invocation_args(roots, mem, &rsp.invocation);
  sp_must_strs_eq(t, args, sp_da_size(args), it->expect.args);
  return SP_OK;
}
