#include "toolchain.h"

#define PARSE_MAX_TOOLCHAINS 2

typedef struct {
  spn_err_t err;
  u32 entries;
  fixture_toolchain_t toolchains [PARSE_MAX_TOOLCHAINS];
} parse_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  parse_expect_t expect;
} parse_test_t;

static const parse_test_t tests [] = {
  {
    .name = "distribution",
    .file = "distribution.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .version = "1.0.0",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .path = "A", .args = { "cc" } },
          .cxx = { .path = "A", .args = { "c++" } },
          .archiver = { .path = "A", .args = { "ar" } },
          .lld = true,
          .link_args = { "-fuse-ld=lld" },
          .hosts = {
            {
              .triple = { SPN_ARCH_X64, SPN_OS_LINUX },
              .url = "https://example.com/linux.tar.xz",
              .sha256 = "aa",
              .mirrors = "https://example.com/mirrors.txt",
            },
            {
              .triple = { SPN_ARCH_ARM64, SPN_OS_MACOS },
              .url = "https://example.com/macos.tar.xz",
              .sha256 = "bb",
              .mirrors = "https://example.com/mirrors.txt",
            },
          },
          .targets = {
            { .triple = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL }, .sdk_toolchain = true },
            { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
          },
        },
      },
    },
  },
  {
    .name = "local",
    .file = "local.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .version = "",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .name = "cc" },
          .cxx = { .name = "" },
          .archiver = { .name = "ar" },
        },
      },
    },
  },
  {
    .name = "host_restricted_local",
    .file = "restricted.json",
    .expect = {
      .entries = 2,
      .toolchains = {
        {
          .name = "A",
          .version = "",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .name = "A" },
          .hosts = {
            {
              .triple = { SPN_ARCH_X64, SPN_OS_LINUX },
              .url = "",
              .sha256 = "",
            },
          },
        },
        {
          .name = "B",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .name = "B" },
        },
      },
    },
  },
  {
    .name = "multiple_toolchains",
    .file = "multiple.json",
    .expect = {
      .entries = 2,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .name = "A" },
        },
        {
          .name = "B",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "B" },
        },
      },
    },
  },
  {
    .name = "empty_document",
    .file = "empty.json",
    .expect = {
      .toolchains = {
        { .name = "A", .absent = true },
      },
    },
  },
  {
    .name = "malformed_json",
    .file = "malformed.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "invalid_host_key",
    .file = "bad_host.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "mixed_hosts",
    .file = "mixed.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_rejected",
    .file = "linker_rejected.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "target_beyond_driver",
    .file = "bad_target_driver.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_lld",
    .file = "linkers.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "A" },
          .archiver = { .name = "llvm-ar" },
          .lld = true,
          .targets = {
            { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
            { .triple = TARGET_WIN_GNU, .sdk_toolchain = true },
            { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
            HOST_ARM_MACOS,
            { .triple = TARGET_WASM, .sdk_toolchain = true },
          },
        },
      },
    },
  },
  {
    .name = "program_absolute",
    .file = "program_absolute.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .path = "/A" },
          .archiver = { .name = "ar" },
        },
      },
    },
  },
  {
    .name = "program_relative",
    .file = "program_relative.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_per_target",
    .file = "sdk.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .path = "A" },
          .archiver = { .path = "A" },
          .hosts = {
            { .triple = { SPN_ARCH_X64, SPN_OS_LINUX }, .url = "https://example.com/linux.tar.xz", .sha256 = "aa" },
          },
          .targets = {
            { .triple = HOST_ARM_LINUX, .sdk = { "S/linux" } },
            { .triple = HOST_ARM_MACOS, .sdk = { "S/macos" } },
            { .triple = TARGET_WASM, .sdk = { "S/wasi" } },
            { .triple = TARGET_WIN_GNU, .sdk = { "S/windows" } },
            { .triple = TARGET_WIN_MSVC, .sdk = { "S/msvc" } },
            { .triple = HOST_X64_LINUX },
          },
        },
      },
    },
  },
  {
    .name = "sdk_on_none_target",
    .file = "sdk_none.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_on_elf_target",
    .file = "sdk_elf.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "A" },
          .archiver = { .name = "A" },
          .targets = {
            { .triple = { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_ELF }, .sdk = { "/S" } },
          },
        },
      },
    },
  },
  {
    .name = "sdk_absolute_in_distribution",
    .file = "sdk_absolute.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sanitizers_and_toolchain_sdk",
    .file = "caps.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "A" },
          .archiver = { .name = "A" },
          .targets = {
            { .triple = HOST_X64_LINUX, .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED },
            { .triple = TARGET_WASM, .sdk_toolchain = true },
            { .triple = TARGET_WIN_GNU, .sdk_toolchain = true },
          },
        },
      },
    },
  },
  {
    .name = "sanitizers_on_none_target",
    .file = "sanitizers_none.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_required_off_host",
    .file = "sdk_required.json",
    .expect = { .err = SPN_ERROR },
  },
};

sp_test_each(parse, decls, parse_test_t, tests) {
  sp_str_t json = sp_zero;
  if (fixture_read_json(t, it->file, &json)) return SP_ERR;

  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  sp_must_eq(t, (u32)it->expect.err, (u32)spn_toolchain_decls_parse(sp_test_arena(t), json, &decls));
  if (it->expect.err) {
    return SP_OK;
  }

  sp_must_eq(t, it->expect.entries, (u32)sp_da_size(decls));

  sp_carr_for(it->expect.toolchains, at) {
    fixture_toolchain_t toolchain = it->expect.toolchains[at];
    if (!toolchain.name) {
      break;
    }
    if (fixture_check_decl(t, fixture_decl(decls, toolchain.name), toolchain)) return SP_ERR;
  }

  return SP_OK;
}
