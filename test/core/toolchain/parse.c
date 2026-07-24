#include "sp.h"
#include "utest.h"

#include "fixture.h"

#define PARSE_MAX_TOOLCHAINS 2

typedef struct {
  const c8* file;
  spn_err_t err;
  u32 entries;
  fixture_toolchain_t toolchains [PARSE_MAX_TOOLCHAINS];
} parse_test_t;

static void run_parse_test(s32* utest_result, parse_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_str_t json = sp_zero;
  fixture_read_json(utest_result, t.file, &json);

  spn_toolchain_catalog_t catalog = sp_zero;
  ASSERT_EQ((u32)t.err, (u32)spn_toolchain_catalog_init(&catalog, json, mem));
  if (t.err) {
    return;
  }

  ASSERT_EQ(t.entries, fixture_catalog_size(&catalog));

  sp_carr_for(t.toolchains, it) {
    fixture_toolchain_t toolchain = t.toolchains[it];
    if (!toolchain.name) {
      break;
    }
    fixture_check_entry(utest_result, spn_toolchain_catalog_get(&catalog, sp_str_view(toolchain.name)), toolchain);
  }
}

UTEST(parse, distribution) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "distribution.json",
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
          { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
        },
      },
    },
  });
}

UTEST(parse, local) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "local.json",
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
  });
}

UTEST(parse, multiple_toolchains) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "multiple.json",
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
  });
}

UTEST(parse, duplicate_name_keeps_first) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "duplicate_name.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .compiler = { .program = "A" },
      },
    },
  });
}

UTEST(parse, empty_document) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "empty.json",
    .toolchains = {
      { .name = "A", .absent = true },
    },
  });
}

UTEST(parse, root_not_object) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "root_not_object.json",
  });
}

UTEST(parse, malformed_json) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "malformed.json",
    .err = SPN_ERROR,
  });
}

UTEST(parse, unknown_driver) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "unknown_driver.json",
    .entries = 1,
    .toolchains = {
      { .name = "A", .compiler = { .program = "A" } },
    },
  });
}

UTEST(parse, unknown_host_key) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "unknown_host_key.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
        .hosts = {
          { .url = "https://example.com/A.tar.xz", .sha256 = "aa" },
        },
      },
    },
  });
}

UTEST(parse, host_without_sha) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "host_no_sha.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
        .hosts = {
          {
            .triple = { SPN_ARCH_X64, SPN_OS_LINUX },
            .url = "https://example.com/A.tar.xz",
            .sha256 = "",
          },
        },
      },
    },
  });
}

UTEST(parse, host_not_object) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "host_not_object.json",
    .entries = 1,
    .toolchains = {
      { .name = "A", .driver = SPN_CC_DRIVER_GCC },
    },
  });
}

UTEST(parse, partial_target) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "partial_target.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .targets = {
          { .os = SPN_OS_LINUX },
        },
      },
    },
  });
}

UTEST(parse, target_not_object) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "target_not_object.json",
    .entries = 1,
    .toolchains = {
      { .name = "A", .driver = SPN_CC_DRIVER_GCC },
    },
  });
}

UTEST(parse, non_string_args_dropped) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "args_non_string.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .compiler = { .program = "A", .args = { "cc" } },
      },
    },
  });
}

UTEST(parse, missing_fields_are_empty) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "missing_required.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .version = "",
        .compiler = { .program = "" },
        .cxx = { .program = "" },
        .linker = { .program = "" },
        .archiver = { .program = "" },
      },
    },
  });
}

UTEST(parse, unknown_fields_ignored) {
  run_parse_test(utest_result, (parse_test_t) {
    .file = "unknown_field.json",
    .entries = 1,
    .toolchains = {
      {
        .name = "A",
        .driver = SPN_CC_DRIVER_GCC,
        .compiler = { .program = "A" },
      },
    },
  });
}
