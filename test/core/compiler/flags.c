#include "compiler.h"

#define flags_max 6

typedef struct {
  const c8* compile [flags_max];
  const c8* link [flags_max];
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
      .mode = SPN_MODE_DEBUG,
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
      .mode = SPN_MODE_RELEASE,
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
      .mode = SPN_MODE_DEBUG,
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
      .mode = SPN_MODE_RELEASE,
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
      .mode = SPN_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
    },
    .driver = SPN_CC_DRIVER_MSVC,
    .expect = { .compile = { "/Z7", "/Od" } },
  },
  {
    .name = "freestanding_zig_strips_runtime",
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_FREESTANDING,
      .abi = SPN_ABI_BARE,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = { .compile = { "-ffreestanding", "-fno-stack-protector", "-fno-sanitize=undefined" }, .link = { "-nostartfiles", "-nolibc" } },
  },
  {
    .name = "freestanding_gcc_strips_runtime",
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_FREESTANDING,
      .abi = SPN_ABI_BARE,
    },
    .driver = SPN_CC_DRIVER_GCC,
    .expect = { .compile = { "-ffreestanding", "-fno-stack-protector" }, .link = { "-nostartfiles", "-nolibc" } },
  },
  {
    .name = "freestanding_clang_strips_runtime",
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_FREESTANDING,
      .abi = SPN_ABI_BARE,
    },
    .driver = SPN_CC_DRIVER_CLANG,
    .expect = { .compile = { "-ffreestanding", "-fno-stack-protector" }, .link = { "-nostartfiles", "-nolibc" } },
  },
  {
    .name = "freestanding_elf_strips_nothing",
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_FREESTANDING,
      .abi = SPN_ABI_ELF,
    },
    .driver = SPN_CC_DRIVER_GCC,
  },
  {
    .name = "linux_none_zig_strips_runtime",
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_BARE,
    },
    .driver = SPN_CC_DRIVER_ZIG,
    .expect = { .compile = { "-ffreestanding", "-fno-stack-protector", "-fno-sanitize=undefined" }, .link = { "-nostartfiles", "-nolibc" } },
  },
};

sp_test_each(render_flags, resolve, flags_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);

  spn_cc_flags_t flags = sp_zero;
  spn_cc_render_flags(mem, &toolchain, &it->profile, &flags);
  sp_must_strs_eq(t, flags.compile, sp_da_size(flags.compile), it->expect.compile);
  sp_must_strs_eq(t, flags.link, sp_da_size(flags.link), it->expect.link);
  return SP_OK;
}
