#include "toolchain.h"

#define SDK_DETECT_MAX_VARS 2

typedef struct {
  const c8* key;
  const c8* value;
} var_t;

typedef struct {
  test_path_t macos;
} expect_t;

typedef struct {
  const c8* name;
  var_t vars [SDK_DETECT_MAX_VARS];
  spn_triple_t host;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "macos_env_off_apple", .vars = { { "SPN_MACOS_SDK", "/S" } }, .host = HOST_X64_LINUX, .expect = { .macos = { "/S" } } },
  { .name = "empty_env_off_apple_and_windows", .host = HOST_X64_LINUX },
};

sp_test_each(sdk_detect, env, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_env_t env = sp_zero;
  sp_env_init(mem, &env);
  sp_carr_for(it->vars, at) {
    if (!it->vars[at].key) {
      break;
    }
    sp_env_insert(&env, sp_cstr_as_str(it->vars[at].key), sp_cstr_as_str(it->vars[at].value));
  }

  spn_path_roots_t roots = sp_zero;
  spn_sdk_host_t host = spn_sdk_detect(mem, &roots, &env, it->host);
  sp_expect_eq(t, 0u, (u32)sp_da_size(host.msvc));
  return test_check_path(t, host.macos.root, it->expect.macos);
}
