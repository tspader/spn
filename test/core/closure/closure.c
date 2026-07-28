#include "closure.h"

#include "utest.h"

typedef struct {
  const c8* name;
  bool private;
} closure_expected_t;

typedef struct {
  closure_graph_test_t graph;
  closure_expected_t expect [CLOSURE_TEST_MAX_NAMES];
} closure_test_t;

static void run_test(s32* utest_result, closure_test_t t) {
  closure_graph_t g = build_graph(&t.graph);

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);

  u32 expected = 0;
  sp_carr_for(t.expect, it) {
    if (!t.expect[it].name) {
      break;
    }
    expected++;
  }

  // The closure leads with the link unit's own package; the expectations
  // describe everything linked in behind it
  ASSERT_EQ(expected + 1, sp_da_size(closure));
  EXPECT_TRUE(closure[0].pkg == g.root->pkg);
  EXPECT_FALSE(closure[0].private);
  sp_for(it, expected) {
    EXPECT_TRUE(sp_str_equal(closure[it + 1].pkg->info->name, sp_str_view(t.expect[it].name)));
    EXPECT_EQ(t.expect[it].private, closure[it + 1].private);
  }
}

UTEST(closure, transitive_static_chain) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC, .deps = { { "spam" } } },
        { .name = "spam", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum" }, { "spam" } },
  });
}

UTEST(closure, shared_boundary_stops_recursion) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" } } },
        { .name = "spum", .kind = SPN_LIB_KIND_SHARED, .deps = { { "spam" } } },
        { .name = "spam", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum" } },
  });
}

UTEST(closure, diamond_dedups_shared_dependency) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" }, { "spam" } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC, .deps = { { "grum" } } },
        { .name = "spam", .kind = SPN_LIB_KIND_STATIC, .deps = { { "grum" } } },
        { .name = "grum", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spam" }, { "spum" }, { "grum" } },
  });
}

UTEST(closure, unreferenced_package_absent) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
        { .name = "orphan", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum" } },
  });
}

// A build dep is a tool in its own unit; it never lands on the link line
UTEST(closure, build_edge_excluded) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" }, { "tool", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
        { .name = "tool", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum" } },
  });
}

UTEST(closure, dep_kind_audience) {
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_LIB));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_EXE));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_SCRIPT));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_TEST));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_CONFIGURE_METAPROGRAM));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_PACKAGE, SPN_TARGET_BUILD_METAPROGRAM));

  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_TEST, SPN_TARGET_LIB));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_TEST, SPN_TARGET_EXE));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_TEST, SPN_TARGET_TEST));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_TEST, SPN_TARGET_CONFIGURE_METAPROGRAM));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_TEST, SPN_TARGET_BUILD_METAPROGRAM));

  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_BUILD, SPN_TARGET_LIB));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_BUILD, SPN_TARGET_EXE));
  EXPECT_FALSE(spn_dep_kind_applies(SPN_DEP_KIND_BUILD, SPN_TARGET_TEST));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_BUILD, SPN_TARGET_CONFIGURE_METAPROGRAM));
  EXPECT_TRUE(spn_dep_kind_applies(SPN_DEP_KIND_BUILD, SPN_TARGET_BUILD_METAPROGRAM));
}

UTEST(closure, build_metaprogram_links_only_build_edges) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .target = SPN_TARGET_BUILD_METAPROGRAM,
      .pkgs = {
        { .name = "test", .deps = { { "spum" }, { "tool", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
        { .name = "tool", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "tool" } },
  });
}

UTEST(closure, build_edge_invisible_transitively) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .target = SPN_TARGET_BUILD_METAPROGRAM,
      .pkgs = {
        { .name = "test", .deps = { { "tool", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "tool", .kind = SPN_LIB_KIND_STATIC, .deps = { { "spum" }, { "gen", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
        { .name = "gen", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "tool" }, { "spum" } },
  });
}

// A test dep links into test executables and nothing else
UTEST(closure, test_edge_only_links_tests) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .target = SPN_TARGET_TEST,
      .pkgs = {
        { .name = "test", .deps = { { "spum", .kind = SPN_DEP_KIND_TEST } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum" } },
  });
}

UTEST(closure, test_edge_excluded_from_exe) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum", .kind = SPN_DEP_KIND_TEST } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = sp_zero,
  });
}

// Privacy inherits down the private edge: everything reached through a
// private dep is private to the link unit
UTEST(closure, private_propagates) {
  run_test(utest_result, (closure_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum", .private = true } } },
        { .name = "spum", .kind = SPN_LIB_KIND_STATIC, .deps = { { "spam" } } },
        { .name = "spam", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = { { "spum", .private = true }, { "spam", .private = true } },
  });
}
