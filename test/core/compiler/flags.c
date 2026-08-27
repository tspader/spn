#include "compiler.h"

#define flags_max 6

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
    .name = "zig_driver_rejects_asan",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_MUSL,
      .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = { .unsupported = SPN_SANITIZER_ADDRESS },
  },
  {
    .name = "zig_driver_allows_ubsan_on_windows",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_GNU,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = {
      .compile = { "-g", "-O0", "-fsanitize=undefined", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer" },
      .link = { "-fsanitize=undefined" },
    },
  },
  {
    .name = "zig_driver_allows_tsan",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_MUSL,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_THREAD,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = {
      .compile = { "-g", "-O0", "-fsanitize=thread", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer" },
      .link = { "-fsanitize=thread" },
    },
  },
  {
    .name = "zig_driver_allows_tsan_on_macos",
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .abi = SPN_ABI_APPLE,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_THREAD,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = {
      .compile = { "-g", "-O0", "-fsanitize=thread", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer" },
      .link = { "-fsanitize=thread" },
    },
  },
  {
    .name = "zig_driver_rejects_tsan_on_windows",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_GNU,
      .sanitizers = SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = { .unsupported = SPN_SANITIZER_THREAD },
  },
  {
    .name = "zig_driver_allows_ubsan",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_MUSL,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
      .sanitizers = SPN_SANITIZER_UNDEFINED,
    },
    .driver = SPN_CC_DRIVER_ZIG,
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

sp_test_each(render_flags, resolve, flags_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);

  spn_cc_flags_t flags = sp_zero;
  spn_err_t err = spn_cc_render_flags(mem, &toolchain, &it->profile, &flags);

  if (it->expect.unsupported) {
    sp_expect_eq(t, err, it->expect.kind ? it->expect.kind : SPN_ERR_SANITIZER_UNSUPPORTED);
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, err);
    sp_expect_eq(t, errs[0].err.sanitizer.unsupported, it->expect.unsupported);
    sp_expect_eq(t, errs[0].err.sanitizer.target.arch, it->profile.arch);
    sp_expect_eq(t, errs[0].err.sanitizer.target.os, it->profile.os);
    sp_expect_eq(t, errs[0].err.sanitizer.target.abi, it->profile.abi);
    return SP_OK;
  }

  sp_expect_eq(t, err, SPN_OK);

  sp_must_strs_eq(t, flags.compile, sp_da_size(flags.compile), it->expect.compile);
  sp_must_strs_eq(t, flags.link, sp_da_size(flags.link), it->expect.link);

  return SP_OK;
}
