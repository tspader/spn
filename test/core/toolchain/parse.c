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
          .compiler = { .program = "A", .args = { "cc" } },
          .cxx = { .program = "A", .args = { "c++" } },
          .archiver = { .program = "A", .args = { "ar" } },
          .linkers = {
            [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD, "ld.lld" },
            [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
          },
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
          .compiler = { .program = "cc" },
          .cxx = { .program = "" },
          .archiver = { .program = "ar" },
          .linkers = {
            [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_GNU },
          },
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
          .compiler = { .program = "A" },
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
          .compiler = { .program = "B" },
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
          .compiler = { .program = "A" },
        },
        {
          .name = "B",
          .driver = SPN_CC_DRIVER_CLANG,
          .compiler = { .program = "B" },
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
          .compiler = { .program = "A" },
          .archiver = { .program = "llvm-ar" },
          .linkers = {
            [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD, "ld.lld" },
            [SPN_LD_FLAVOR_MINGW] = { SPN_LD_FAMILY_LLD, "ld.lld" },
            [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC },
            [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LLD, "ld64.lld" },
            [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
          },
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
    .name = "fixed_driver_fills_linkers",
    .file = "drivers.json",
    .expect = {
      .entries = 5,
      .toolchains = {
        {
          .name = "C",
          .driver = SPN_CC_DRIVER_MSVC,
          .compiler = { .program = "cl" },
          .linkers = {
            [SPN_LD_FLAVOR_MSVC] = { SPN_LD_FAMILY_MSVC },
          },
        },
        {
          .name = "D",
          .driver = SPN_CC_DRIVER_ZIG,
          .compiler = { .program = "D", .args = { "cc" } },
          .linkers = {
            [SPN_LD_FLAVOR_ELF] = { SPN_LD_FAMILY_LLD },
            [SPN_LD_FLAVOR_MINGW] = { SPN_LD_FAMILY_LLD },
            [SPN_LD_FLAVOR_MACHO] = { SPN_LD_FAMILY_LLD },
            [SPN_LD_FLAVOR_WASM] = { SPN_LD_FAMILY_LLD },
          },
        },
      },
    },
  },
  {
    .name = "linker_on_fixed_driver",
    .file = "linker_fixed.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_required",
    .file = "linker_none.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_family_missing",
    .file = "linker_family_missing.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_family_forbidden",
    .file = "linker_family_forbidden.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_program_forbidden",
    .file = "linker_program_forbidden.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "linker_program_missing",
    .file = "linker_program_missing.json",
    .expect = { .err = SPN_ERROR },
  },
  {
    .name = "target_without_linker",
    .file = "linker_target_unlinked.json",
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
