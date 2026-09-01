#include "spn_test.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "profile/profile.h"
#include "pkg/types.h"
#include "toolchain/catalog.h"
#include "toolchain/select.h"

sp_test_suite(profile, .serial = true);

#define PROFILE_HOST_LINUX_GNU  { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define PROFILE_HOST_LINUX_MUSL { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define PROFILE_HOST_WIN_MSVC   { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define PROFILE_HOST_WIN_GNU    { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define PROFILE_HOST_ARM_MACOS  { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE }

typedef struct {
  const c8* name;
  const c8* toolchain;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  spn_linkage_t linkage;
} profile_desc_t;

typedef struct {
  spn_err_t err;
  spn_triple_t target;
  spn_linkage_t linkage;
  const c8* toolchain;
  const c8* selected;
  bool targeted;
} expect_t;

typedef struct {
  const c8* name;
  profile_desc_t profile;
  profile_desc_t derived;
  profile_desc_t overrides;
  spn_triple_t host;
  bool shared_demand;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "default_is_musl_static",
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "auto",
      .selected = "zig",
    },
  },
  {
    .name = "shared_linkage_defaults_to_gnu",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_SHARED },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "static_linkage_defaults_to_musl",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_STATIC },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "shared_demand_defaults_to_gnu",
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "shared_demand_on_musl_host_prefers_musl",
    .host = PROFILE_HOST_LINUX_MUSL,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "explicit_static_ignores_shared_demand",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_STATIC },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "shared_demand_survives_pinned_abi",
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "explicit_gnu_defaults_to_shared",
    .overrides = { .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "explicit_musl_defaults_to_static",
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "explicit_musl_shared_is_honored",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_SHARED },
    .overrides = { .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "explicit_gnu_static_is_honored",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_STATIC },
    .overrides = { .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "windows_host_defaults_to_gnu",
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .selected = "zig",
    },
  },
  {
    .name = "windows_host_shared_demand_defaults_to_gnu",
    .host = PROFILE_HOST_WIN_MSVC,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "macos_host_uses_apple_abi",
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "wasi_target_uses_musl_abi",
    .overrides = { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "freestanding_target_defaults_to_static",
    .overrides = { .os = SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_BARE },
      .linkage = SPN_LIB_KIND_STATIC,
      .selected = "zig",
      .targeted = true,
    },
  },
  {
    .name = "freestanding_ignores_shared_demand",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "macos_target_uses_apple_abi",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "cross_windows_requires_abi",
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_TARGET_ABI },
  },
  {
    .name = "cross_windows_with_abi",
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "cross_linux_requires_abi",
    .overrides = { .os = SPN_OS_LINUX },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = { .err = SPN_ERR_TARGET_ABI },
  },
  {
    .name = "cross_arch_requires_abi",
    .overrides = { .arch = SPN_ARCH_ARM64 },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_TARGET_ABI },
  },
  {
    .name = "cross_arch_with_abi",
    .overrides = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "cross_linux_with_abi",
    .overrides = { .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "explicit_abi_wins",
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .selected = "msvc",
      .targeted = true,
    },
  },
  {
    .name = "msvc_completes_abi_from_target_list",
    .overrides = { .toolchain = "msvc" },
    .host = PROFILE_HOST_WIN_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "msvc",
      .selected = "msvc",
    },
  },
  {
    .name = "msvc_cant_target_linux_host",
    .overrides = { .toolchain = "msvc" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET },
  },
  {
    .name = "host_only_toolchain_completes_to_host",
    .profile = { .name = "default", .toolchain = "gcc" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "gcc",
      .selected = "gcc",
    },
  },
  {
    .name = "host_only_toolchain_rejects_cross",
    .profile = { .name = "default", .toolchain = "gcc" },
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET },
  },
  {
    .name = "override_toolchain_wins",
    .profile = { .name = "default", .toolchain = "gcc" },
    .overrides = { .toolchain = "clang" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
      .selected = "clang",
    },
  },
  {
    .name = "manifest_abi_applies_to_host",
    .profile = { .name = "default", .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "os_override_drops_manifest_abi",
    .profile = { .name = "default", .abi = SPN_ABI_GNU },
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "os_override_drops_manifest_abi_and_requires_one",
    .profile = { .name = "default", .abi = SPN_ABI_MUSL },
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_TARGET_ABI },
  },
  {
    .name = "arch_override_keeps_manifest_abi",
    .profile = { .name = "default", .abi = SPN_ABI_GNU },
    .overrides = { .arch = SPN_ARCH_ARM64 },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "override_os_with_abi_is_honored",
    .profile = { .name = "default", .abi = SPN_ABI_MUSL },
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
    .host = PROFILE_HOST_WIN_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .selected = "msvc",
      .targeted = true,
    },
  },
  {
    .name = "derived_os_drops_base_abi",
    .profile = { .name = "default", .abi = SPN_ABI_GNU },
    .derived = { .name = "mac", .os = SPN_OS_MACOS },
    .overrides = { .name = "mac" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_APPLE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "derived_abi_overlays_base_os",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS },
    .derived = { .name = "msvc", .abi = SPN_ABI_MSVC },
    .overrides = { .name = "msvc" },
    .host = PROFILE_HOST_WIN_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "undefined_profile",
    .overrides = { .name = "missing" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_UNDEFINED },
  },
};

static spn_profile_info_t desc_to_info(const profile_desc_t* d) {
  return (spn_profile_info_t) {
    .name = d->name ? sp_str_view(d->name) : (sp_str_t) sp_zero,
    .toolchain = d->toolchain ? sp_str_view(d->toolchain) : (sp_str_t) sp_zero,
    .os = d->os,
    .arch = d->arch,
    .abi = d->abi,
    .linkage = d->linkage,
  };
}

static spn_profile_override_t desc_to_override(const profile_desc_t* d) {
  return (spn_profile_override_t) {
    .name = d->name ? sp_cstr_as_str(d->name) : (sp_str_t) sp_zero,
    .toolchain = d->toolchain ? sp_cstr_as_str(d->toolchain) : (sp_str_t) sp_zero,
    .triple = { .arch = d->arch, .os = d->os, .abi = d->abi },
  };
}

static sp_err_t profile_catalog(sp_test_t* t, spn_toolchain_catalog_t* catalog) {
  sp_mem_t mem = sp_test_arena(t);
  sp_str_t path = test_repo_path(mem, sp_str_lit("source/core/toolchain/toolchains.json"));

  sp_str_t json = sp_zero;
  sp_must_ok(t, sp_io_read_file(mem, path, &json));
  sp_must_eq(t, (u32)SPN_OK, (u32)spn_toolchain_catalog_init(catalog, json, mem));
  return SP_OK;
}

sp_test_each(profile, resolve, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = spn.mem;

  spn_profile_info_t profile = desc_to_info(&it->profile);
  spn_profile_info_t derived = desc_to_info(&it->derived);
  spn_profile_override_t overrides = desc_to_override(&it->overrides);

  spn_pkg_info_t pkg = sp_zero;
  if (!sp_str_empty(profile.name)) {
    sp_str_om_insert(pkg.profiles, profile.name, profile);
  }
  if (!sp_str_empty(derived.name)) {
    sp_str_om_insert(pkg.profiles, derived.name, derived);
  }

  spn_profile_table_t table = SP_NULLPTR;
  sp_str_ht_init(mem, table);
  spn_profile_populate(&table, &pkg);

  spn_toolchain_catalog_t catalog = sp_zero;
  if (profile_catalog(t, &catalog)) return SP_ERR;

  spn_profile_info_t result = sp_zero;
  spn_toolchain_resolution_t resolution = sp_zero;
  spn_err_t err = spn_profile_resolve(table, &overrides, &result);
  if (!err) {
    bool shared = spn_profile_shared(&result, it->shared_demand);
    err = spn_toolchain_select(&catalog, (spn_toolchain_query_t) {
      .name = result.toolchain,
      .target = { result.arch, result.os, result.abi },
      .host = it->host,
      .role = SPN_TOOLCHAIN_ROLE_BUILD,
      .shared = shared,
    }, sp_test_arena(t), &resolution);
    if (!err) {
      spn_profile_finalize(&result, resolution.triple, shared);
    }
  }

  sp_must_eq(t, (u32)it->expect.err, (u32)err);
  if (it->expect.err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, it->expect.err);
    return SP_OK;
  }

  sp_expect_eq(t, it->expect.target.arch, result.arch);
  sp_expect_eq(t, it->expect.target.os, result.os);
  sp_expect_eq(t, it->expect.target.abi, result.abi);
  sp_expect_eq(t, it->expect.targeted, result.targeted);
  sp_expect_eq(t, (u32)it->expect.linkage, (u32)result.linkage);
  if (it->expect.toolchain) {
    sp_expect_str_eq_c(t, result.toolchain, it->expect.toolchain);
  }
  if (it->expect.selected) {
    sp_must(t, resolution.info);
    sp_expect_str_eq_c(t, resolution.info->name, it->expect.selected);
  }

  return SP_OK;
}
