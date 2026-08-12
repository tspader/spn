#include "fixture.h"

#define SELECT_MAX_QUERIES 2
#define SELECT_MAX_TARGETS 2
#define SELECT_MAX_CHECKS 4

typedef struct {
  spn_err_t err;
  const c8* name;
  const c8* artifact;
  bool no_artifact;
} select_expect_t;

typedef struct {
  const c8* name;
  spn_toolchain_role_t role;
  spn_triple_t target;
  spn_triple_t host;
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
  bool supported;
} supports_check_t;

typedef struct {
  const c8* name;
  spn_triple_t targets [SELECT_MAX_TARGETS];
  supports_check_t checks [SELECT_MAX_CHECKS];
} supports_test_t;

static const supports_test_t supports_tests [] = {
  {
    .name = "empty_targets_match_host_only",
    .checks = {
      { .target = HOST_X64_LINUX, .supported = true },
      { .target = { SPN_ARCH_X64, SPN_OS_LINUX } },
      { .target = TARGET_WIN_GNU },
    },
  },
  {
    .name = "declared_targets_replace_host",
    .targets = { TARGET_WIN_GNU },
    .checks = {
      { .target = TARGET_WIN_GNU, .supported = true },
      { .target = HOST_X64_LINUX },
    },
  },
  {
    .name = "wildcard_target_fields",
    .targets = {
      { .os = SPN_OS_LINUX },
    },
    .checks = {
      { .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }, .supported = true },
      { .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU }, .supported = true },
      { .target = TARGET_WIN_GNU },
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
        .expect = { .name = "A" },
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
        .expect = { .name = "B", .artifact = "https://example.com/linux.tar.xz" },
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
        .expect = { .name = "C", .no_artifact = true },
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
        .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET },
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
        .expect = { .name = "A", .no_artifact = true },
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
        .expect = { .name = "A", .artifact = "https://example.com/linux.tar.xz" },
      },
      {
        .name = "A",
        .target = TARGET_WASM,
        .host = HOST_ARM_MACOS,
        .expect = { .name = "A", .artifact = "https://example.com/macos.tar.xz" },
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

bool is_supported(spn_toolchain_info_t* toolchain, spn_triple_t target, spn_triple_t host);

sp_test_each(select, supports, supports_test_t, supports_tests) {
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

  sp_carr_for(it->checks, at) {
    supports_check_t check = it->checks[at];
    if (fixture_triple_empty(check.target)) {
      break;
    }
    sp_expect_eq(t, check.supported, is_supported(&toolchain, check.target, (spn_triple_t) HOST_X64_LINUX));
  }

  return SP_OK;
}

sp_test_each(select, resolve, select_test_t, select_tests) {
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
    spn_err_union_t err = spn_toolchain_select(&catalog, (spn_toolchain_query_t) {
      .name = sp_str_view(query.name),
      .target = query.target,
      .host = host,
      .role = query.role,
    }, mem, &resolutions[at]);

    sp_must_eq(t, (u32)query.expect.err, (u32)err.kind);

    if (query.expect.err) {
      sp_expect_str_eq_c(t, err.toolchain.name, query.name);
      sp_expect_eq(t, (u32)query.role, (u32)err.toolchain.role);
      sp_da_for(err.toolchain.candidates, ct) {
        spn_toolchain_info_t* candidate = spn_toolchain_catalog_get(&catalog, err.toolchain.candidates[ct]);
        sp_must(t, candidate);
        sp_expect(t, is_supported(candidate, query.target, host));
      }
      sp_expect(t, fixture_triple_equal(err.toolchain.host, host));
      if (query.expect.err != SPN_ERR_TOOLCHAIN_UNKNOWN) {
        sp_expect(t, fixture_triple_equal(err.toolchain.target, query.target));
      }
    } else {
      sp_must(t, resolutions[at].info);
      sp_expect_str_eq_c(t, resolutions[at].info->name, query.expect.name);
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
