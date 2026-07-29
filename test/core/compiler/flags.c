#include "compiler.h"

#define flags_max 5

typedef struct {
  const c8* compile [flags_max];
  const c8* link [flags_max];
  spn_sanitizer_set_t unsupported;
  spn_err_t kind;
} flags_expect_t;

typedef struct {
  const c8* name;
  spn_profile_info_t profile;
  spn_cc_driver_t driver;
  const c8* toolchain;
  flags_expect_t expect;
} flags_test_t;

static const flags_test_t tests [] = {
  {
    .name = "render_gcc_debug",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
    },
    .driver = SPN_CC_DRIVER_GCC,
    .expect = { .compile = { "-g", "-O0" } },
  },
  {
    .name = "render_clang_release",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .mode = SPN_BUILD_MODE_RELEASE,
      .opt = SPN_OPT_LEVEL_2,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = { .compile = { "-O2", "-DNDEBUG" } },
  },
  {
    .name = "sanitize_both_lines",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = {
      .compile = { "-g", "-O0", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer" },
      .link = { "-fsanitize=address,undefined" },
    },
  },
  {
    .name = "sanitize_msvc",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .mode = SPN_BUILD_MODE_RELEASE,
      .opt = SPN_OPT_LEVEL_2,
      .sanitizers = SPN_SANITIZER_ADDRESS,
    },
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = {
      .compile = { "/O2", "/DNDEBUG", "/fsanitize=address" },
      .link = { "/fsanitize=address" },
    },
  },
  {
    .name = "render_msvc_debug",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
    },
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = { .compile = { "/Z7", "/Od" } },
  },
  {
    .name = "reject_msan_on_macos",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_MACOS,
      .sanitizers = SPN_SANITIZER_MEMORY,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = { .unsupported = SPN_SANITIZER_MEMORY },
  },
  {
    .name = "reject_asan_on_wasi",
    .profile = {
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
      .sanitizers = SPN_SANITIZER_ADDRESS,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = { .unsupported = SPN_SANITIZER_ADDRESS },
  },
  {
    .name = "reject_asan_on_zig_musl",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_MUSL,
      .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .toolchain = "zig",
    .expect = { .unsupported = SPN_SANITIZER_ADDRESS },
  },
  {
    .name = "allow_ubsan_on_zig_musl",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_MUSL,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .toolchain = "zig",
    .expect = {
      .compile = { "-g", "-O0", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer" },
      .link = { "-fsanitize=undefined" },
    },
  },
  {
    .name = "reject_sanitizers_with_static_linkage",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .linkage = SPN_LIB_KIND_STATIC,
      .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = {
      .kind = SPN_ERR_SANITIZER_STATIC,
      .unsupported = SPN_SANITIZER_ADDRESS,
    },
  },
};

sp_test_each(render_flags, resolve, flags_test_t, tests) {
  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);
  if (it->toolchain) {
    toolchain.name = sp_str_view(it->toolchain);
  }

  spn_cc_flags_t flags = sp_zero;
  spn_err_union_t err = spn_cc_render_flags(sp_test_arena(t), &toolchain, &it->profile, &flags);

  if (it->expect.unsupported) {
    sp_expect_eq(t, err.kind, it->expect.kind ? it->expect.kind : SPN_ERR_SANITIZER_UNSUPPORTED);
    sp_expect_eq(t, err.sanitizer.unsupported, it->expect.unsupported);
    sp_expect_eq(t, err.sanitizer.target.arch, it->profile.arch);
    sp_expect_eq(t, err.sanitizer.target.os, it->profile.os);
    sp_expect_eq(t, err.sanitizer.target.abi, it->profile.abi);
    return SP_OK;
  }

  sp_expect_eq(t, err.kind, SPN_OK);

  u32 compile = 0;
  sp_carr_detect_len(it->expect.compile, compile, it->expect.compile[compile]);
  u32 link = 0;
  sp_carr_detect_len(it->expect.link, link, it->expect.link[link]);

  sp_must_eq(t, compile, sp_da_size(flags.compile));
  sp_must_eq(t, link, sp_da_size(flags.link));
  sp_for(i, compile) {
    sp_expect_str_eq_c(t, flags.compile[i], it->expect.compile[i]);
  }
  sp_for(i, link) {
    sp_expect_str_eq_c(t, flags.link[i], it->expect.link[i]);
  }

  return SP_OK;
}
