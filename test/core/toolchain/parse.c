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
    .file = "distribution.toml",
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
            { .triple = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL } },
            { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
          },
        },
      },
    },
  },
  {
    .name = "local",
    .file = "local.toml",
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
    .file = "restricted.toml",
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
    .file = "multiple.toml",
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
    .file = "empty.toml",
    .expect = {
      .toolchains = {
        { .name = "A", .absent = true },
      },
    },
  },
  {
    .name = "malformed_toml",
    .file = "malformed.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "invalid_host_key",
    .file = "bad_host.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "mixed_hosts",
    .file = "mixed.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_rejected",
    .file = "linker_rejected.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "target_beyond_driver",
    .file = "bad_target_driver.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_lld",
    .file = "linkers.toml",
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
            { .triple = TARGET_WIN_GNU },
            { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
            HOST_ARM_MACOS,
            { .triple = TARGET_WASM },
          },
        },
      },
    },
  },
  {
    .name = "program_absolute",
    .file = "program_absolute.toml",
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
    .file = "program_relative.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_per_target",
    .file = "sdk.toml",
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
    .file = "sdk_none.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_on_elf_target",
    .file = "sdk_elf.toml",
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
    .name = "sdk_absolute_in_distribution_is_a_host_path",
    .file = "sdk_absolute.toml",
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
            { .triple = HOST_ARM_LINUX, .sdk = { "/S" } },
          },
        },
      },
    },
  },
  {
    .name = "sanitizers_and_toolchain_sdk",
    .file = "caps.toml",
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
            { .triple = TARGET_WASM },
            { .triple = TARGET_WIN_GNU },
          },
        },
      },
    },
  },
  {
    .name = "sanitizers_on_none_target",
    .file = "sanitizers_none.toml",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "sdk_absent_off_host_is_the_toolchains",
    .file = "sdk_absent.toml",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "A" },
          .archiver = { .name = "A" },
          .targets = { { TARGET_WIN_GNU } },
        },
      },
    },
  },
};

sp_test_each(parse, decls, parse_test_t, tests) {
  sp_da(spn_toolchain_decl_t) decls = SP_NULLPTR;
  sp_da(spn_codegen_issue_t) issues = SP_NULLPTR;
  if (fixture_decls(t, it->file, &decls, &issues)) return SP_ERR;

  sp_must_eq(t, it->expect.err != SPN_OK, !sp_da_empty(issues));
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
