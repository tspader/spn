#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_sdk_kind_t kind;
  const c8* dir;
  spn_wasi_spelling_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "wasip1_layout",      .kind = SPN_SDK_SYSROOT, .dir = "lib/wasm32-wasip1", .expect = SPN_WASI_SPELLING_WASIP1 },
  { .name = "legacy_layout",      .kind = SPN_SDK_SYSROOT, .dir = "lib/wasm32-wasi" },
  { .name = "empty_sysroot",      .kind = SPN_SDK_SYSROOT },
  { .name = "no_sysroot",         .kind = SPN_SDK_NONE, .dir = "lib/wasm32-wasip1" },
};

sp_test_each(sdk_wasi, spelling, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t root = sp_test_dir(t);
  if (it->dir) {
    sp_fs_create_dir(sp_fs_join_path(mem, root, sp_cstr_as_str(it->dir)));
  }
  spn_path_roots_t roots = sp_zero;
  spn_sdk_t sdk = it->kind == SPN_SDK_SYSROOT ? spn_sdk_sysroot((spn_path_t) { .sub = root }) : sp_zero_struct(spn_sdk_t);
  sp_expect_eq(t, (u32)it->expect, (u32)spn_sdk_wasi_spelling(&roots, mem, &sdk));
  return SP_OK;
}
