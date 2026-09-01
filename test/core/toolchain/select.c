#include "toolchain.h"
#include "triple/triple.h"

#define SELECT_MAX_QUERIES 2
#define SELECT_MAX_TARGETS 4
#define SELECT_MAX_CHECKS 5
#define SELECT_MAX_ABIS 3

#define HOST_X64_LINUX_MUSL { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define HOST_X64_WIN_GNU    { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU }
#define HOST_X64_WIN_MSVC   { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define TARGET_WIN_MSVC     { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC }
#define TARGET_LINUX_GNU    { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU }
#define TARGET_LINUX_MUSL   { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define TARGET_ARM_LINUX_GNU  { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU }
#define TARGET_ARM_LINUX_MUSL { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL }
#define TARGET_ARM_MACOS      { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE }
#define TARGET_ARM_BARE       { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE }

typedef struct {
  spn_err_t err;
  const c8* name;
  const c8* artifact;
  bool no_artifact;
  spn_triple_t triple;
  const c8* abis [SELECT_MAX_ABIS];
} select_expect_t;

typedef struct {
  const c8* name;
  spn_toolchain_role_t role;
  spn_triple_t target;
  spn_triple_t host;
  bool shared;
  select_expect_t expect;
} select_query_t;

typedef struct {
  const c8* name;
  const c8* file;
  select_query_t queries [SELECT_MAX_QUERIES];
  struct { bool same_definition; } expect;
} select_test_t;

typedef struct {
  spn_triple_t target;
  bool shared;
  spn_err_t err;
  spn_triple_t triple;
  const c8* abis [SELECT_MAX_ABIS];
} complete_check_t;

typedef struct {
  const c8* name;
  spn_triple_t targets [SELECT_MAX_TARGETS];
  spn_triple_t host;
  complete_check_t checks [SELECT_MAX_CHECKS];
} complete_test_t;

static const complete_test_t complete_tests [] = {
  {
    .name = "host_only_completes_to_host",
    .checks = {
      { .triple = HOST_X64_LINUX },
      { .target = { .os = SPN_OS_LINUX }, .triple = HOST_X64_LINUX },
      { .target = { .abi = SPN_ABI_GNU }, .triple = HOST_X64_LINUX },
      { .target = HOST_X64_LINUX, .triple = HOST_X64_LINUX },
      { .shared = true, .triple = HOST_X64_LINUX },
    },
  },
  {
    .name = "host_only_rejects_foreign_fields",
    .checks = {
      { .target = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_GNU }, .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = { .abi = SPN_ABI_MUSL }, .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_GNU }, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  },
  {
    .name = "explicit_abi_filters_entries",
    .targets = { TARGET_LINUX_GNU, TARGET_LINUX_MUSL },
    .checks = {
      { .target = { .abi = SPN_ABI_MUSL }, .triple = TARGET_LINUX_MUSL },
      { .target = { .abi = SPN_ABI_GNU }, .triple = TARGET_LINUX_GNU },
      { .target = { .abi = SPN_ABI_MSVC }, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  },
  {
    .name = "native_static_prefers_musl",
    .targets = { TARGET_LINUX_GNU, TARGET_LINUX_MUSL },
    .checks = {
      { .triple = TARGET_LINUX_MUSL },
    },
  },
  {
    .name = "native_shared_prefers_host_libc",
    .targets = { TARGET_LINUX_GNU, TARGET_LINUX_MUSL },
    .checks = {
      { .shared = true, .triple = TARGET_LINUX_GNU },
    },
  },
  {
    .name = "native_shared_on_musl_host_prefers_musl",
    .targets = { TARGET_LINUX_GNU, TARGET_LINUX_MUSL },
    .host = HOST_X64_LINUX_MUSL,
    .checks = {
      { .shared = true, .triple = TARGET_LINUX_MUSL },
    },
  },
  {
    .name = "native_single_abi_completes_regardless_of_linkage",
    .targets = { TARGET_LINUX_MUSL },
    .checks = {
      { .triple = TARGET_LINUX_MUSL },
      { .shared = true, .triple = TARGET_LINUX_MUSL },
    },
  },
  {
    .name = "native_windows_prefers_gnu",
    .targets = { TARGET_WIN_MSVC, TARGET_WIN_GNU },
    .host = HOST_X64_WIN_GNU,
    .checks = {
      { .triple = TARGET_WIN_GNU },
      { .shared = true, .triple = TARGET_WIN_GNU },
      { .target = { .abi = SPN_ABI_MSVC }, .triple = TARGET_WIN_MSVC },
    },
  },
  {
    .name = "native_windows_ignores_host_abi",
    .targets = { TARGET_WIN_MSVC, TARGET_WIN_GNU },
    .host = HOST_X64_WIN_MSVC,
    .checks = {
      { .triple = TARGET_WIN_GNU },
    },
  },
  {
    .name = "native_single_abi_list_ignores_host_abi",
    .targets = { TARGET_WIN_MSVC, { SPN_ARCH_ARM64, SPN_OS_WINDOWS, SPN_ABI_MSVC } },
    .host = HOST_X64_WIN_GNU,
    .checks = {
      { .triple = TARGET_WIN_MSVC },
    },
  },
  {
    .name = "unset_arch_and_os_pin_to_host",
    .targets = { TARGET_ARM_LINUX_GNU, TARGET_LINUX_GNU },
    .checks = {
      { .target = { .abi = SPN_ABI_GNU }, .triple = TARGET_LINUX_GNU },
    },
  },
  {
    .name = "host_pinning_rejects_mismatched_list",
    .targets = { TARGET_WIN_GNU, TARGET_ARM_LINUX_GNU },
    .checks = {
      { .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  },
  {
    .name = "cross_only_list_rejects_unset_target",
    .targets = { TARGET_WASM },
    .checks = {
      { .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = TARGET_WASM, .triple = TARGET_WASM },
    },
  },
  {
    .name = "cross_requires_abi",
    .targets = { TARGET_ARM_LINUX_GNU, TARGET_ARM_LINUX_MUSL },
    .checks = {
      { .target = { .arch = SPN_ARCH_ARM64 }, .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "musl" } },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_LINUX }, .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "musl" } },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_LINUX }, .shared = true, .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "musl" } },
      { .target = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL }, .triple = TARGET_ARM_LINUX_MUSL },
      { .target = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_GNU }, .shared = true, .triple = TARGET_ARM_LINUX_GNU },
    },
  },
  {
    .name = "cross_never_ranks_on_host_libc",
    .targets = { TARGET_ARM_LINUX_GNU, TARGET_ARM_LINUX_MUSL },
    .host = HOST_X64_LINUX_MUSL,
    .checks = {
      { .target = { .arch = SPN_ARCH_ARM64 }, .shared = true, .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "musl" } },
    },
  },
  {
    .name = "cross_abi_is_definitional_not_from_list",
    .targets = { TARGET_WIN_GNU },
    .checks = {
      { .target = { .os = SPN_OS_WINDOWS }, .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "msvc" } },
      { .target = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_GNU }, .triple = TARGET_WIN_GNU },
      { .target = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC }, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  },
  {
    .name = "cross_single_abi_os_completes",
    .targets = { TARGET_ARM_MACOS, TARGET_WASM, TARGET_ARM_BARE },
    .checks = {
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS }, .triple = TARGET_ARM_MACOS },
      { .target = { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI }, .triple = TARGET_WASM },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_FREESTANDING }, .triple = TARGET_ARM_BARE },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS, .abi = SPN_ABI_GNU }, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  },
  {
    .name = "unset_arch_never_drifts",
    .targets = { TARGET_ARM_MACOS },
    .checks = {
      { .target = { .os = SPN_OS_MACOS }, .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS }, .triple = TARGET_ARM_MACOS },
    },
  },
  {
    .name = "explicit_abi_keeps_host_arch",
    .targets = { TARGET_ARM_LINUX_MUSL },
    .checks = {
      { .target = { .abi = SPN_ABI_MUSL }, .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = { .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL }, .err = SPN_ERR_TOOLCHAIN_TARGET },
      { .target = { .arch = SPN_ARCH_ARM64, .abi = SPN_ABI_MUSL }, .triple = TARGET_ARM_LINUX_MUSL },
    },
  },
};

static const select_test_t select_tests [] = {
  {
    .name = "auto_picks_first_in_declared_order",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = TARGET_WIN_GNU,
        .expect = { .name = "A", .triple = TARGET_WIN_GNU },
      },
    },
  },
  {
    .name = "auto_completes_partial_to_host",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .expect = { .name = "B", .artifact = "https://example.com/linux.tar.xz", .triple = TARGET_LINUX_MUSL },
      },
    },
  },
  {
    .name = "auto_error_reports_pinned_target",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = { .abi = SPN_ABI_MSVC },
        .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
      },
    },
  },
  {
    .name = "auto_cross_requires_abi_before_scanning",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = { .os = SPN_OS_WINDOWS },
        .expect = { .err = SPN_ERR_TARGET_ABI, .abis = { "gnu", "msvc" } },
      },
    },
  },
  {
    .name = "auto_skips_unsupported_target",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = HOST_X64_LINUX,
        .expect = { .name = "B", .artifact = "https://example.com/linux.tar.xz", .triple = HOST_X64_LINUX },
      },
    },
  },
  {
    .name = "auto_skips_distribution_without_host_artifact",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = { .os = SPN_OS_LINUX },
        .host = HOST_ARM_LINUX,
        .expect = { .name = "C", .no_artifact = true, .triple = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_MUSL } },
      },
    },
  },
  {
    .name = "auto_with_empty_catalog",
    .file = "empty.json",
    .queries = {
      {
        .name = "auto",
        .target = HOST_X64_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
      },
    },
  },
  {
    .name = "auto_with_no_capable_toolchain",
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = TARGET_WASM,
        .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
      },
    },
  },
  {
    .name = "unknown_name",
    .file = "empty.json",
    .queries = {
      {
        .name = "A",
        .target = HOST_X64_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_UNKNOWN },
      },
    },
  },
  {
    .name = "unsupported_target",
    .file = "local.json",
    .queries = {
      {
        .name = "A",
        .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
        .target = TARGET_WIN_GNU,
        .expect = { .err = SPN_ERR_TOOLCHAIN_SCRIPT_TARGET },
      },
    },
  },
  {
    .name = "local_resolves_without_artifact",
    .file = "local.json",
    .queries = {
      {
        .name = "A",
        .target = HOST_X64_LINUX,
        .expect = { .name = "A", .no_artifact = true, .triple = HOST_X64_LINUX },
      },
    },
  },
  {
    .name = "local_completes_partial_to_host",
    .file = "local.json",
    .queries = {
      {
        .name = "A",
        .expect = { .name = "A", .no_artifact = true, .triple = HOST_X64_LINUX },
      },
    },
  },
  {
    .name = "local_host_restriction",
    .file = "restricted.json",
    .queries = {
      {
        .name = "A",
        .expect = { .name = "A", .no_artifact = true, .triple = HOST_X64_LINUX },
      },
      {
        .name = "A",
        .host = HOST_ARM_MACOS,
        .expect = { .err = SPN_ERR_TOOLCHAIN_HOST },
      },
    },
  },
  {
    .name = "auto_skips_host_restricted",
    .file = "restricted.json",
    .queries = {
      {
        .name = "auto",
        .host = HOST_ARM_MACOS,
        .expect = { .name = "B", .no_artifact = true, .triple = HOST_ARM_MACOS },
      },
    },
  },
  {
    .name = "distribution_pin_without_artifact_is_ineligible",
    .file = "mixed.json",
    .queries = {
      {
        .name = "A",
        .target = HOST_X64_LINUX,
        .expect = { .name = "A", .artifact = "https://example.com/linux.tar.xz", .triple = HOST_X64_LINUX },
      },
      {
        .name = "A",
        .target = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_APPLE },
        .host = HOST_ARM_MACOS,
        .expect = { .err = SPN_ERR_TOOLCHAIN_HOST },
      },
    },
  },
  {
    .name = "distribution_artifact_matches_host",
    .file = "distribution.json",
    .queries = {
      {
        .name = "A",
        .target = TARGET_WASM,
        .expect = { .name = "A", .artifact = "https://example.com/linux.tar.xz", .triple = TARGET_WASM },
      },
      {
        .name = "A",
        .target = TARGET_WASM,
        .host = HOST_ARM_MACOS,
        .expect = { .name = "A", .artifact = "https://example.com/macos.tar.xz", .triple = TARGET_WASM },
      },
    },
    .expect = { .same_definition = true },
  },
  {
    .name = "distribution_rejects_unsupported_host",
    .file = "distribution.json",
    .queries = {
      {
        .name = "A",
        .target = TARGET_WASM,
        .host = HOST_ARM_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_HOST, .no_artifact = true },
      },
    },
  },
  {
    .name = "target_error_precedes_host_error",
    .file = "distribution.json",
    .queries = {
      {
        .name = "A",
        .target = TARGET_WIN_GNU,
        .host = HOST_ARM_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .no_artifact = true },
      },
    },
  },
};

static spn_triple_t pinned_target(spn_triple_t target, spn_triple_t host) {
  return (spn_triple_t) {
    target.arch ? target.arch : host.arch,
    target.os ? target.os : host.os,
    target.abi,
  };
}

static sp_err_t check_target_error(sp_test_t* t, sp_mem_t mem, spn_event_t* err, spn_toolchain_info_t* toolchain, spn_triple_t target, spn_triple_t host) {
  sp_expect(t, fixture_triple_equal(err->err.toolchain.target, pinned_target(target, host)));
  sp_expect(t, fixture_triple_equal(err->err.toolchain.host, host));

  sp_da(sp_str_t) targets = err->err.toolchain.targets;
  if (sp_da_empty(toolchain->targets)) {
    sp_must_eq(t, 1, sp_da_size(targets));
    sp_expect_str_eq(t, targets[0], spn_triple_to_str(mem, host));
    return SP_OK;
  }

  sp_must_eq(t, sp_da_size(toolchain->targets), sp_da_size(targets));
  sp_da_for(toolchain->targets, it) {
    sp_expect_str_eq(t, targets[it], spn_triple_to_str(mem, toolchain->targets[it]));
  }
  return SP_OK;
}

static sp_err_t check_abi_error(sp_test_t* t, spn_event_t* err, spn_triple_t target, spn_triple_t host, const c8* const* abis) {
  sp_expect(t, fixture_triple_equal(err->err.completion.target, pinned_target(target, host)));
  sp_expect(t, fixture_triple_equal(err->err.completion.host, host));
  sp_da(sp_str_t) candidates = err->err.completion.candidates;
  sp_must_strs_eq(t, candidates, sp_da_size(candidates), abis);
  return SP_OK;
}

sp_test_each(select, complete, complete_test_t, complete_tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);

  spn_toolchain_info_t toolchain = fixture_local_toolchain("A", "cc");
  sp_carr_for(it->targets, at) {
    if (fixture_triple_empty(it->targets[at])) {
      break;
    }
    if (!toolchain.targets) {
      toolchain.targets = sp_da_new(mem, spn_triple_t);
    }
    sp_da_push(toolchain.targets, it->targets[at]);
  }

  spn_toolchain_catalog_t catalog = sp_zero;
  sp_str_om_init(catalog.entries);
  spn_toolchain_catalog_add(&catalog, toolchain);

  spn_triple_t host = fixture_triple_empty(it->host) ? (spn_triple_t) HOST_X64_LINUX : it->host;

  sp_carr_for(it->checks, at) {
    complete_check_t check = it->checks[at];
    if (!check.err && fixture_triple_empty(check.triple)) {
      break;
    }

    spn_toolchain_resolution_t resolution = sp_zero;
    spn_err_t err = spn_toolchain_select(&catalog, (spn_toolchain_query_t) {
      .name = sp_str_lit("A"),
      .target = check.target,
      .host = host,
      .shared = check.shared,
    }, mem, &resolution);

    sp_must_eq(t, (u32)check.err, (u32)err);
    if (!check.err) {
      sp_expect(t, fixture_triple_equal(resolution.triple, check.triple));
      continue;
    }

    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, check.err);
    switch (check.err) {
      case SPN_ERR_TARGET_ABI: {
        if (check_abi_error(t, &errs[0], check.target, host, check.abis)) return SP_ERR;
        break;
      }
      default: {
        if (check_target_error(t, mem, &errs[0], spn_toolchain_catalog_get(&catalog, sp_str_lit("A")), check.target, host)) return SP_ERR;
        break;
      }
    }
  }

  return SP_OK;
}

sp_test_each(select, resolve, select_test_t, select_tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file)) return SP_ERR;

  spn_toolchain_resolution_t resolutions [SELECT_MAX_QUERIES] = sp_zero;

  sp_carr_for(it->queries, at) {
    select_query_t query = it->queries[at];
    if (!query.name) {
      break;
    }

    spn_triple_t host = fixture_triple_empty(query.host) ? (spn_triple_t) HOST_X64_LINUX : query.host;
    spn_err_t err = spn_toolchain_select(&catalog, (spn_toolchain_query_t) {
      .name = sp_str_view(query.name),
      .target = query.target,
      .host = host,
      .role = query.role,
      .shared = query.shared,
    }, mem, &resolutions[at]);

    sp_must_eq(t, (u32)query.expect.err, (u32)err);

    if (query.expect.err == SPN_ERR_TARGET_ABI) {
      sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
      sp_must_eq(t, 1, sp_da_size(errs));
      sp_expect_eq(t, errs[0].err.kind, query.expect.err);
      if (check_abi_error(t, &errs[0], query.target, host, query.expect.abis)) return SP_ERR;
    } else if (query.expect.err) {
      sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
      sp_must_eq(t, 1, sp_da_size(errs));
      spn_err_toolchain_t* toolchain = &errs[0].err.toolchain;
      sp_expect_eq(t, errs[0].err.kind, query.expect.err);
      sp_expect_str_eq_c(t, toolchain->name, query.name);
      sp_da_for(toolchain->candidates, ct) {
        sp_must(t, spn_toolchain_catalog_get(&catalog, toolchain->candidates[ct]));
      }
      spn_toolchain_info_t* named = spn_toolchain_catalog_get(&catalog, sp_str_view(query.name));
      if (named) {
        if (check_target_error(t, mem, &errs[0], named, query.target, host)) return SP_ERR;
      } else {
        sp_expect(t, fixture_triple_equal(toolchain->host, host));
        sp_expect(t, sp_da_empty(toolchain->targets));
      }
    } else {
      sp_must(t, resolutions[at].info);
      sp_expect_str_eq_c(t, resolutions[at].info->name, query.expect.name);
      sp_expect(t, fixture_triple_equal(resolutions[at].triple, query.expect.triple));
    }

    if (query.expect.artifact) {
      sp_must(t, !sp_opt_is_null(resolutions[at].artifact));
      sp_expect_str_eq_c(t, sp_opt_get(resolutions[at].artifact).url, query.expect.artifact);
    }
    if (query.expect.no_artifact) {
      sp_expect(t, sp_opt_is_null(resolutions[at].artifact));
    }
  }

  if (it->expect.same_definition) {
    sp_expect(t, resolutions[0].info == resolutions[1].info);
  }

  return SP_OK;
}
