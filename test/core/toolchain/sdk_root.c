#include "toolchain.h"

typedef struct {
  test_path_t include [4];
  test_path_t lib [3];
} msvc_expect_t;

typedef struct {
  spn_sdk_kind_t kind;
  test_path_t root;
  msvc_expect_t msvc;
} expect_t;

typedef struct {
  const c8* name;
  fixture_sdk_t sdk;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "sysroot_keeps_root",
    .sdk = { SPN_SDK_SYSROOT, { "/S" }, SPN_ARCH_X64 },
    .expect = { .kind = SPN_SDK_SYSROOT, .root = { "/S" } },
  },
  {
    .name = "macos_keeps_root",
    .sdk = { SPN_SDK_MACOS, { "/S" }, SPN_ARCH_ARM64 },
    .expect = { .kind = SPN_SDK_MACOS, .root = { "/S" } },
  },
  {
    .name = "rooted_root_is_kept",
    .sdk = { SPN_SDK_SYSROOT, { "aa/S", SPN_PATH_ROOT_TOOLCHAIN }, SPN_ARCH_X64 },
    .expect = { .kind = SPN_SDK_SYSROOT, .root = { "aa/S", SPN_PATH_ROOT_TOOLCHAIN } },
  },
  {
    .name = "msvc_lays_out_xwin_x64",
    .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_X64 },
    .expect = {
      .kind = SPN_SDK_MSVC,
      .msvc = {
        .include = { { "/X/crt/include" }, { "/X/sdk/include/ucrt" }, { "/X/sdk/include/um" }, { "/X/sdk/include/shared" } },
        .lib = { { "/X/crt/lib/x86_64" }, { "/X/sdk/lib/ucrt/x86_64" }, { "/X/sdk/lib/um/x86_64" } },
      },
    },
  },
  {
    .name = "msvc_lays_out_xwin_arm64",
    .sdk = { SPN_SDK_MSVC, { "/X" }, SPN_ARCH_ARM64 },
    .expect = {
      .kind = SPN_SDK_MSVC,
      .msvc = {
        .include = { { "/X/crt/include" }, { "/X/sdk/include/ucrt" }, { "/X/sdk/include/um" }, { "/X/sdk/include/shared" } },
        .lib = { { "/X/crt/lib/aarch64" }, { "/X/sdk/lib/ucrt/aarch64" }, { "/X/sdk/lib/um/aarch64" } },
      },
    },
  },
  {
    .name = "msvc_dirs_keep_the_root",
    .sdk = { SPN_SDK_MSVC, { "aa/X", SPN_PATH_ROOT_TOOLCHAIN }, SPN_ARCH_X64 },
    .expect = {
      .kind = SPN_SDK_MSVC,
      .msvc = {
        .include = { { "aa/X/crt/include", SPN_PATH_ROOT_TOOLCHAIN }, { "aa/X/sdk/include/ucrt", SPN_PATH_ROOT_TOOLCHAIN }, { "aa/X/sdk/include/um", SPN_PATH_ROOT_TOOLCHAIN }, { "aa/X/sdk/include/shared", SPN_PATH_ROOT_TOOLCHAIN } },
        .lib = { { "aa/X/crt/lib/x86_64", SPN_PATH_ROOT_TOOLCHAIN }, { "aa/X/sdk/lib/ucrt/x86_64", SPN_PATH_ROOT_TOOLCHAIN }, { "aa/X/sdk/lib/um/x86_64", SPN_PATH_ROOT_TOOLCHAIN } },
      },
    },
  },
};

static sp_err_t check_msvc(sp_test_t* t, const spn_sdk_msvc_t* msvc, const msvc_expect_t* expect, spn_arch_t arch) {
  sp_expect_eq(t, (u32)arch, (u32)msvc->arch);
  spn_path_t include [] = { msvc->include.vc, msvc->include.ucrt, msvc->include.um, msvc->include.shared };
  sp_carr_for(include, it) {
    if (test_check_path(t, include[it], expect->include[it])) return SP_ERR;
  }
  spn_path_t lib [] = { msvc->lib.vc, msvc->lib.ucrt, msvc->lib.um };
  sp_carr_for(lib, it) {
    if (test_check_path(t, lib[it], expect->lib[it])) return SP_ERR;
  }
  return SP_OK;
}

sp_test_each(sdk_root, layout, test_t, tests) {
  spn_sdk_t sdk = fixture_sdk(sp_test_arena(t), it->sdk);
  sp_must_eq(t, (u32)it->expect.kind, (u32)sdk.kind);
  switch (sdk.kind) {
    case SPN_SDK_SYSROOT:
    case SPN_SDK_MACOS: {
      return test_check_path(t, sdk.root, it->expect.root);
    }
    case SPN_SDK_MSVC: {
      return check_msvc(t, &sdk.msvc, &it->expect.msvc, it->sdk.arch);
    }
    case SPN_SDK_NONE: {
      sp_unreachable_case();
    }
  }
  sp_unreachable_return(SP_ERR);
}
