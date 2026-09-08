#include "toolchain.h"

typedef struct {
  bool serves;
} serve_expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  spn_triple_t target;
  serve_expect_t expect;
} serve_test_t;

static const serve_test_t serve_tests [] = {
  { .name = "macos_serves_any_arch",        .sdk = { SPN_SDK_MACOS, { "/S" } },                .target = HOST_X64_MACOS,      .expect = { .serves = true } },
  { .name = "macos_never_serves_linux",     .sdk = { SPN_SDK_MACOS, { "/S" } },                .target = HOST_X64_LINUX },
  { .name = "msvc_serves_its_arch",         .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .target = TARGET_WIN_MSVC,     .expect = { .serves = true } },
  { .name = "msvc_never_serves_other_arch", .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .target = TARGET_ARM_WIN_MSVC },
  { .name = "msvc_never_serves_mingw",      .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },   .target = TARGET_WIN_GNU },
  { .name = "sysroot_never_serves",         .sdk = { SPN_SDK_SYSROOT, { "/S" } },              .target = HOST_X64_LINUX },
};

typedef struct {
  bool found;
  u32 at;
} find_expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdks [FIXTURE_MAX_SDKS];
  spn_triple_t target;
  find_expect_t expect;
} find_test_t;

static const find_test_t find_tests [] = {
  { .name = "picks_the_serving_entry",  .sdks = { { SPN_SDK_MACOS, { "/S" } }, { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { .found = true, .at = 1 } },
  { .name = "first_serving_entry_wins", .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { .found = true } },
  { .name = "none_serving_is_absent",   .sdks = { { SPN_SDK_MACOS, { "/S" } } },             .target = TARGET_WIN_MSVC },
  { .name = "empty_table_is_absent",    .target = HOST_ARM_MACOS },
};

sp_test_each(sdk_serve, serves, serve_test_t, serve_tests) {
  spn_sdk_t sdk = fixture_sdk(sp_test_arena(t), it->sdk);
  sp_expect_eq(t, it->expect.serves, spn_sdk_serves(&sdk, it->target));
  return SP_OK;
}

sp_test_each(sdk_serve, find, find_test_t, find_tests) {
  sp_da(spn_sdk_t) sdks = fixture_sdks(sp_test_arena(t), it->sdks, FIXTURE_MAX_SDKS);
  const spn_sdk_t* found = spn_sdk_find(sdks, it->target);
  sp_must_eq(t, it->expect.found, found != SP_NULLPTR);
  if (found) {
    sp_expect_eq(t, (u64)it->expect.at, (u64)(found - sdks));
  }
  return SP_OK;
}
