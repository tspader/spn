#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

#include "semver/compare.h"
#include "ctx/types.h"
#include "pkg/load.h"

UTEST_STATE();

spn_ctx_t spn;

// MAIN
int main(int argc, const char *const argv[]) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx);

  spn.intern = sp_intern_new();
  spn.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  s32 result = utest_main(argc, argv);

  ctx_deinit(ctx);
  return result;
}

// FIXTURE
struct load {};
UTEST_F_SETUP(load) {}
UTEST_F_TEARDOWN(load) {}

///////////////
// EXECUTOR //
//////////////
typedef struct {
  const c8* name;
  const c8* url;
  spn_triple_t hosts [16];
  spn_triple_t targets [16];
} toolchain_t;

typedef struct {
  const c8* manifest [64];
  const c8* namespace;
  const c8* name;
  spn_semver_t version;
  toolchain_t toolchains [4];
} test_t;

void run_case(s32* utest_result, struct load* utest_fixture, test_t test) {
  tmpfs_t* fs = &ctx_get()->fs;

  sp_str_builder_t builder = SP_ZERO_INITIALIZE();
  sp_for(it, sp_carr_len(test.manifest)) {
    const c8* line = test.manifest[it];
    if (!line) break;

    sp_str_builder_append(&builder, sp_str_view(line));
    sp_str_builder_new_line(&builder);
  }

  tmpfs_create(fs, sp_str_lit("spn.toml"),  sp_str_builder_to_str(&builder));

  sp_str_t path = tmpfs_get(fs, sp_str_lit("spn.toml"));
  spn_pkg_t pkg = SP_ZERO_INITIALIZE();
  spn_err_union_t err = spn_pkg_load(&pkg, path);

  if (test.namespace) SP_EXPECT_STR_EQ_CSTR(pkg.namespace, test.namespace);
  if (test.name) SP_EXPECT_STR_EQ_CSTR(pkg.name, test.name);
  if (!spn_semver_is_empty(test.version)) {
    EXPECT_EQ(test.version.major, pkg.version.major);
    EXPECT_EQ(test.version.minor, pkg.version.minor);
    EXPECT_EQ(test.version.patch, pkg.version.patch);
  }

  u32 num_toolchains = sp_om_size(pkg.toolchains);
  sp_carr_for(test.toolchains, it) {
    if (!test.toolchains[it].name) break;

    EXPECT_GT(num_toolchains, it);
    spn_toolchain_entry_t* toolchain = sp_om_at(pkg.toolchains, it);

    if (test.toolchains[it].url) SP_EXPECT_STR_EQ_CSTR(toolchain->info.url, test.toolchains[it].url);

    sp_carr_for(test.toolchains[it].targets, t) {
      spn_triple_t expected = test.toolchains[it].targets[t];
      if (expected.arch == SPN_ARCH_NONE) break;

      spn_triple_t actual = toolchain->info.targets[t];
      EXPECT_EQ(expected.arch, actual.arch);
      EXPECT_EQ(expected.os, actual.os);
      EXPECT_EQ(expected.abi, actual.abi);
    }
  }
}

UTEST_F(load, smoke) {
  run_case(utest_result, utest_fixture, (test_t) {
    .manifest = {
      "[package]",
      tkv(namespace, "spn"),
      tkv(name, "test"),
      tkv(version, "1.0.0")
    },
    .namespace = "spn",
    .name = "test",
    .version = spn_semver_lit(1, 0, 0)
  });
}

UTEST_F(load, basic_package) {
  run_case(utest_result, utest_fixture, (test_t) {
    .manifest = {
      "[package]",
      tkv(namespace, "core"),
      tkv(name, "zig"),
      tkv(version, "1.0.0"),
      "",
      "[[toolchain]]",
      tkv(name, "x86_64-linux"),
      tkv(url, "https://example.com/toolchain.tar.gz"),
      tkv(compiler, "cc"),
      tkv(linker, "ld"),
      tkv(archiver, "ar"),
      tkv(driver, "gcc"),
      tkv(export, true),
      ""
      "[[toolchain.host]]",
      tkv(arch, "x86_64"),
      tkv(os, "linux"),
      tkv(abi, "gnu"),
      ""
      "[[toolchain.host]]",
      tkv(arch, "x86_64"),
      tkv(os, "linux"),
      tkv(abi, "musl"),
      ""
      "[[toolchain.target]]",
      tkv(arch, "aarch64"),
      tkv(os, "linux"),
      tkv(abi, "gnu"),
      ""
      "[[toolchain.target]]",
      tkv(arch, "aarch64"),
      tkv(os, "macos"),
      ""
      "[[toolchain.target]]",
      tkv(arch, "x86_64"),
      tkv(os, "windows"),
      tkv(abi, "mingw"),
    },
    .toolchains = {
      {
        .name = "x86_64-w64-mingw32-cross",
        .url =  "https://musl.cc/x86_64-w64-mingw32-cross.tgz",
        .targets = {
          { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
          { SPN_ARCH_ARM64, SPN_OS_MACOS, },
          { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW },
        }
      }
    }
  });
}
