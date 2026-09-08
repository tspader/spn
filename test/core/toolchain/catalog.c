#include "toolchain.h"

#define CATALOG_MAX_ADDS 2
#define CATALOG_MAX_ORDER 4
#define CATALOG_MAX_TOOLCHAINS 2

typedef struct {
  const c8* name;
  fixture_launcher_t compiler;
} add_t;

typedef struct {
  const c8* order [CATALOG_MAX_ORDER];
  fixture_toolchain_t toolchains [CATALOG_MAX_TOOLCHAINS];
} add_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  add_t adds [CATALOG_MAX_ADDS];
  add_expect_t expect;
} add_test_t;

typedef struct {
  fixture_target_t targets [FIXTURE_MAX_TARGETS];
} targets_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  spn_triple_t host;
  const c8* toolchain;
  targets_expect_t expect;
} targets_test_t;

typedef struct {
  spn_toolchain_support_kind_t kind;
  const c8* artifact;
} support_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  spn_triple_t host;
  const c8* toolchain;
  support_expect_t expect;
} support_test_t;

static const add_test_t add_tests [] = {
  {
    .name = "overrides_by_name",
    .file = "multiple.json",
    .adds = {
      { .name = "A", .compiler = { .path = "/A" } },
    },
    .expect = {
      .order = { "A", "B" },
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .path = "/A" },
          .targets = { HOST_X64_LINUX, TARGET_X64_BARE },
        },
      },
    },
  },
  {
    .name = "coexists_with_entries",
    .file = "multiple.json",
    .adds = {
      { .name = "C", .compiler = { .name = "C" } },
    },
    .expect = {
      .order = { "A", "B", "C" },
      .toolchains = {
        { .name = "D", .absent = true },
      },
    },
  },
};

static const targets_test_t targets_tests [] = {
  {
    .name = "gcc_on_linux_targets_host_and_bare_metal",
    .file = "drivers.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .targets = { HOST_X64_LINUX, TARGET_X64_BARE } },
  },
  {
    .name = "clang_on_linux_targets_host_only",
    .file = "drivers.json",
    .host = HOST_ARM_LINUX,
    .toolchain = "B",
    .expect = { .targets = { HOST_ARM_LINUX } },
  },
  {
    .name = "gcc_on_macos_targets_host_only",
    .file = "drivers.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "A",
    .expect = { .targets = { HOST_ARM_MACOS } },
  },
  {
    .name = "msvc_on_linux_targets_nothing",
    .file = "drivers.json",
    .host = HOST_X64_LINUX,
    .toolchain = "C",
  },
  {
    .name = "declared_targets_are_kept",
    .file = "auto.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .targets = { TARGET_WIN_GNU } },
  },
  {
    .name = "artifact_sdks_root_under_artifact",
    .file = "sdk.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = {
      .targets = {
        { .triple = HOST_ARM_LINUX, .sdk = { "aa/S/linux", SPN_PATH_ROOT_TOOLCHAIN } },
        { .triple = HOST_ARM_MACOS, .sdk = { "aa/S/macos", SPN_PATH_ROOT_TOOLCHAIN } },
        { .triple = TARGET_WASM, .sdk = { "aa/S/wasi", SPN_PATH_ROOT_TOOLCHAIN } },
        { .triple = TARGET_WIN_GNU, .sdk = { "aa/S/windows", SPN_PATH_ROOT_TOOLCHAIN } },
        { .triple = TARGET_WIN_MSVC, .sdk = { "aa/S/msvc", SPN_PATH_ROOT_TOOLCHAIN } },
        { .triple = HOST_X64_LINUX },
      },
    },
  },
  {
    .name = "local_sdks_are_kept",
    .file = "sdk_local.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .targets = { { .triple = HOST_ARM_LINUX, .sdk = { "/S" } } } },
  },
};

static const support_test_t support_tests [] = {
  {
    .name = "local_without_hosts_supports_any_host",
    .file = "local.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_LOCAL },
  },
  {
    .name = "local_with_hosts_supports_listed_host",
    .file = "restricted.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_LOCAL },
  },
  {
    .name = "local_with_hosts_rejects_other_host",
    .file = "restricted.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_NONE },
  },
  {
    .name = "distribution_resolves_host_artifact",
    .file = "distribution.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_ARTIFACT, .artifact = "https://example.com/linux.tar.xz" },
  },
  {
    .name = "distribution_resolves_per_host",
    .file = "distribution.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_ARTIFACT, .artifact = "https://example.com/macos.tar.xz" },
  },
  {
    .name = "distribution_ignores_host_abi",
    .file = "distribution.json",
    .host = HOST_X64_LINUX_MUSL,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_ARTIFACT, .artifact = "https://example.com/linux.tar.xz" },
  },
  {
    .name = "distribution_rejects_unlisted_host",
    .file = "distribution.json",
    .host = HOST_ARM_LINUX,
    .toolchain = "A",
    .expect = { .kind = SPN_TOOLCHAIN_SUPPORT_NONE },
  },
};

sp_test_each(catalog, add, add_test_t, add_tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file, (spn_triple_t) HOST_X64_LINUX, sp_zero_struct(spn_sdk_host_t))) {
    return SP_ERR;
  }

  u32 adds = 0;
  sp_carr_detect_len(it->adds, adds, it->adds[adds].name);
  sp_for(at, adds) {
    spn_toolchain_catalog_add(&catalog, fixture_local_toolchain(it->adds[at].name, it->adds[at].compiler));
  }

  u32 order = 0;
  sp_carr_detect_len(it->expect.order, order, it->expect.order[order]);
  sp_must_eq(t, order, fixture_catalog_size(&catalog));
  sp_for(at, order) {
    sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, at)->name, it->expect.order[at]);
  }

  u32 toolchains = 0;
  sp_carr_detect_len(it->expect.toolchains, toolchains, it->expect.toolchains[toolchains].name);
  sp_for(at, toolchains) {
    fixture_toolchain_t toolchain = it->expect.toolchains[at];
    if (fixture_check_entry(t, spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(toolchain.name)), toolchain)) {
      return SP_ERR;
    }
  }

  return SP_OK;
}

sp_test_each(catalog, targets, targets_test_t, targets_tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file, it->host, sp_zero_struct(spn_sdk_host_t))) {
    return SP_ERR;
  }

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(it->toolchain));
  sp_must(t, info);

  u32 targets = 0;
  sp_carr_detect_len(it->expect.targets, targets, !fixture_target_empty(it->expect.targets[targets]));
  return fixture_check_targets(t, info->targets, it->expect.targets, targets);
}

sp_test_each(catalog, support, support_test_t, support_tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file, it->host, sp_zero_struct(spn_sdk_host_t))) {
    return SP_ERR;
  }

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(it->toolchain));
  sp_must(t, info);

  sp_expect_eq(t, (u32)it->expect.kind, (u32)info->support.kind);
  sp_expect_str_eq_c(t, info->support.artifact.url, it->expect.artifact ? it->expect.artifact : "");
  return SP_OK;
}
