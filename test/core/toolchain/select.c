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
  const c8* file;
  select_query_t queries [SELECT_MAX_QUERIES];
  struct { bool same_definition; } expect;
} select_test_t;

typedef struct {
  spn_triple_t target;
  bool supported;
} supports_check_t;

typedef struct {
  spn_triple_t targets [SELECT_MAX_TARGETS];
  supports_check_t checks [SELECT_MAX_CHECKS];
} supports_test_t;

static void run_select_test(s32* utest_result, select_test_t t) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, t.file);

  spn_toolchain_resolution_t resolutions [SELECT_MAX_QUERIES] = sp_zero;

  sp_carr_for(t.queries, it) {
    select_query_t query = t.queries[it];
    if (!query.name) {
      break;
    }

    spn_triple_t host = fixture_triple_empty(query.host) ? HOST_X64_LINUX : query.host;
    spn_err_union_t err = spn_toolchain_select(&catalog, (spn_toolchain_query_t) {
      .name = sp_str_view(query.name),
      .target = query.target,
      .host = host,
      .role = query.role,
    }, &resolutions[it]);

    ASSERT_EQ((u32)query.expect.err, (u32)err.kind);

    if (query.expect.err) {
      EXPECT_STR(err.toolchain.name, query.name);
      EXPECT_EQ((u32)query.role, (u32)err.toolchain.role);
      EXPECT_TRUE(err.toolchain.catalog == &catalog);
      EXPECT_TRUE(fixture_triple_equal(err.toolchain.host, host));
      if (query.expect.err != SPN_ERR_TOOLCHAIN_UNKNOWN) {
        EXPECT_TRUE(fixture_triple_equal(err.toolchain.target, query.target));
      }
    } else {
      ASSERT_TRUE(resolutions[it].info);
      EXPECT_STR(resolutions[it].info->name, query.expect.name);
    }

    if (query.expect.artifact) {
      ASSERT_FALSE(sp_opt_is_null(resolutions[it].artifact));
      EXPECT_STR(sp_opt_get(resolutions[it].artifact).url, query.expect.artifact);
    }
    if (query.expect.no_artifact) {
      EXPECT_TRUE(sp_opt_is_null(resolutions[it].artifact));
    }
  }

  if (t.expect.same_definition) {
    EXPECT_TRUE(resolutions[0].info == resolutions[1].info);
  }
}

static void run_supports_test(s32* utest_result, supports_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  spn_toolchain_info_t toolchain = fixture_local_toolchain("A", "cc");
  sp_carr_for(t.targets, it) {
    if (fixture_triple_empty(t.targets[it])) {
      break;
    }
    if (!toolchain.targets) {
      toolchain.targets = sp_da_new(mem, spn_triple_t);
    }
    sp_da_push(toolchain.targets, t.targets[it]);
  }

  sp_carr_for(t.checks, it) {
    supports_check_t check = t.checks[it];
    if (fixture_triple_empty(check.target)) {
      break;
    }
    EXPECT_EQ(check.supported, spn_toolchain_supports(&toolchain, check.target, HOST_X64_LINUX));
  }
}

UTEST(select, supports_empty_targets_match_host_only) {
  run_supports_test(utest_result, (supports_test_t) {
    .checks = {
      { .target = HOST_X64_LINUX, .supported = true },
      { .target = { SPN_ARCH_X64, SPN_OS_LINUX } },
      { .target = TARGET_WIN_GNU },
    },
  });
}

UTEST(select, supports_declared_targets_replace_host) {
  run_supports_test(utest_result, (supports_test_t) {
    .targets = { TARGET_WIN_GNU },
    .checks = {
      { .target = TARGET_WIN_GNU, .supported = true },
      { .target = HOST_X64_LINUX },
    },
  });
}

UTEST(select, supports_wildcard_target_fields) {
  run_supports_test(utest_result, (supports_test_t) {
    .targets = {
      { .os = SPN_OS_LINUX },
    },
    .checks = {
      { .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_MUSL }, .supported = true },
      { .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU }, .supported = true },
      { .target = TARGET_WIN_GNU },
    },
  });
}

UTEST(select, auto_picks_first_in_declared_order) {
  run_select_test(utest_result, (select_test_t) {
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = TARGET_WIN_GNU,
        .expect = { .name = "A" },
      },
    },
  });
}

UTEST(select, auto_skips_unsupported_target) {
  run_select_test(utest_result, (select_test_t) {
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = HOST_X64_LINUX,
        .expect = { .name = "B", .artifact = "https://example.com/linux.tar.xz" },
      },
    },
  });
}

UTEST(select, auto_skips_distribution_without_host_artifact) {
  run_select_test(utest_result, (select_test_t) {
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = { .os = SPN_OS_LINUX },
        .host = HOST_ARM_LINUX,
        .expect = { .name = "C", .no_artifact = true },
      },
    },
  });
}

UTEST(select, auto_with_no_capable_toolchain) {
  run_select_test(utest_result, (select_test_t) {
    .file = "auto.json",
    .queries = {
      {
        .name = "auto",
        .target = TARGET_WASM,
        .expect = { .err = SPN_ERR_TOOLCHAIN_NONE },
      },
    },
  });
}

UTEST(select, unknown_name) {
  run_select_test(utest_result, (select_test_t) {
    .file = "empty.json",
    .queries = {
      {
        .name = "A",
        .target = HOST_X64_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_UNKNOWN },
      },
    },
  });
}

UTEST(select, unsupported_target) {
  run_select_test(utest_result, (select_test_t) {
    .file = "local.json",
    .queries = {
      {
        .name = "A",
        .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
        .target = TARGET_WIN_GNU,
        .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET },
      },
    },
  });
}

UTEST(select, local_resolves_without_artifact) {
  run_select_test(utest_result, (select_test_t) {
    .file = "local.json",
    .queries = {
      {
        .name = "A",
        .target = HOST_X64_LINUX,
        .expect = { .name = "A", .no_artifact = true },
      },
    },
  });
}

UTEST(select, distribution_artifact_matches_host) {
  run_select_test(utest_result, (select_test_t) {
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
  });
}

UTEST(select, distribution_rejects_unsupported_host) {
  run_select_test(utest_result, (select_test_t) {
    .file = "distribution.json",
    .queries = {
      {
        .name = "A",
        .target = TARGET_WASM,
        .host = HOST_ARM_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_HOST, .no_artifact = true },
      },
    },
  });
}

UTEST(select, target_error_precedes_host_error) {
  run_select_test(utest_result, (select_test_t) {
    .file = "distribution.json",
    .queries = {
      {
        .name = "A",
        .target = TARGET_WIN_GNU,
        .host = HOST_ARM_LINUX,
        .expect = { .err = SPN_ERR_TOOLCHAIN_TARGET, .no_artifact = true },
      },
    },
  });
}
