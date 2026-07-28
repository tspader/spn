#include "closure.h"

#include "utest.h"

static void run_test(s32* utest_result, target_list_test_t t) {
  closure_graph_t g = build_graph(&t.graph);
  sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(g.mem, g.root);
  expect_target_names(utest_result, libs, t.expect, CLOSURE_TEST_MAX_NAMES);
}

// Runtime collection crosses shared boundaries the link closure stops at: a
// shared lib's own shared deps must still sit next to the executable
UTEST(runtime_libs, collect_shared_transitively) {
  run_test(utest_result, (target_list_test_t) {
    .graph = {
      .root = "test",
      .pkgs = {
        { .name = "test", .deps = { { "spum" } } },
        { .name = "spum", .kind = SPN_LIB_KIND_SHARED, .deps = { { "spam" } } },
        { .name = "spam", .kind = SPN_LIB_KIND_STATIC, .deps = { { "grum" } } },
        { .name = "grum", .kind = SPN_LIB_KIND_SHARED },
      },
    },
    .expect = { "grum", "spum" },
  });
}

UTEST(runtime_libs, include_sibling_shared) {
  run_test(utest_result, (target_list_test_t) {
    .graph = {
      .root = "test",
      .root_deps = { "plug", "dull" },
      .pkgs = {
        { .name = "test", .deps = { { "spum" } }, .libs = {
          { "plug", SPN_LIB_KIND_SHARED },
          { "dull", SPN_LIB_KIND_SHARED, .no_link = true },
        } },
        { .name = "spum", .kind = SPN_LIB_KIND_SHARED },
      },
    },
    .expect = { "plug", "spum" },
  });
}
