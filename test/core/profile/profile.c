#include "spn_test.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "profile/profile.h"
#include "pkg/types.h"
#include "target/types.h"
#include "triple/triple.h"

sp_test_suite(profile, .serial = true);

#define PROFILE_MAX_ABIS 3

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
  bool targeted;
} expect_t;

typedef struct {
  const c8* name;
  profile_desc_t profile;
  profile_desc_t derived;
  profile_desc_t overrides;
  spn_triple_t host;
  bool shared_demand;
  spn_abi_t abi;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "default_pins_to_host",
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
      .toolchain = "auto",
    },
  },
  {
    .name = "explicit_shared_is_kept",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_SHARED },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "explicit_static_is_kept",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_STATIC },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_STATIC,
    },
  },
  {
    .name = "shared_demand_defaults_to_shared",
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
    },
  },
  {
    .name = "explicit_static_ignores_shared_demand",
    .profile = { .name = "default", .linkage = SPN_LIB_KIND_STATIC },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .abi = SPN_ABI_MUSL,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
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
    .name = "gnu_defaults_to_shared",
    .overrides = { .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "musl_defaults_to_static",
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
    .name = "freestanding_defaults_to_static",
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .shared_demand = true,
    .abi = SPN_ABI_BARE,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "override_os_keeps_host_arch",
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .linkage = SPN_LIB_KIND_SHARED,
      .targeted = true,
    },
  },
  {
    .name = "override_arch_keeps_host_os",
    .overrides = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "override_os_pins_foreign_host_arch",
    .overrides = { .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
      .linkage = SPN_LIB_KIND_STATIC,
      .targeted = true,
    },
  },
  {
    .name = "manifest_toolchain_applies",
    .profile = { .name = "default", .toolchain = "gcc" },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "gcc",
    },
  },
  {
    .name = "override_toolchain_wins",
    .profile = { .name = "default", .toolchain = "gcc" },
    .overrides = { .toolchain = "clang" },
    .host = PROFILE_HOST_LINUX_GNU,
    .abi = SPN_ABI_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX },
      .linkage = SPN_LIB_KIND_SHARED,
      .toolchain = "clang",
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
    .abi = SPN_ABI_APPLE,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS },
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
    .host = PROFILE_HOST_WIN_GNU,
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
    .abi = SPN_ABI_APPLE,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_MACOS },
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

typedef struct {
  spn_abi_t abis [PROFILE_MAX_ABIS];
} query_expect_t;

typedef struct {
  const c8* name;
  spn_triple_t target;
  spn_linkage_t linkage;
  spn_triple_t host;
  query_expect_t expect;
} query_test_t;

static const query_test_t query_tests [] = {
  {
    .name = "native_static_prefers_musl",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL, SPN_ABI_GNU } },
  },
  {
    .name = "native_shared_prefers_host_libc",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_GNU, SPN_ABI_MUSL } },
  },
  {
    .name = "native_shared_on_musl_host_prefers_musl",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_MUSL,
    .expect = { .abis = { SPN_ABI_MUSL, SPN_ABI_GNU } },
  },
  {
    .name = "native_windows_prefers_gnu",
    .target = { SPN_ARCH_X64, SPN_OS_WINDOWS },
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = { .abis = { SPN_ABI_GNU, SPN_ABI_MSVC } },
  },
  {
    .name = "native_macos_is_apple",
    .target = { SPN_ARCH_ARM64, SPN_OS_MACOS },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = { .abis = { SPN_ABI_APPLE } },
  },
  {
    .name = "explicit_abi_is_the_only_candidate",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_GNU } },
  },
  {
    .name = "explicit_abi_ignores_linkage",
    .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL } },
  },
  {
    .name = "native_windows_ignores_shared",
    .target = { SPN_ARCH_X64, SPN_OS_WINDOWS },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = { .abis = { SPN_ABI_GNU, SPN_ABI_MSVC } },
  },
  {
    .name = "cross_explicit_abi_is_the_only_candidate",
    .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL } },
  },
  {
    .name = "cross_os_with_many_abis_has_no_candidates",
    .target = { SPN_ARCH_X64, SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
  },
  {
    .name = "cross_arch_with_many_abis_has_no_candidates",
    .target = { SPN_ARCH_ARM64, SPN_OS_LINUX },
    .linkage = SPN_LIB_KIND_SHARED,
    .host = PROFILE_HOST_LINUX_GNU,
  },
  {
    .name = "cross_macos_is_apple",
    .target = { SPN_ARCH_ARM64, SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_APPLE } },
  },
  {
    .name = "cross_wasi_is_musl",
    .target = { SPN_ARCH_WASM32, SPN_OS_WASI },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_MUSL } },
  },
  {
    .name = "cross_freestanding_is_bare",
    .target = { SPN_ARCH_ARM64, SPN_OS_FREESTANDING },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .abis = { SPN_ABI_BARE } },
  },
};

static spn_profile_info_t desc_to_info(const profile_desc_t* d) {
  return (spn_profile_info_t) {
    .name = d->name ? sp_cstr_as_str(d->name) : (sp_str_t) sp_zero,
    .toolchain = d->toolchain ? sp_cstr_as_str(d->toolchain) : (sp_str_t) sp_zero,
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
  if (it->shared_demand) {
    sp_str_om_insert(pkg.libs, sp_str_lit("L"), ((spn_target_info_t) { .name = sp_str_lit("L"), .linkages = { .shared = true } }));
  }

  spn_profile_table_t table = SP_NULLPTR;
  sp_str_ht_init(mem, table);
  spn_profile_populate(&table, &pkg);

  spn_profile_info_t result = sp_zero;
  spn_err_t err = spn_profile_resolve(table, &overrides, it->host, &pkg, &result);
  sp_must_eq(t, (u32)it->expect.err, (u32)err);
  if (err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, err);
    return SP_OK;
  }

  sp_expect(t, spn_triple_equal(it->expect.target, (spn_triple_t) { result.arch, result.os, result.abi }));
  sp_expect_eq(t, it->expect.targeted, result.targeted);
  if (it->expect.toolchain) {
    sp_expect_str_eq_c(t, result.toolchain, it->expect.toolchain);
  }

  spn_profile_finalize(&result, it->abi ? it->abi : result.abi);
  sp_expect_eq(t, (u32)it->expect.linkage, (u32)result.linkage);
  return SP_OK;
}

sp_test_each(profile, query, query_test_t, query_tests) {
  spn_profile_info_t profile = {
    .toolchain = sp_str_lit("T"),
    .arch = it->target.arch,
    .os = it->target.os,
    .abi = it->target.abi,
    .linkage = it->linkage,
  };

  spn_toolchain_query_t query = spn_profile_query(&profile, it->host);
  sp_expect_str_eq(t, query.name, profile.toolchain);
  sp_expect(t, spn_triple_equal(query.target, it->target));

  u32 abis = 0;
  sp_carr_detect_len(it->expect.abis, abis, it->expect.abis[abis]);
  sp_must_eq(t, abis, query.abis.count);
  sp_for(at, abis) {
    sp_expect_eq(t, (u32)it->expect.abis[at], (u32)query.abis.items[at]);
  }
  return SP_OK;
}
