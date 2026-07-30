#include "spn_test.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "profile/profile.h"
#include "pkg/types.h"

sp_test_suite(profile, .serial = true);

#define PROFILE_HOST_LINUX_GNU { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define PROFILE_HOST_WIN_MSVC  { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define PROFILE_HOST_ARM_MACOS { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE }

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
    .name = "macos_host_keeps_abi_empty",
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "arch_override_keeps_host_os",
    .overrides = { .arch = SPN_ARCH_ARM64 },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "wasi_target_keeps_abi_empty",
    .overrides = { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_WASM32, SPN_OS_WASI, SPN_ABI_NONE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "macos_target_keeps_abi_empty",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "cross_windows_defaults_to_gnu",
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "cross_linux_defaults_to_musl",
    .overrides = { .os = SPN_OS_LINUX },
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
      .targeted = true,
    },
  },
  {
    .name = "profile_toolchain_overrides_auto",
    .profile = { .name = "default", .toolchain = "system" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "system",
    },
  },
  {
    .name = "override_toolchain_wins",
    .profile = { .name = "default", .toolchain = "system" },
    .overrides = { .toolchain = "msvc" },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "msvc",
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
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "os_override_redefaults_manifest_abi",
    .profile = { .name = "default", .abi = SPN_ABI_MUSL },
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
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
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .linkage = SPN_LIB_KIND_SHARED,
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
      .target = { SPN_ARCH_X64, SPN_OS_MACOS, SPN_ABI_NONE },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "derived_abi_overlays_base_os",
    .profile = { .name = "default", .os = SPN_OS_WINDOWS },
    .derived = { .name = "msvc", .abi = SPN_ABI_MSVC },
    .overrides = { .name = "msvc" },
    .host = PROFILE_HOST_LINUX_GNU,
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

sp_test_each(profile, resolve, test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = spn.mem;

  spn_profile_info_t profile = desc_to_info(&it->profile);
  spn_profile_info_t derived = desc_to_info(&it->derived);
  spn_profile_info_t overrides = desc_to_info(&it->overrides);

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

  spn_profile_info_t result = sp_zero;
  spn_err_union_t err = spn_profile_resolve(table, &overrides, it->host, it->shared_demand, &result);
  sp_must_eq(t, (u32)it->expect.err, (u32)err.kind);
  if (it->expect.err) {
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

  return SP_OK;
}
