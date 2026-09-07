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
          .compiler = { .name = "A", .args = { "cc" } },
          .cxx = { .name = "A", .args = { "c++" } },
          .archiver = { .name = "A", .args = { "ar" } },
          .linkers = {
            [SPN_LD_FLAVOR_ELF] = SPN_LD_FAMILY_LLD,
            [SPN_LD_FLAVOR_MINGW] = SPN_LD_FAMILY_GNU,
            [SPN_LD_FLAVOR_MSVC] = SPN_LD_FAMILY_MSVC,
            [SPN_LD_FLAVOR_MACHO] = SPN_LD_FAMILY_LD64,
            [SPN_LD_FLAVOR_WASM] = SPN_LD_FAMILY_LLD,
          },
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
            { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL },
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
          .linkers = FAMILIES_NATIVE,
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
    .name = "linker_per_flavor",
    .file = "linkers.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .name = "A" },
          .archiver = { .name = "llvm-ar" },
          .linkers = FAMILIES_LLD,
          .targets = {
            { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
            TARGET_WIN_GNU,
            { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
            HOST_ARM_MACOS,
            TARGET_WASM,
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
