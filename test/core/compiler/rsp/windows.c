#include "../compiler.h"

static const rsp_test_t tests [] = {
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
    .name = "apostrophe_quoted",
    .program = "lib",
    .args = { "A'B" },
    .expect = {
      .content = "\"A'B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "hash_prefix_quoted",
    .program = "lib",
    .args = { "#A" },
    .expect = {
      .content = "\"#A\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "newline_quoted",
    .program = "lib",
    .args = { "A\nB" },
    .expect = {
      .content = "\"A\nB\"\n",
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

sp_test_each(rsp_windows, render, rsp_test_t, tests, .setup = spn_test_ctx_setup) {
  return expect_rsp(t, it, SPN_RSP_STYLE_WINDOWS);
}
