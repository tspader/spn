#include "closure.h"

typedef struct {
  const c8* pkg;
  bool private;
} expect_t;

typedef struct {
  const c8* name;
  closure_graph_test_t graph;
  expect_t expect [CLOSURE_TEST_MAX_NAMES];
} test_t;

static const test_t tests [] = {
  {
    .name = "walk_static_chain",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" },
      { "D2" }
    },
  },
  {
    .name = "stop_at_shared",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_SHARED, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" }
    },
  },
  {
    .name = "dedupe_diamond",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D3" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D3" } } },
        { .name = "D3", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D2" },
      { "D1" },
      { "D3" }
    },
  },
  {
    .name = "skip_unreferenced",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" }
    },
  },
  // A build dep is a tool in its own unit; it never lands on the link line
  {
    .name = "skip_build_edges",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" }
    },
  },
  {
    .name = "metaprogram_links_build_edges",
    .graph = {
      .root = "P1",
      .target = SPN_TARGET_KIND_BUILD_METAPROGRAM,
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D2" }
    },
  },
  {
    .name = "hide_transitive_build_edges",
    .graph = {
      .root = "P1",
      .target = SPN_TARGET_KIND_BUILD_METAPROGRAM,
      .pkgs = {
        { .name = "P1", .deps = { { "D1", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D2" }, { "D3", .kind = SPN_DEP_KIND_BUILD } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
        { .name = "D3", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" },
      { "D2" }
    },
  },
  // A test dep links into test executables and nothing else
  {
    .name = "link_test_edges_in_tests",
    .graph = {
      .root = "P1",
      .target = SPN_TARGET_KIND_TEST,
      .pkgs = {
        { .name = "P1", .deps = { { "D1", .kind = SPN_DEP_KIND_TEST } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1" }
    },
  },
  {
    .name = "skip_test_edges_in_exe",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1", .kind = SPN_DEP_KIND_TEST } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
      },
    },
  },
  // Privacy inherits down the private edge: everything reached through a
  // private dep is private to the link unit
  {
    .name = "propagate_privacy",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1", .private = true } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1", .private = true },
      { "D2", .private = true }
    },
  },
};

sp_test_each(closure, walk, test_t, tests) {
  closure_graph_t g = build_graph(&it->graph);

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected].pkg);

  // The closure leads with the link unit's own package; the expectations
  // describe everything linked in behind it
  sp_must_eq(t, expected + 1, sp_da_size(closure));
  sp_expect(t, closure[0].pkg == g.root->pkg);
  sp_expect(t, !closure[0].private);
  sp_for(row, expected) {
    sp_expect_str_eq_c(t, closure[row + 1].pkg->info->name, it->expect[row].pkg);
    sp_expect_eq(t, it->expect[row].private, closure[row + 1].private);
  }

  return SP_OK;
}
