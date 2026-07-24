#include "fixture.h"

#define PARSE_MAX_TOOLCHAINS 2

typedef struct {
  spn_err_t err;
  u32 entries;
  fixture_toolchain_t toolchains [PARSE_MAX_TOOLCHAINS];
} parse_expect_t;

typedef struct {
  const c8* file;
  parse_expect_t expect;
} parse_test_t;

static void run_parse_test(s32* utest_result, parse_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_str_t json = sp_zero;
  fixture_read_json(utest_result, t.file, &json);

  spn_toolchain_catalog_t catalog = sp_zero;
  ASSERT_EQ((u32)t.expect.err, (u32)spn_toolchain_catalog_init(&catalog, json, mem));
  if (t.expect.err) {
    return;
  }

  ASSERT_EQ(t.expect.entries, fixture_catalog_size(&catalog));

  sp_carr_for(t.expect.toolchains, it) {
    fixture_toolchain_t toolchain = t.expect.toolchains[it];
    if (!toolchain.name) {
      break;
    }
    fixture_check_entry(utest_result, spn_toolchain_catalog_get(&catalog, sp_str_view(toolchain.name)), toolchain);
  }
}

UTEST(parse, distribution) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "distribution.json",
    .expect = {
      .entries = 1,
      .toolchains = {
        {
          .name = "A",
          .version = "1.0.0",
          .driver = SPN_CC_DRIVER_CLANG,
          .source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
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
            { SPN_ARCH_WASM32, SPN_OS_WASI },
            { .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL },
          },
        },
      },
    },
  });
}

UTEST(parse, local) {
  run_parse_test(utest_result, (parse_test_t) {
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
  });
}

UTEST(parse, multiple_toolchains) {
  run_parse_test(utest_result, (parse_test_t) {
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
  });
}

UTEST(parse, empty_document) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "empty.json",
    .expect = {
      .toolchains = {
        { .name = "A", .absent = true },
      },
    },
  });
}

UTEST(parse, malformed_json) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "malformed.json",
    .expect = { .err = SPN_ERROR },
  });
}
