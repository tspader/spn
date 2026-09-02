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
          .linker = { .program = "A", .args = { "cc" } },
          .archiver = { .program = "A", .args = { "ar" } },
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
          .linker = { .program = "cc" },
          .archiver = { .program = "ar" },
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
};

sp_test_each(parse, catalog, parse_test_t, tests) {
  sp_str_t json = sp_zero;
  if (fixture_read_json(t, it->file, &json)) return SP_ERR;

  spn_toolchain_catalog_t catalog = sp_zero;
  spn_toolchain_catalog_init(&catalog, (spn_triple_t) HOST_X64_LINUX, sp_test_arena(t));
  sp_must_eq(t, (u32)it->expect.err, (u32)spn_toolchain_catalog_load(&catalog, json));
  if (it->expect.err) {
    return SP_OK;
  }

  sp_must_eq(t, it->expect.entries, fixture_catalog_size(&catalog));

  sp_carr_for(it->expect.toolchains, at) {
    fixture_toolchain_t toolchain = it->expect.toolchains[at];
    if (!toolchain.name) {
      break;
    }
    if (fixture_check_entry(t, spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(toolchain.name)), toolchain)) return SP_ERR;
  }

  return SP_OK;
}
