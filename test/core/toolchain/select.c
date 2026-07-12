#include "sp.h"
#include "utest.h"

#include "fixture.h"

typedef struct {
  const c8* name;
  const c8* compiler;
  spn_triple_t targets [2];
} select_toolchain_t;

typedef struct {
  spn_err_t kind;
  const c8* build;
  const c8* script;
  u32 required;
  bool dedup;
  struct {
    const c8* name;
    spn_toolchain_role_t role;
    spn_triple_t target;
  } err;
} select_expect_t;

typedef struct {
  const c8* build;
  spn_triple_t target;
  select_toolchain_t toolchains [2];
  select_expect_t expect;
} select_test_t;

typedef struct {
  spn_triple_t target;
  bool supported;
} supports_check_t;

typedef struct {
  spn_triple_t targets [2];
  supports_check_t checks [4];
} supports_test_t;

static bool triple_empty(spn_triple_t triple) {
  return !triple.arch && !triple.os && !triple.abi;
}

static void fixture_targets(sp_mem_t mem, spn_toolchain_t* toolchain, const spn_triple_t* targets, u32 len) {
  sp_for(it, len) {
    if (triple_empty(targets[it])) {
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
  fixture_catalog(utest_result, &catalog, HOST_X64_LINUX);

  sp_carr_for(t.toolchains, it) {
    select_toolchain_t desc = t.toolchains[it];
    if (!desc.name) {
      break;
    }
    spn_toolchain_t toolchain = fixture_local_toolchain(desc.name, desc.compiler);
    fixture_targets(mem, &toolchain, desc.targets, SP_CARR_LEN(desc.targets));
    spn_toolchain_catalog_add(&catalog, toolchain);
  }

  spn_toolchain_query_t query = {
    .build = sp_str_view(t.build),
    .script = sp_str_lit("zig"),
    .target = t.target,
    .host = HOST_X64_LINUX,
  };

  spn_toolchain_selection_t selection = sp_zero;
  spn_err_union_t err = spn_toolchain_select(&catalog, query, mem, &selection);

  ASSERT_EQ((u32)t.expect.kind, (u32)err.kind);

  if (t.expect.kind != SPN_OK) {
    EXPECT_STR(err.toolchain.name, t.expect.err.name);
    EXPECT_EQ((u32)t.expect.err.role, (u32)err.toolchain.role);
    EXPECT_TRUE(err.toolchain.catalog == &catalog);
    EXPECT_EQ((u32)HOST_X64_LINUX.arch, (u32)err.toolchain.host.arch);
    EXPECT_EQ((u32)HOST_X64_LINUX.os, (u32)err.toolchain.host.os);
    EXPECT_EQ((u32)HOST_X64_LINUX.abi, (u32)err.toolchain.host.abi);
    if (!triple_empty(t.expect.err.target)) {
      EXPECT_EQ((u32)t.expect.err.target.arch, (u32)err.toolchain.target.arch);
      EXPECT_EQ((u32)t.expect.err.target.os, (u32)err.toolchain.target.os);
      EXPECT_EQ((u32)t.expect.err.target.abi, (u32)err.toolchain.target.abi);
    }
    return;
  }

  ASSERT_TRUE(selection.build);
  ASSERT_TRUE(selection.script);
  EXPECT_STR(selection.build->name, t.expect.build);
  EXPECT_STR(selection.script->name, t.expect.script);
  EXPECT_EQ(t.expect.required, (u32)sp_da_size(selection.required));

  if (t.expect.dedup) {
    EXPECT_TRUE(selection.build == selection.script);
    EXPECT_TRUE(selection.build == selection.required[0]);
  }
}

static void run_supports_test(s32* utest_result, supports_test_t t) {
  sp_mem_t mem = sp_mem_arena_as_allocator(ctx_get()->arena);

  spn_toolchain_t toolchain = fixture_local_toolchain("system", "cc");
  fixture_targets(mem, &toolchain, t.targets, SP_CARR_LEN(t.targets));

  sp_carr_for(t.checks, it) {
    supports_check_t check = t.checks[it];
    if (triple_empty(check.target)) {
      break;
    }
    EXPECT_EQ(check.supported, spn_toolchain_supports(&toolchain, check.target, HOST_X64_LINUX));
  }
}

UTEST(select, system_for_host_pulls_zig_for_scripts) {
  run_select_test(utest_result, (select_test_t) {
    .build = "system",
    .target = HOST_X64_LINUX,
    .expect = {
      .build = "system",
      .script = "zig",
      .required = 2,
    },
  });
}

UTEST(select, zig_as_build_toolchain_dedups) {
  run_select_test(utest_result, (select_test_t) {
    .build = "zig",
    .target = HOST_X64_LINUX,
    .expect = {
      .build = "zig",
      .script = "zig",
      .required = 1,
      .dedup = true,
    },
  });
}

UTEST(select, unknown_build_toolchain) {
  run_select_test(utest_result, (select_test_t) {
    .build = "gcc-13",
    .target = HOST_X64_LINUX,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_UNKNOWN,
      .err = { .name = "gcc-13" },
    },
  });
}

UTEST(select, undeclared_targets_are_host_only) {
  run_select_test(utest_result, (select_test_t) {
    .build = "system",
    .target = TARGET_WIN_GNU,
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_TARGET,
      .err = {
        .name = "system",
        .target = TARGET_WIN_GNU,
      },
    },
  });
}

UTEST(select, declared_target_allows_cross) {
  run_select_test(utest_result, (select_test_t) {
    .build = "mingw",
    .target = TARGET_WIN_GNU,
    .toolchains = {
      { .name = "mingw", .compiler = "x86_64-w64-mingw32-gcc", .targets = { TARGET_WIN_GNU } },
    },
    .expect = {
      .build = "mingw",
      .script = "zig",
      .required = 2,
    },
  });
}

UTEST(select, cross_toolchain_rejects_other_targets) {
  run_select_test(utest_result, (select_test_t) {
    .build = "mingw",
    .target = HOST_X64_LINUX,
    .toolchains = {
      { .name = "mingw", .compiler = "x86_64-w64-mingw32-gcc", .targets = { TARGET_WIN_GNU } },
    },
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_TARGET,
      .err = {
        .name = "mingw",
        .target = HOST_X64_LINUX,
      },
    },
  });
}

UTEST(select, script_toolchain_must_target_wasm) {
  run_select_test(utest_result, (select_test_t) {
    .build = "system",
    .target = HOST_X64_LINUX,
    .toolchains = {
      { .name = "zig", .compiler = "/opt/zig/zig" },
    },
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_TARGET,
      .err = {
        .name = "zig",
        .role = SPN_TOOLCHAIN_ROLE_SCRIPT,
        .target = { .arch = SPN_ARCH_WASM32, .os = SPN_OS_WASI },
      },
    },
  });
}

UTEST(select, build_target_failure_reports_build_role_for_zig) {
  run_select_test(utest_result, (select_test_t) {
    .build = "zig",
    .target = { .arch = SPN_ARCH_X64, .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
    .expect = {
      .kind = SPN_ERR_TOOLCHAIN_TARGET,
      .err = {
        .name = "zig",
        .target = { .arch = SPN_ARCH_X64, .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
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
      { .target = { .arch = SPN_ARCH_X64, .os = SPN_OS_LINUX, .abi = SPN_ABI_MUSL }, .supported = true },
      { .target = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_LINUX, .abi = SPN_ABI_GNU }, .supported = true },
      { .target = TARGET_WIN_GNU },
    },
  });
}
