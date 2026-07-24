#include "fixture.h"

#define BUILTINS_MAX_HOSTS 6
#define BUILTINS_MAX_TARGETS 4

typedef struct {
  spn_cc_driver_t driver;
  spn_toolchain_source_t source;
  spn_triple_t hosts [BUILTINS_MAX_HOSTS];
  spn_triple_t targets [BUILTINS_MAX_TARGETS];
} builtins_expect_t;

typedef struct {
  const c8* name;
  builtins_expect_t expect;
} builtins_test_t;

static void builtins_catalog(s32* utest_result, spn_toolchain_catalog_t* catalog) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t path = sp_fs_join_path(mem, ctx_get_paths(ctx_get()).repo, sp_str_lit("source/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  ASSERT_EQ(SP_OK, sp_io_read_file(mem, path, &json));
  ASSERT_EQ(SPN_OK, spn_toolchain_catalog_init(catalog, json, mem));
}

static bool builtins_is_sha256(sp_str_t str) {
  if (str.len != 64) {
    return false;
  }
  sp_for(it, str.len) {
    c8 c = str.data[it];
    bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    if (!hex) {
      return false;
    }
  }
  return true;
}

static void run_builtins_test(s32* utest_result, builtins_test_t t) {
  spn_toolchain_catalog_t catalog = sp_zero;
  builtins_catalog(utest_result, &catalog);

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_str_view(t.name));
  ASSERT_TRUE(info);
  EXPECT_EQ((u32)t.expect.driver, (u32)info->driver);
  EXPECT_EQ((u32)t.expect.source, (u32)info->source);

  sp_carr_for(t.expect.hosts, it) {
    if (fixture_triple_empty(t.expect.hosts[it])) {
      break;
    }
    EXPECT_FALSE(sp_opt_is_null(spn_toolchain_select_artifact(info->hosts, t.expect.hosts[it])));
  }

  sp_carr_for(t.expect.targets, it) {
    if (fixture_triple_empty(t.expect.targets[it])) {
      break;
    }
    EXPECT_TRUE(fixture_has_target(info, t.expect.targets[it]));
  }
}

UTEST(builtins, well_formed) {
  spn_toolchain_catalog_t catalog = sp_zero;
  builtins_catalog(utest_result, &catalog);
  ASSERT_TRUE(fixture_catalog_size(&catalog));

  sp_str_ht_for_kv(catalog.entries, entry) {
    spn_toolchain_info_t* info = *entry.val;

    EXPECT_TRUE(sp_str_equal(info->name, *entry.key));
    EXPECT_FALSE(sp_str_empty(info->name));
    EXPECT_NE((u32)SPN_CC_DRIVER_NONE, (u32)info->driver);
    EXPECT_FALSE(sp_str_empty(info->compiler.program));
    EXPECT_FALSE(sp_str_empty(info->linker.program));
    EXPECT_FALSE(sp_str_empty(info->archiver.program));
    EXPECT_TRUE(spn_toolchain_has_cxx(info));

    if (info->source == SPN_TOOLCHAIN_SOURCE_DISTRIBUTION) {
      EXPECT_FALSE(sp_str_empty(info->version));
    }

    sp_da_for(info->hosts, it) {
      spn_toolchain_host_t host = info->hosts[it];
      EXPECT_NE((u32)SPN_ARCH_NONE, (u32)host.triple.arch);
      EXPECT_NE((u32)SPN_OS_NONE, (u32)host.triple.os);
      EXPECT_TRUE(sp_str_starts_with(host.artifact.url, sp_str_lit("https://")));
      EXPECT_TRUE(builtins_is_sha256(host.artifact.sha256));
      EXPECT_FALSE(sp_str_empty(host.artifact.mirror_list));
    }

    sp_da_for(info->targets, it) {
      EXPECT_FALSE(fixture_triple_empty(info->targets[it]));
    }
  }
}

UTEST(builtins, zig) {
  run_builtins_test(utest_result, (builtins_test_t) {
    .name = "zig",
    .expect = {
      .driver = SPN_CC_DRIVER_CLANG,
      .source = SPN_TOOLCHAIN_SOURCE_DISTRIBUTION,
      .hosts = {
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_X64, SPN_OS_MACOS },
        { SPN_ARCH_ARM64, SPN_OS_MACOS },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      },
      .targets = {
        { SPN_ARCH_WASM32, SPN_OS_WASI },
        { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
        { SPN_ARCH_ARM64, SPN_OS_MACOS },
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      },
    },
  });
}

UTEST(builtins, msvc) {
  run_builtins_test(utest_result, (builtins_test_t) {
    .name = "msvc",
    .expect = {
      .driver = SPN_CC_DRIVER_MSVC,
      .targets = {
        { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
        { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      },
    },
  });
}

UTEST(builtins, system) {
  run_builtins_test(utest_result, (builtins_test_t) {
    .name = "system",
    .expect = {
      .driver = SPN_CC_DRIVER_GCC,
    },
  });
}
