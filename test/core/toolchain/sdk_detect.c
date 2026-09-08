#include "toolchain.h"

typedef struct {
  const c8* key;
  const c8* value;
} var_t;

typedef struct {
  const c8* name;
  var_t vars [2];
  spn_triple_t host;
  fixture_sdk_t expect [FIXTURE_MAX_SDKS];
} test_t;

static const test_t tests [] = {
  { .name = "macos_env_on_any_host", .vars = { { "SPN_MACOS_SDK", "/S" } }, .host = HOST_X64_LINUX, .expect = { { SPN_SDK_MACOS, { "/S" } } } },
  { .name = "empty_env_off_apple_and_windows", .host = HOST_X64_LINUX },
};

sp_test_each(sdk_detect, env, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = sp_zero;
  sp_env_init(mem, &env);
  sp_carr_for(it->vars, at) {
    if (!it->vars[at].key) break;
    sp_env_insert(&env, sp_cstr_as_str(it->vars[at].key), sp_cstr_as_str(it->vars[at].value));
  }

  sp_da(spn_sdk_t) sdks = spn_sdk_detect(mem, &env, it->host);
  u32 count = 0;
  sp_carr_detect_len(it->expect, count, it->expect[count].kind);
  sp_must_eq(t, count, (u32)sp_da_size(sdks));
  sp_for(at, count) {
    sp_expect_eq(t, (u32)it->expect[at].kind, (u32)sdks[at].kind);
    if (test_check_path(t, sdks[at].root, it->expect[at].root)) return SP_ERR;
  }
  return SP_OK;
}
