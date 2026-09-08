#include "toolchain.h"

typedef struct {
  spn_sdk_kind_t kind;
  test_path_t root;
  test_path_t vc;
} expect_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  fixture_target_t target;
  fixture_sdk_t sdks [FIXTURE_MAX_SDKS];
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "entry_path_wins_over_host",
    .driver = SPN_CC_DRIVER_CLANG,
    .target = { HOST_ARM_MACOS, { "/E" } },
    .sdks = { { SPN_SDK_MACOS, { "/H" } } },
    .expect = { SPN_SDK_MACOS, { "/E" } },
  },
  {
    .name = "host_serves_unlisted_macos",
    .driver = SPN_CC_DRIVER_CLANG,
    .target = { HOST_ARM_MACOS },
    .sdks = { { SPN_SDK_MACOS, { "/H" } } },
    .expect = { SPN_SDK_MACOS, { "/H" } },
  },
  {
    .name = "host_serves_msvc_by_arch",
    .driver = SPN_CC_DRIVER_ZIG,
    .target = { TARGET_ARM_WIN_MSVC },
    .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 }, { SPN_SDK_MSVC, { "/Y" }, SPN_ARCH_ARM64 } },
    .expect = { SPN_SDK_MSVC, .vc = { "/Y/crt/include" } },
  },
  {
    .name = "entry_path_on_msvc_lays_out_xwin",
    .driver = SPN_CC_DRIVER_CLANG,
    .target = { TARGET_WIN_MSVC, { "aa/X", SPN_PATH_ROOT_TOOLCHAIN } },
    .expect = { SPN_SDK_MSVC, .vc = { "aa/X/crt/include", SPN_PATH_ROOT_TOOLCHAIN } },
  },
  {
    .name = "entry_sysroot_is_a_sysroot",
    .driver = SPN_CC_DRIVER_GCC,
    .target = { HOST_ARM_LINUX, { "/S" } },
    .expect = { SPN_SDK_SYSROOT, { "/S" } },
  },
  {
    .name = "nothing_resolves_to_none",
    .driver = SPN_CC_DRIVER_CLANG,
    .target = { HOST_X64_LINUX },
    .sdks = { { SPN_SDK_MACOS, { "/H" } } },
  },
  {
    .name = "wrong_kind_on_host_is_none",
    .driver = SPN_CC_DRIVER_ZIG,
    .target = { HOST_ARM_MACOS },
    .sdks = { { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 } },
  },
};

sp_test_each(sdk_resolve, precedence, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toolchain_info_t info = { .driver = it->driver };
  spn_toolchain_selection_t selection = { .toolchain = &info, .target = fixture_target(it->target) };
  spn_sdk_t sdk = spn_sdk_resolve(mem, fixture_sdks(mem, it->sdks, FIXTURE_MAX_SDKS), &selection);
  sp_must_eq(t, (u32)it->expect.kind, (u32)sdk.kind);
  switch (sdk.kind) {
    case SPN_SDK_NONE: return SP_OK;
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS: return test_check_path(t, sdk.root, it->expect.root);
    case SPN_SDK_MSVC: return test_check_path(t, sdk.msvc.include.vc, it->expect.vc);
  }
  sp_unreachable_return(SP_ERR);
}
