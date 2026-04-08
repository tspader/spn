#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

#include "semver/compare.h"
#include "ctx/types.h"
#include "pkg/load.h"
#include "profile/types.h"
#include "target/types.h"

///////////
// STATE //
///////////
UTEST_STATE();

spn_ctx_t spn;

//////////
// MAIN //
//////////
int main(int argc, const char *const argv[]) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx);

  spn.intern = sp_intern_new();
  spn.arena = sp_mem_arena_new_ex(256, SP_MEM_ARENA_MODE_NO_REALLOC, 1);

  s32 result = utest_main(argc, argv);

  ctx_deinit(ctx);
  return result;
}

///////////////
// EXECUTOR //
//////////////
typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
} profile_t;

typedef struct {
  const c8* name;
  const c8* url;
  spn_triple_t hosts [4];
  spn_triple_t targets [4];
} toolchain_t;

typedef struct {
  const c8* name;
  const c8* deps [4];
} target_t;

typedef struct {
  const c8* manifest [64];
  const c8* namespace;
  const c8* name;
  spn_semver_t version;
  toolchain_t toolchains [4];
  profile_t profiles [4];
  target_t exes [8];
  target_t libs [8];
} test_t;

void run_case(s32* utest_result, test_t test) {
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

  struct {
    struct { u32 actual; u32 expected; } toolchains;
    struct { u32 actual; u32 expected; } profiles;
    struct { u32 actual; u32 expected; } exes;
    struct { u32 actual; u32 expected; } libs;
    struct { u32 actual; u32 expected; } deps;
  } num = SP_ZERO_INITIALIZE();

  // Toolchains
  num.toolchains.actual = sp_om_size(pkg.toolchains);

  sp_carr_for(test.toolchains, it) {
    if (!test.toolchains[it].name) break;
    num.toolchains.expected++;
  }
  EXPECT_EQ(num.toolchains.expected, num.toolchains.actual);

  sp_for(it, num.toolchains.expected) {
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

  // Profiles
  num.profiles.actual = sp_om_size(pkg.profiles);

  sp_carr_for(test.profiles, it) {
    if (!test.profiles[it].name) break;
    num.profiles.expected++;
  }
  EXPECT_EQ(num.profiles.expected, num.profiles.actual);

  sp_for(it, num.profiles.expected) {
    profile_t expected = test.profiles[it];
    spn_profile_info_t* actual = sp_om_at(pkg.profiles, it);

    if (expected.name) SP_EXPECT_STR_EQ_CSTR(actual->name, expected.name);
    if (expected.toolchain) SP_EXPECT_STR_EQ_CSTR(actual->toolchain, expected.toolchain);
    EXPECT_EQ(actual->linkage, expected.linkage);
    if (expected.standard) EXPECT_EQ(actual->standard, expected.standard);
    if (expected.mode) EXPECT_EQ(actual->mode, expected.mode);
  }

  // Executables
  num.exes.actual = sp_om_size(pkg.exes);

  sp_carr_for(test.exes, it) {
    if (!test.exes[it].name) break;
    num.exes.expected++;
  }
  EXPECT_EQ(num.exes.expected, num.exes.actual);

  sp_for(it, num.exes.expected) {
    spn_target_t* target = sp_om_at(pkg.exes, it);

    SP_EXPECT_STR_EQ_CSTR(target->name, test.exes[it].name);

    num.deps.expected = 0;
    sp_carr_for(test.exes[it].deps, j) {
      if (!test.exes[it].deps[j]) break;
      num.deps.expected++;
    }
    num.deps.actual = sp_da_size(target->deps);
    ASSERT_EQ(num.deps.expected, num.deps.actual);

    sp_carr_for(test.exes[it].deps, j) {
      const c8* expected = test.exes[it].deps[j];
      if (!expected) break;

      SP_EXPECT_STR_EQ_CSTR(target->deps[j], expected);
    }
  }

  // Libs
  num.libs.actual = sp_om_size(pkg.libs);

  sp_carr_for(test.libs, it) {
    if (!test.libs[it].name) break;
    num.libs.expected++;
  }
  EXPECT_EQ(num.libs.expected, num.libs.actual);

  sp_for(it, num.libs.expected) {
    spn_target_t* target = sp_om_at(pkg.libs, it);
    SP_EXPECT_STR_EQ_CSTR(target->name, test.libs[it].name);
  }
}

UTEST(load, smoke) {
  run_case(utest_result, (test_t) {
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

UTEST(load, toolchain) {
  run_case(utest_result, (test_t) {
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
      "",
      "[[toolchain.host]]",
      tkv(arch, "x86_64"),
      tkv(os, "linux"),
      tkv(abi, "gnu"),
      "",
      "[[toolchain.host]]",
      tkv(arch, "x86_64"),
      tkv(os, "linux"),
      tkv(abi, "musl"),
      "",
      "[[toolchain.target]]",
      tkv(arch, "aarch64"),
      tkv(os, "linux"),
      tkv(abi, "gnu"),
      "",
      "[[toolchain.target]]",
      tkv(arch, "aarch64"),
      tkv(os, "macos"),
      "",
      "[[toolchain.target]]",
      tkv(arch, "x86_64"),
      tkv(os, "windows"),
      tkv(abi, "mingw"),
    },
    .toolchains = {
      {
        .name = "x86_64-w64-mingw32-cross",
        .url =  "https://example.com/toolchain.tar.gz",
        .hosts = {
          { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
          { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
        },
        .targets = {
          { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
          { SPN_ARCH_ARM64, SPN_OS_MACOS, },
          { SPN_ARCH_X64,   SPN_OS_WINDOWS, SPN_ABI_MINGW },
        }
      }
    }
  });
}

UTEST(load, profile) {
  run_case(utest_result, (test_t) {
    .manifest = {
      "[package]",
      tkv(name, "spum"),
      tkv(version, "1.0.0"),
      "",
      "[profile.default]",
      tkv(toolchain, "zig"),
      tkv(linkage, "static"),
      tkv(standard, "c99"),
      tkv(mode, "debug"),
      "",
      "[profile.shared]",
      tkv(linkage, "shared"),
    },
    .profiles = {
      { "default", "zig", SPN_LIB_KIND_STATIC, SPN_C99, SPN_BUILD_MODE_DEBUG },
      { "shared", "", SPN_LIB_KIND_SHARED, SPN_C_STANDARD_NONE, SPN_BUILD_MODE_NONE },
    },
  });
}

UTEST(load, target_deps) {
  run_case(utest_result, (test_t) {
    .manifest = {
      "[package]",
      tkv(name, "test"),
      tkv(version, "1.0.0"),
      "",
      "[[lib]]",
      tkv(name, "spum"),
      tk(kinds) "["
        q("static")
      "]",
      tk(source) "["
        q("spum.c")
      "]",
      "",
      "[[bin]]",
      tkv(name, "main"),
      tk(source) "["
        q("main.c")
      "]",
      tk(deps) "["
        q("spum")
      "]",
    },
    .name = "test",
    .exes = {
      { .name = "main", .deps = { "spum" } },
    },
    .libs = {
      { .name = "spum" },
    },
  });
}

