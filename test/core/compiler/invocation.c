#include "compiler.h"
#include "compiler/push.h"

#define INVOCATION_MAX_ARGS 4
#define INVOCATION_MAX_ENV 2
#define INVOCATION_MAX_VALUES 3

typedef struct {
  const c8* key;
  const c8* values [INVOCATION_MAX_VALUES];
} env_t;

typedef struct {
  const c8* str;
} expect_t;

typedef struct {
  const c8* name;
  const c8* args [INVOCATION_MAX_ARGS];
  env_t env [INVOCATION_MAX_ENV];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "program_and_args", .args = { "-c", "A.c" }, .expect = { .str = "cc -c A.c" } },
  { .name = "env_precedes_program", .args = { "A.o" }, .env = { { "K", { "/L" } } }, .expect = { .str = "K=/L cc A.o" } },
  { .name = "list_values_join_with_semicolons", .env = { { "K", { "/A", "/B" } }, { "L", { "/C" } } }, .expect = { .str = "K=/A;/B L=/C cc" } },
};

sp_test_each(invocation, to_str, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_invocation_t invocation = { .program = spn_arg_lit(sp_str_lit("cc")) };
  sp_carr_for(it->args, at) {
    if (!it->args[at]) {
      break;
    }
    spn_cc_push_c(mem, &invocation, it->args[at]);
  }
  sp_carr_for(it->env, at) {
    if (!it->env[at].key) {
      break;
    }
    spn_path_t values [INVOCATION_MAX_VALUES] = sp_zero;
    u32 count = 0;
    sp_carr_detect_len(it->env[at].values, count, it->env[at].values[count]);
    sp_for(v, count) {
      values[v] = test_arg_path(it->env[at].values[v]);
    }
    spn_cc_push_env_paths(mem, &invocation, it->env[at].key, values, count);
  }
  sp_expect_str_eq_c(t, spn_invocation_to_str(mem, &invocation), it->expect.str);
  return SP_OK;
}
