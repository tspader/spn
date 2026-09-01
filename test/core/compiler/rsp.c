#include "compiler.h"

#include "compiler/rsp.h"

#define rsp_args_max 8

typedef struct {
  const c8* content;
  const c8* args [render_args_max];
} expect_t;

typedef struct {
  const c8* name;
  const c8* program;
  u32 launcher;
  const c8* args [rsp_args_max];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "one_arg_per_line",
    .program = "lib",
    .args = { "/nologo", "/OUT:A.lib", "B.o", "C.o" },
    .expect = {
      .content = "/nologo\n/OUT:A.lib\nB.o\nC.o\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "launcher_stays_on_command_line",
    .program = "zig",
    .launcher = 1,
    .args = { "ar", "rcs", "A.a", "B.o" },
    .expect = {
      .content = "rcs\nA.a\nB.o\n",
      .args = { "ar", "@A.rsp" },
    },
  },
  {
    .name = "no_args",
    .program = "lib",
    .expect = {
      .content = "",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "whitespace_quoted",
    .program = "lib",
    .args = { "A B", "C\tD" },
    .expect = {
      .content = "\"A B\"\n\"C\tD\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "empty_quoted",
    .program = "lib",
    .args = { "" },
    .expect = {
      .content = "\"\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "quote_escaped",
    .program = "lib",
    .args = { "A\"B" },
    .expect = {
      .content = "\"A\\\"B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "backslashes_before_quote_doubled",
    .program = "lib",
    .args = { "A\\\\\"B" },
    .expect = {
      .content = "\"A\\\\\\\\\\\"B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "trailing_backslashes_doubled_when_quoted",
    .program = "lib",
    .args = { "A B\\" },
    .expect = {
      .content = "\"A B\\\\\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "backslashes_kept_when_unquoted",
    .program = "lib",
    .args = { "A\\B\\" },
    .expect = {
      .content = "A\\B\\\n",
      .args = { "@A.rsp" },
    },
  },
};

sp_test_each(rsp, render, test_t, tests, .setup = spn_test_ctx_setup) {
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

  spn_rsp_t rsp = spn_rsp_render(mem, roots, &invocation, test_arg_path("A.rsp"));
  sp_expect_str_eq_c(t, rsp.content, it->expect.content);
  sp_expect_str_eq_c(t, spn_arg_str(roots, mem, rsp.invocation.program), it->program);
  sp_expect_eq(t, rsp.invocation.launcher, it->launcher);
  sp_da(sp_str_t) args = spn_invocation_args(roots, mem, &rsp.invocation);
  sp_must_strs_eq(t, args, sp_da_size(args), it->expect.args);
  return SP_OK;
}
