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
  fixture_row_t rows [FIXTURE_MAX_TARGETS];
} rows_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  spn_triple_t host;
  const c8* toolchain;
  rows_expect_t expect;
} rows_test_t;

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  bool lld;
  spn_triple_t host;
  fixture_sdks_t sdks;
  fixture_target_t targets [FIXTURE_MAX_TARGETS];
  rows_expect_t expect;
} bind_test_t;

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
          .rows = { { HOST_X64_LINUX, .sanitizers = SAN_GCC_LINUX }, { TARGET_X64_BARE }, { TARGET_X64_LINUX_NONE } },
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

static const rows_test_t rows_tests [] = {
  {
    .name = "gcc_on_linux_targets_host_and_none",
    .file = "drivers.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .rows = { { HOST_X64_LINUX, .sanitizers = SAN_GCC_LINUX }, { TARGET_X64_BARE }, { TARGET_X64_LINUX_NONE } } },
  },
  {
    .name = "clang_on_linux_targets_host_and_its_arch_bare",
    .file = "multiple.json",
    .host = HOST_ARM_LINUX,
    .toolchain = "B",
    .expect = { .rows = { { HOST_ARM_LINUX, .sanitizers = SAN_CLANG_LINUX }, { TARGET_ARM_BARE }, { TARGET_ARM_LINUX_NONE } } },
  },
  {
    .name = "lld_does_not_add_arches",
    .file = "drivers.json",
    .host = HOST_ARM_LINUX,
    .toolchain = "B",
    .expect = { .rows = { { HOST_ARM_LINUX, .sanitizers = SAN_CLANG_LINUX }, { TARGET_ARM_BARE }, { TARGET_ARM_LINUX_NONE } } },
  },
  {
    .name = "gcc_on_macos_targets_host_only",
    .file = "drivers.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "A",
    .expect = { .rows = { { HOST_ARM_MACOS, .sanitizers = SAN_GCC_MACOS } } },
  },
  {
    .name = "clang_on_macos_targets_both_arches_and_no_elf",
    .file = "multiple.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "B",
    .expect = { .rows = { { HOST_ARM_MACOS, .sanitizers = SAN_CLANG_MACOS }, { HOST_X64_MACOS, .sanitizers = SAN_CLANG_MACOS } } },
  },
  {
    .name = "lld_on_macos_targets_no_elf",
    .file = "drivers.json",
    .host = HOST_ARM_MACOS,
    .toolchain = "B",
    .expect = { .rows = { { HOST_ARM_MACOS, .sanitizers = SAN_CLANG_MACOS }, { HOST_X64_MACOS, .sanitizers = SAN_CLANG_MACOS } } },
  },
  {
    .name = "gcc_on_windows_brings_its_libc",
    .file = "drivers.json",
    .host = HOST_X64_WINDOWS,
    .toolchain = "A",
    .expect = { .rows = { { TARGET_WIN_GNU } } },
  },
  {
    .name = "msvc_on_linux_targets_nothing",
    .file = "drivers.json",
    .host = HOST_X64_LINUX,
    .toolchain = "C",
  },
  {
    .name = "fixed_driver_keeps_its_list",
    .file = "auto.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .rows = { { TARGET_WIN_GNU } } },
  },
  {
    .name = "artifact_sdks_root_under_artifact",
    .file = "sdk.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = {
      .rows = {
        { HOST_ARM_LINUX, { SPN_SDK_SYSROOT, { "aa/S/linux", SPN_PATH_ROOT_TOOLCHAIN } } },
        { HOST_ARM_MACOS, { SPN_SDK_MACOS, { "aa/S/macos", SPN_PATH_ROOT_TOOLCHAIN } } },
        { TARGET_WASM, { SPN_SDK_SYSROOT, { "aa/S/wasi", SPN_PATH_ROOT_TOOLCHAIN } } },
        { TARGET_WIN_GNU, { SPN_SDK_SYSROOT, { "aa/S/windows", SPN_PATH_ROOT_TOOLCHAIN } } },
        { TARGET_WIN_MSVC, { SPN_SDK_MSVC, .vc = { "aa/S/msvc/crt/lib/x86_64", SPN_PATH_ROOT_TOOLCHAIN } } },
        { HOST_X64_LINUX },
        { TARGET_X64_BARE },
        { TARGET_X64_LINUX_NONE },
      },
    },
  },
  {
    .name = "local_sdks_are_kept",
    .file = "sdk_local.json",
    .host = HOST_X64_LINUX,
    .toolchain = "A",
    .expect = { .rows = { { HOST_ARM_LINUX, { SPN_SDK_SYSROOT, { "/S" } } }, { HOST_X64_LINUX }, { TARGET_X64_BARE }, { TARGET_X64_LINUX_NONE } } },
  },
};

static const bind_test_t bind_tests [] = {
  {
    .name = "entry_path_wins_over_host",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_ARM_MACOS,
    .sdks = { .macos = { "/H" } },
    .targets = { { HOST_ARM_MACOS, { "/E" } } },
    .expect = { .rows = { { HOST_ARM_MACOS, { SPN_SDK_MACOS, { "/E" } } } } },
  },
  {
    .name = "toolchain_source_takes_a_served_host_sdk",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_ARM_MACOS,
    .sdks = { .macos = { "/H" } },
    .targets = { { .triple = HOST_ARM_MACOS, .sdk_toolchain = true } },
    .expect = { .rows = { { HOST_ARM_MACOS, { SPN_SDK_MACOS, { "/H" } } } } },
  },
  {
    .name = "toolchain_source_without_a_host_sdk_is_none",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_X64_LINUX,
    .targets = { { .triple = HOST_ARM_MACOS, .sdk_toolchain = true } },
    .expect = { .rows = { { HOST_ARM_MACOS } } },
  },
  {
    .name = "host_source_takes_the_served_host_sdk",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_ARM_MACOS,
    .sdks = { .macos = { "/H" } },
    .targets = { { HOST_ARM_MACOS } },
    .expect = { .rows = { { HOST_ARM_MACOS, { SPN_SDK_MACOS, { "/H" } } } } },
  },
  {
    .name = "host_source_without_a_host_sdk_is_dropped",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_ARM_MACOS,
    .sdks = { .msvc = { { { "/X" }, SPN_ARCH_X64 } } },
    .targets = { { HOST_ARM_MACOS } },
  },
  {
    .name = "host_source_sysroot_must_be_the_host",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_X64_LINUX,
    .targets = { { HOST_X64_LINUX }, { HOST_X64_LINUX_MUSL }, { HOST_ARM_LINUX } },
    .expect = { .rows = { { HOST_X64_LINUX } } },
  },
  {
    .name = "retargeting_driver_reaches_served_sdks",
    .driver = SPN_CC_DRIVER_CLANG,
    .host = HOST_X64_LINUX,
    .sdks = { .macos = { "/H" }, .msvc = { { { "/X" }, SPN_ARCH_X64 } } },
    .targets = { { .triple = TARGET_WASM, .sdk_toolchain = true } },
    .expect = {
      .rows = {
        { TARGET_WASM },
        { HOST_X64_LINUX },
        { HOST_X64_MACOS, { SPN_SDK_MACOS, { "/H" } } },
        { HOST_ARM_MACOS, { SPN_SDK_MACOS, { "/H" } } },
        { TARGET_WIN_MSVC, { SPN_SDK_MSVC, .vc = { "/X/crt/lib/x86_64" } } },
        { TARGET_X64_BARE },
        { TARGET_X64_LINUX_NONE },
      },
    },
  },
  {
    .name = "fixed_driver_reaches_only_its_list",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_X64_LINUX,
    .sdks = { .macos = { "/H" }, .msvc = { { { "/X" }, SPN_ARCH_X64 } } },
    .targets = { { .triple = TARGET_WASM, .sdk_toolchain = true } },
    .expect = { .rows = { { TARGET_WASM } } },
  },
  {
    .name = "listed_rows_keep_their_sanitizers",
    .driver = SPN_CC_DRIVER_ZIG,
    .host = HOST_X64_LINUX,
    .targets = { { .triple = HOST_X64_LINUX, .sdk_toolchain = true, .sanitizers = SAN_ZIG_UT }, { .triple = TARGET_X64_BARE } },
    .expect = { .rows = { { HOST_X64_LINUX, .sanitizers = SAN_ZIG_UT }, { TARGET_X64_BARE }, { TARGET_X64_LINUX_NONE } } },
  },
  {
    .name = "coff_host_targets_no_bare",
    .driver = SPN_CC_DRIVER_CLANG,
    .lld = true,
    .host = HOST_X64_WINDOWS,
    .targets = { { .triple = TARGET_WIN_GNU, .sdk_toolchain = true } },
    .expect = { .rows = { { TARGET_WIN_GNU } } },
  },
  {
    .name = "lld_does_not_retarget_gcc",
    .driver = SPN_CC_DRIVER_GCC,
    .lld = true,
    .host = HOST_X64_LINUX,
    .expect = { .rows = { { HOST_X64_LINUX, .sanitizers = SAN_GCC_LINUX }, { TARGET_X64_BARE }, { TARGET_X64_LINUX_NONE } } },
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

sp_test_each(catalog, rows, rows_test_t, rows_tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file, it->host, sp_zero_struct(spn_sdk_host_t))) {
    return SP_ERR;
  }

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_cstr_as_str(it->toolchain));
  sp_must(t, info);
  return fixture_check_expected_rows(t, info->rows, it->expect.rows);
}

sp_test_each(catalog, bind, bind_test_t, bind_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toolchain_decl_t toolchain = fixture_local_toolchain("A", (fixture_launcher_t) { .name = "cc" });
  toolchain.driver = it->driver;
  toolchain.lld = it->lld;
  toolchain.targets = sp_da_new(mem, spn_toolchain_target_t);
  sp_carr_for(it->targets, at) {
    if (fixture_target_empty(it->targets[at])) {
      break;
    }
    sp_da_push(toolchain.targets, fixture_target(it->targets[at]));
  }

  spn_toolchain_catalog_t catalog = sp_zero;
  spn_toolchain_catalog_init(&catalog, it->host, fixture_sdks(mem, it->sdks), mem);
  spn_toolchain_catalog_add(&catalog, toolchain);

  spn_toolchain_info_t* info = spn_toolchain_catalog_get(&catalog, sp_str_lit("A"));
  sp_must(t, info);
  return fixture_check_expected_rows(t, info->rows, it->expect.rows);
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
