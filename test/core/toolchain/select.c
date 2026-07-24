#include "sp.h"
#include "utest.h"

#include "fixture.h"

#define SELECT_MAX_TOOLCHAINS 2
#define SELECT_MAX_TARGETS 2
#define SELECT_MAX_QUERIES 2
#define SELECT_MAX_CHECKS 4

typedef struct {
  const c8* name;
  const c8* compiler;
  spn_triple_t targets [SELECT_MAX_TARGETS];
} select_toolchain_t;

typedef struct {
  const c8* name;
  spn_toolchain_role_t role;
  spn_triple_t target;
  spn_triple_t host;
  const c8* expect;
  spn_err_t err;
  bool no_artifact;
} select_query_t;

typedef struct {
  const c8* file;
  select_toolchain_t toolchains [SELECT_MAX_TOOLCHAINS];
  select_query_t queries [SELECT_MAX_QUERIES];
  bool same_definition;
} select_test_t;

typedef struct {
  spn_triple_t target;
  bool supported;
} supports_check_t;

typedef struct {
  spn_triple_t targets [SELECT_MAX_TARGETS];
  supports_check_t checks [SELECT_MAX_CHECKS];
} supports_test_t;

static void fixture_targets(sp_mem_t mem, spn_toolchain_info_t* toolchain, const spn_triple_t* targets, u32 len) {
  sp_for(it, len) {
    if (fixture_triple_empty(targets[it])) {
      break;
    }
    if (!toolchain->targets) {
      toolchain->targets = sp_da_new(mem, spn_triple_t);
    }
    sp_da_push(toolchain->targets, targets[it]);
  }
}

static void run_select_test(s32* utest_result, select_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, t.file);

  sp_carr_for(t.toolchains, it) {
    select_toolchain_t desc = t.toolchains[it];
    if (!desc.name) {
      break;
    }
    spn_toolchain_info_t toolchain = fixture_local_toolchain(desc.name, desc.compiler);
    fixture_targets(mem, &toolchain, desc.targets, SP_CARR_LEN(desc.targets));
    spn_toolchain_catalog_add(&catalog, toolchain);
  }

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

    ASSERT_EQ((u32)query.err, (u32)err.kind);

    if (query.err) {
      EXPECT_STR(err.toolchain.name, query.name);
      EXPECT_EQ((u32)query.role, (u32)err.toolchain.role);
      EXPECT_TRUE(err.toolchain.catalog == &catalog);
      EXPECT_TRUE(fixture_triple_equal(err.toolchain.host, host));
      if (query.err != SPN_ERR_TOOLCHAIN_UNKNOWN) {
        EXPECT_TRUE(fixture_triple_equal(err.toolchain.target, query.target));
      }
    } else {
      ASSERT_TRUE(resolutions[it].info);
      EXPECT_STR(resolutions[it].info->name, query.expect);
    }

    if (query.no_artifact) {
      EXPECT_TRUE(sp_opt_is_null(resolutions[it].artifact));
    }
  }

  if (t.same_definition) {
    EXPECT_TRUE(resolutions[0].info == resolutions[1].info);
  }
}

static void run_supports_test(s32* utest_result, supports_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  spn_toolchain_info_t toolchain = fixture_local_toolchain("system", "cc");
  fixture_targets(mem, &toolchain, t.targets, SP_CARR_LEN(t.targets));

  sp_carr_for(t.checks, it) {
    supports_check_t check = t.checks[it];
    if (fixture_triple_empty(check.target)) {
      break;
    }
    EXPECT_EQ(check.supported, spn_toolchain_supports(&toolchain, check.target, HOST_X64_LINUX));
  }
}

UTEST(select, system_for_host_pulls_zig_for_scripts) {
  run_select_test(utest_result, (select_test_t) {
    .file = "zig_system.json",
    .queries = {
      { .name = "system", .target = HOST_X64_LINUX, .expect = "system" },
      { .name = "zig", .role = SPN_TOOLCHAIN_ROLE_SCRIPT, .target = TARGET_WASM, .expect = "zig" },
    },
  });
}

UTEST(select, zig_resolves_for_both_contexts) {
  run_select_test(utest_result, (select_test_t) {
    .file = "zig.json",
    .queries = {
      { .name = "zig", .target = HOST_X64_LINUX, .expect = "zig" },
      { .name = "zig", .role = SPN_TOOLCHAIN_ROLE_SCRIPT, .target = TARGET_WASM, .expect = "zig" },
    },
    .same_definition = true,
  });
}

UTEST(select, unknown_toolchain) {
  run_select_test(utest_result, (select_test_t) {
    .file = "empty.json",
    .queries = {
      { .name = "gcc-13", .target = HOST_X64_LINUX, .err = SPN_ERR_TOOLCHAIN_UNKNOWN },
    },
  });
}

UTEST(select, undeclared_targets_are_host_only) {
  run_select_test(utest_result, (select_test_t) {
    .file = "system.json",
    .queries = {
      { .name = "system", .target = TARGET_WIN_GNU, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  });
}

UTEST(select, declared_target_allows_cross) {
  run_select_test(utest_result, (select_test_t) {
    .file = "empty.json",
    .toolchains = {
      { .name = "mingw", .compiler = "x86_64-w64-mingw32-gcc", .targets = { TARGET_WIN_GNU } },
    },
    .queries = {
      { .name = "mingw", .target = TARGET_WIN_GNU, .expect = "mingw" },
    },
  });
}

UTEST(select, cross_toolchain_rejects_other_targets) {
  run_select_test(utest_result, (select_test_t) {
    .file = "empty.json",
    .toolchains = {
      { .name = "mingw", .compiler = "x86_64-w64-mingw32-gcc", .targets = { TARGET_WIN_GNU } },
    },
    .queries = {
      { .name = "mingw", .target = HOST_X64_LINUX, .err = SPN_ERR_TOOLCHAIN_TARGET },
    },
  });
}

UTEST(select, script_role_is_reported) {
  run_select_test(utest_result, (select_test_t) {
    .file = "empty.json",
    .toolchains = {
      { .name = "zig", .compiler = "/opt/zig/zig" },
    },
    .queries = {
      {
        .name = "zig",
        .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
        .target = TARGET_WASM,
        .err = SPN_ERR_TOOLCHAIN_TARGET,
      },
    },
  });
}

UTEST(select, undeclared_target_rejects_cross) {
  run_select_test(utest_result, (select_test_t) {
    .file = "zig.json",
    .queries = {
      {
        .name = "zig",
        .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
        .err = SPN_ERR_TOOLCHAIN_TARGET,
      },
    },
  });
}

UTEST(select, distribution_rejects_unsupported_host) {
  run_select_test(utest_result, (select_test_t) {
    .file = "zig.json",
    .queries = {
      {
        .name = "zig",
        .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
        .target = TARGET_WASM,
        .host = HOST_ARM_LINUX,
        .err = SPN_ERR_TOOLCHAIN_HOST,
        .no_artifact = true,
      },
    },
  });
}

UTEST(select, supports_empty_targets_match_host_only) {
  run_supports_test(utest_result, (supports_test_t) {
    .checks = {
      { .target = HOST_X64_LINUX, .supported = true },
      { .target = TARGET_WIN_GNU },
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
