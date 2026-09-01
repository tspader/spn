#include "../compiler.h"

static const rsp_test_t tests [] = {
  {
    .name = "plain_unquoted",
    .program = "gcc",
    .args = { "-o", "A/B.exe", "C.o" },
    .expect = {
      .content = "-o\nA/B.exe\nC.o\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "whitespace_quoted",
    .program = "gcc",
    .args = { "A B" },
    .expect = {
      .content = "\"A B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "apostrophe_quoted",
    .program = "gcc",
    .args = { "A'B" },
    .expect = {
      .content = "\"A'B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "quote_escaped",
    .program = "gcc",
    .args = { "A\"B" },
    .expect = {
      .content = "\"A\\\"B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "backslash_quoted_and_escaped",
    .program = "gcc",
    .args = { "A\\B" },
    .expect = {
      .content = "\"A\\\\B\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "trailing_backslash_escaped",
    .program = "gcc",
    .args = { "A\\" },
    .expect = {
      .content = "\"A\\\\\"\n",
      .args = { "@A.rsp" },
    },
  },
  {
    .name = "backslash_before_quote_both_escaped",
    .program = "gcc",
    .args = { "A\\\"B" },
    .expect = {
      .content = "\"A\\\\\\\"B\"\n",
      .args = { "@A.rsp" },
    },
  },
};

sp_test_each(rsp_gnu, render, rsp_test_t, tests, .setup = spn_test_ctx_setup) {
  return expect_rsp(t, it, SPN_RSP_STYLE_GNU);
}
