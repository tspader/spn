#ifndef SPN_TEST_TOOLCHAIN_FIXTURE_H
#define SPN_TEST_TOOLCHAIN_FIXTURE_H

#include "sp.h"
#include "test.h"
#include "sha256/sha256.h"
#include "toolchain/toolchain.h"

#define EXPECT_STR(actual, cstr) EXPECT_TRUE(sp_str_equal((actual), sp_str_view(cstr)))

#define FIXTURE_MAX_ARGS 2
#define FIXTURE_MAX_HOSTS 2
#define FIXTURE_MAX_TARGETS 4

static const spn_triple_t HOST_X64_LINUX  = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU };
static const spn_triple_t HOST_ARM_LINUX  = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU };
static const spn_triple_t HOST_ARM_MACOS  = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE };
static const spn_triple_t TARGET_WIN_GNU  = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU };
static const spn_triple_t TARGET_WASM     = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_NONE };

typedef struct {
  const c8* program;
  const c8* args [FIXTURE_MAX_ARGS];
} fixture_launcher_t;

typedef struct {
  spn_triple_t triple;
  const c8* url;
  const c8* sha256;
  const c8* mirrors;
} fixture_host_t;

typedef struct {
  const c8* name;
  bool absent;
  const c8* version;
  spn_cc_driver_t driver;
  spn_toolchain_source_t source;
  fixture_launcher_t compiler;
  fixture_launcher_t cxx;
  fixture_launcher_t linker;
  fixture_launcher_t archiver;
  fixture_host_t hosts [FIXTURE_MAX_HOSTS];
  spn_triple_t targets [FIXTURE_MAX_TARGETS];
} fixture_toolchain_t;

static bool fixture_triple_empty(spn_triple_t triple) {
  return !triple.arch && !triple.os && !triple.abi;
}

static bool fixture_triple_equal(spn_triple_t a, spn_triple_t b) {
  return a.arch == b.arch && a.os == b.os && a.abi == b.abi;
}

static bool fixture_has_target(spn_toolchain_info_t* info, spn_triple_t target) {
  sp_da_for(info->targets, it) {
    if (fixture_triple_equal(info->targets[it], target)) {
      return true;
    }
  }
  return false;
}

static void fixture_check_launcher(s32* utest_result, spn_toolchain_launcher_t launcher, fixture_launcher_t expect) {
  if (!expect.program) {
    return;
  }

  EXPECT_STR(launcher.program, expect.program);

  u32 args = 0;
  sp_carr_for(expect.args, it) {
    if (!expect.args[it]) {
      break;
    }
    args++;
  }

  ASSERT_EQ(args, (u32)sp_da_size(launcher.args));
  sp_for(it, args) {
    EXPECT_STR(launcher.args[it], expect.args[it]);
  }
}

static void fixture_check_host(s32* utest_result, spn_toolchain_host_t host, fixture_host_t expect) {
  EXPECT_TRUE(fixture_triple_equal(host.triple, expect.triple));

  if (expect.url) {
    EXPECT_STR(host.artifact.url, expect.url);
  }
  if (expect.sha256) {
    EXPECT_STR(host.artifact.sha256, expect.sha256);
  }
  if (expect.mirrors) {
    EXPECT_STR(host.artifact.mirror_list, expect.mirrors);
  }
}

static void fixture_check_entry(s32* utest_result, spn_toolchain_info_t* info, fixture_toolchain_t expect) {
  if (expect.absent) {
    EXPECT_FALSE(info);
    return;
  }
  ASSERT_TRUE(info);

  if (expect.version) {
    EXPECT_STR(info->version, expect.version);
  }
  EXPECT_EQ((u32)expect.driver, (u32)info->driver);
  EXPECT_EQ((u32)expect.source, (u32)info->source);

  fixture_check_launcher(utest_result, info->compiler, expect.compiler);
  fixture_check_launcher(utest_result, info->cxx, expect.cxx);
  fixture_check_launcher(utest_result, info->linker, expect.linker);
  fixture_check_launcher(utest_result, info->archiver, expect.archiver);

  u32 hosts = 0;
  sp_carr_for(expect.hosts, it) {
    if (!expect.hosts[it].url) {
      break;
    }
    hosts++;
  }

  u32 targets = 0;
  sp_carr_for(expect.targets, it) {
    if (fixture_triple_empty(expect.targets[it])) {
      break;
    }
    targets++;
  }

  ASSERT_EQ(hosts, (u32)sp_da_size(info->hosts));
  ASSERT_EQ(targets, (u32)sp_da_size(info->targets));

  sp_for(it, hosts) {
    fixture_check_host(utest_result, info->hosts[it], expect.hosts[it]);
  }
  sp_for(it, targets) {
    EXPECT_TRUE(fixture_has_target(info, expect.targets[it]));
  }
}

static void fixture_read_json(s32* utest_result, const c8* file, sp_str_t* json) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);
  sp_str_t path = sp_fs_join_path(mem, sp_str_lit(TOOLCHAINS_DIR), sp_str_view(file));
  ASSERT_EQ(SP_OK, sp_io_read_file(mem, path, json));
}

static void fixture_catalog(s32* utest_result, spn_toolchain_catalog_t* catalog, const c8* file) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  sp_str_t json = sp_zero;
  fixture_read_json(utest_result, file, &json);
  ASSERT_EQ(SPN_OK, spn_toolchain_catalog_init(catalog, json, mem));
}

static u32 fixture_catalog_size(spn_toolchain_catalog_t* catalog) {
  return (u32)sp_str_ht_size(catalog->entries);
}

static spn_toolchain_info_t fixture_local_toolchain(const c8* name, const c8* compiler) {
  return (spn_toolchain_info_t) {
    .name = sp_str_view(name),
    .driver = SPN_CC_DRIVER_GCC,
    .compiler = { .program = sp_str_view(compiler) },
    .linker = { .program = sp_str_view(compiler) },
    .archiver = { .program = sp_str_view("ar") },
  };
}

#endif
