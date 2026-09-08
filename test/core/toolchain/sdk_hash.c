#include "toolchain.h"

typedef struct {
  bool equal;
} expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t a;
  fixture_sdk_t b;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "same_sdk_same_hash",      .a = { SPN_SDK_MACOS, { "/S" } },                .b = { SPN_SDK_MACOS, { "/S" } },                .expect = { .equal = true } },
  { .name = "none_differs_from_root",  .a = { SPN_SDK_NONE },                           .b = { SPN_SDK_MACOS, { "/S" } } },
  { .name = "root_changes_hash",       .a = { SPN_SDK_MACOS, { "/S" } },                .b = { SPN_SDK_MACOS, { "/T" } } },
  { .name = "kind_changes_hash",       .a = { SPN_SDK_MACOS, { "/S" } },                .b = { SPN_SDK_SYSROOT, { "/S" } } },
  { .name = "path_root_changes_hash",  .a = { SPN_SDK_SYSROOT, { "S" } },               .b = { SPN_SDK_SYSROOT, { "S", SPN_PATH_ROOT_TOOLCHAIN } } },
  { .name = "msvc_root_changes_hash",  .a = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .b = { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_X64 } },
  { .name = "arch_changes_hash",       .a = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .b = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_ARM64 } },
  { .name = "libc_file_changes_hash",  .a = { SPN_SDK_LIBC_MSVC, { "/L" } },            .b = { SPN_SDK_LIBC_MSVC, { "/M" } } },
  { .name = "libc_kind_changes_hash",  .a = { SPN_SDK_LIBC_MACOS, { "/L" } },           .b = { SPN_SDK_LIBC_MSVC, { "/L" } } },
};

sp_test_each(sdk_hash, hash, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_sdk_t a = fixture_sdk(mem, it->a);
  spn_sdk_t b = fixture_sdk(mem, it->b);
  sp_expect_eq(t, it->expect.equal, spn_sdk_hash(&a) == spn_sdk_hash(&b));
  return SP_OK;
}
