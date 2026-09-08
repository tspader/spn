#include "toolchain.h"

typedef struct {
  bool found;
  u32 at;
} expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdks [FIXTURE_MAX_SDKS];
  spn_triple_t target;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "macos_serves_any_arch",        .sdks = { { SPN_SDK_MACOS, { "/S" } } },              .target = HOST_X64_MACOS,      .expect = { .found = true } },
  { .name = "macos_never_serves_linux",     .sdks = { { SPN_SDK_MACOS, { "/S" } } },              .target = HOST_X64_LINUX },
  { .name = "msvc_serves_its_arch",         .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC,     .expect = { .found = true } },
  { .name = "msvc_never_serves_other_arch", .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_ARM_WIN_MSVC },
  { .name = "msvc_never_serves_mingw",      .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_GNU },
  { .name = "picks_the_serving_entry",      .sdks = { { SPN_SDK_MACOS, { "/S" } }, { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { .found = true, .at = 1 } },
  { .name = "first_serving_entry_wins",     .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { .found = true } },
  { .name = "empty_table_is_absent",        .target = HOST_ARM_MACOS },
};

sp_test_each(sdk_serve, find, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  sp_da(spn_sdk_t) sdks = fixture_sdks(mem, it->sdks, FIXTURE_MAX_SDKS);
  spn_sdk_t found = spn_sdk_find(sdks, it->target);
  sp_must_eq(t, it->expect.found, found.kind != SPN_SDK_NONE);
  if (found.kind) {
    sp_expect_eq(t, spn_sdk_hash(&sdks[it->expect.at]), spn_sdk_hash(&found));
  }
  return SP_OK;
}
