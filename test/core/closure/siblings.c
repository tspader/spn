#include "closure.h"

#include "utest.h"

static void run_test(s32* utest_result, target_list_test_t t) {
  closure_graph_t g = build_graph(&t.graph);
  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);
  ASSERT_LE(1u, sp_da_size(closure));
  EXPECT_TRUE(closure[0].pkg == g.root->pkg);
  expect_target_names(utest_result, closure[0].targets, t.expect, CLOSURE_TEST_MAX_NAMES);
}

UTEST(siblings, order_depender_first) {
  run_test(utest_result, (target_list_test_t) {
    .graph = {
      .root = "test",
      .root_deps = { "util" },
      .pkgs = {
        { .name = "test", .libs = {
          { "util", SPN_LIB_KIND_STATIC, .deps = { "base" } },
          { "base", SPN_LIB_KIND_STATIC },
        } },
      },
    },
    .expect = { "util", "base" },
  });
}

// A shared sibling is its own link unit: it appears, but its deps are its
// own problem and are not walked into the consumer
UTEST(siblings, stop_at_shared) {
  run_test(utest_result, (target_list_test_t) {
    .graph = {
      .root = "test",
      .root_deps = { "util" },
      .pkgs = {
        { .name = "test", .libs = {
          { "util", SPN_LIB_KIND_SHARED, .deps = { "base" } },
          { "base", SPN_LIB_KIND_STATIC },
        } },
      },
    },
    .expect = { "util" },
  });
}

UTEST(siblings, dedupe_diamond) {
  run_test(utest_result, (target_list_test_t) {
    .graph = {
      .root = "test",
      .root_deps = { "a", "b" },
      .pkgs = {
        { .name = "test", .libs = {
          { "a", SPN_LIB_KIND_STATIC, .deps = { "c" } },
          { "b", SPN_LIB_KIND_STATIC, .deps = { "c" } },
          { "c", SPN_LIB_KIND_STATIC },
        } },
      },
    },
    .expect = { "b", "a", "c" },
  });
}
