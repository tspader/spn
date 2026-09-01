#include "../compiler.h"

#define cmdline_args_max 4

typedef struct {
  u32 len;
} expect_t;

typedef struct {
  const c8* name;
  const c8* program;
  const c8* args [cmdline_args_max];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "program_only",
    .program = "lib",
    .expect = { .len = 3 },
  },
  {
    .name = "space_separated",
    .program = "lib",
    .args = { "/nologo", "A.o" },
    .expect = { .len = 15 },
  },
  {
    .name = "quotes_counted",
    .program = "lib",
    .args = { "A B" },
    .expect = { .len = 9 },
  },
  {
    .name = "escapes_counted",
    .program = "lib",
    .args = { "A\"B" },
    .expect = { .len = 10 },
  },
  {
    .name = "program_quoted",
    .program = "C:/A B/lib",
    .expect = { .len = 12 },
  },
};

sp_test_each(rsp_cmdline, len, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  spn_invocation_t invocation = {
    .program = spn_arg_lit(sp_cstr_as_str(it->program)),
  };
  sp_da_init(mem, invocation.args);
  u32 len = 0;
  sp_carr_detect_len(it->args, len, it->args[len]);
  sp_for(at, len) {
    sp_da_push(invocation.args, spn_arg_lit(sp_cstr_as_str(it->args[at])));
  }

  sp_expect_eq(t, spn_rsp_cmdline_len(&spn.roots, &invocation), it->expect.len);
  return SP_OK;
}
