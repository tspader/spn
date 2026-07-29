#include "closure.h"

static const target_list_test_t tests [] = {
  // Runtime collection crosses shared boundaries the link closure stops at: a
  // shared lib's own shared deps must still sit next to the executable
  {
    .name = "collect_shared_transitively",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_SHARED, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D3" } } },
        { .name = "D3", .kind = SPN_LIB_KIND_SHARED },
      },
    },
    .expect = { "D3", "D1" },
  },
  {
    .name = "include_sibling_shared",
    .graph = {
      .root = "P1",
      .root_deps = { "L1", "L2" },
      .pkgs = {
        {
          .name = "P1",
          .deps = { { "D1" } },
          .libs = {
            { "L1", SPN_LIB_KIND_SHARED },
            { "L2", SPN_LIB_KIND_SHARED, .no_link = true },
          }
        },
        { .name = "D1", .kind = SPN_LIB_KIND_SHARED },
      },
    },
    .expect = { "L1", "D1" },
  },
};

sp_test_each(runtime_libs, collect, target_list_test_t, tests) {
  closure_graph_t g = build_graph(&it->graph);

  sp_da(spn_target_unit_t*) libs = spn_target_runtime_libs(g.mem, g.root);

  return expect_target_names(t, libs, it->expect);
}
