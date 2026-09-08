#include "toolchain.h"

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  spn_triple_t target;
  bool expect;
} serve_test_t;

static const serve_test_t serve_tests [] = {
  { .name = "macos_serves_any_arch",      .sdk = { SPN_SDK_MACOS, { "/S" } },                .target = HOST_ARM_MACOS,      .expect = true },
  { .name = "macos_serves_x64",           .sdk = { SPN_SDK_MACOS, { "/S" } },                .target = HOST_X64_MACOS,      .expect = true },
  { .name = "macos_never_serves_linux",   .sdk = { SPN_SDK_MACOS, { "/S" } },                .target = HOST_X64_LINUX },
  { .name = "msvc_serves_its_arch",       .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .target = TARGET_WIN_MSVC,     .expect = true },
  { .name = "msvc_never_serves_other_arch", .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, .target = TARGET_ARM_WIN_MSVC },
  { .name = "msvc_never_serves_mingw",    .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .target = TARGET_WIN_GNU },
  { .name = "sysroot_never_serves",       .sdk = { SPN_SDK_SYSROOT, { "/S" } },              .target = HOST_X64_LINUX },
};

typedef struct {
  const c8* name;
  fixture_sdk_t sdks [FIXTURE_MAX_SDKS];
  spn_triple_t target;
  s32 expect;
} find_test_t;

static const find_test_t find_tests [] = {
  { .name = "picks_the_serving_entry", .sdks = { { SPN_SDK_MACOS, { "/S" } }, { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = 1 },
  { .name = "first_serving_entry_wins", .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC },
  { .name = "none_serving_is_absent",  .sdks = { { SPN_SDK_MACOS, { "/S" } } },             .target = TARGET_WIN_MSVC, .expect = -1 },
  { .name = "empty_table_is_absent",   .target = HOST_ARM_MACOS, .expect = -1 },
};

sp_test_each(sdk_serve, serves, serve_test_t, serve_tests) {
  spn_sdk_t sdk = fixture_sdk(sp_test_arena(t), it->sdk);
  sp_expect_eq(t, it->expect, spn_sdk_serves(&sdk, it->target));
  return SP_OK;
}

sp_test_each(sdk_serve, find, find_test_t, find_tests) {
  sp_da(spn_sdk_t) sdks = fixture_sdks(sp_test_arena(t), it->sdks, FIXTURE_MAX_SDKS);
  const spn_sdk_t* found = spn_sdk_find(sdks, it->target);
  if (it->expect < 0) {
    sp_expect(t, !found);
    return SP_OK;
  }
  sp_must(t, found);
  sp_expect_eq(t, (u64)it->expect, (u64)(found - sdks));
  return SP_OK;
}
