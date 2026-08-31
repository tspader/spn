#include "spn_test.h"

#include "os/os.h"

#define OS_MAX_VARS 2

typedef struct {
  const c8* name;
  struct {
    const c8* name;
    const c8* value;
  } vars [OS_MAX_VARS];
  const c8* expect;
} env_path_t;

static const env_path_t env_path_tests [] = {
  { "exact",        { { "PATH", "A" } },                 "A" },
  { "windows_case", { { "Path", "A" } },                 "A" },
  { "lowercase",    { { "path", "A" } },                 "A" },
  { "exact_wins",   { { "Path", "B" }, { "PATH", "A" } }, "A" },
  { "missing",      { { "HOME", "B" } },                 "" },
  { "empty",        sp_zero,                             "" },
};

sp_test_each(os, env_path, env_path_t, env_path_tests) {
  sp_env_t env = sp_zero;
  sp_env_init(sp_test_arena(t), &env);
  sp_carr_for(it->vars, at) {
    if (!it->vars[at].name) {
      break;
    }
    sp_env_insert(&env, sp_cstr_as_str(it->vars[at].name), sp_cstr_as_str(it->vars[at].value));
  }
  sp_expect_str_eq_c(t, sp_env_get_path(&env), it->expect);
  return SP_OK;
}
