#include "toolchain.h"

typedef struct {
  const c8* name;
  fixture_sdk_t sdks [FIXTURE_MAX_SDKS];
  spn_triple_t target;
  fixture_sdk_expect_t expect;
} test_t;

static const test_t tests [] = {
  { .name = "macos_serves_any_arch",        .sdks = { { SPN_SDK_MACOS, { "/S" } } },              .target = HOST_X64_MACOS,      .expect = { SPN_SDK_MACOS, { "/S" } } },
  { .name = "macos_never_serves_linux",     .sdks = { { SPN_SDK_MACOS, { "/S" } } },              .target = HOST_X64_LINUX },
  { .name = "msvc_serves_its_arch",         .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC,     .expect = { SPN_SDK_MSVC, .vc = { "/X/crt/lib/x86_64" } } },
  { .name = "msvc_never_serves_other_arch", .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_ARM_WIN_MSVC },
  { .name = "msvc_never_serves_mingw",      .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_GNU },
  { .name = "kinds_do_not_cross",           .sdks = { { SPN_SDK_MACOS, { "/S" } }, { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { SPN_SDK_MSVC, .vc = { "/X/crt/lib/x86_64" } } },
  { .name = "first_arch_entry_wins",        .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_X64 } }, .target = TARGET_WIN_MSVC, .expect = { SPN_SDK_MSVC, .vc = { "/X/crt/lib/x86_64" } } },
  { .name = "empty_table_is_absent",        .target = HOST_ARM_MACOS },
};

sp_test_each(sdk_serve, from_host, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_sdk_host_t host = fixture_sdks(mem, it->sdks, FIXTURE_MAX_SDKS);
  return fixture_check_sdk(t, spn_sdk_from_host(&host, it->target), it->expect);
}
