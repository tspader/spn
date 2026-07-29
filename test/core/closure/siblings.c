#include "closure.h"

static const target_list_test_t tests [] = {
  {
    .name = "order_depender_first",
    .graph = {
      .root = "P1",
      .root_deps = { "L1" },
      .pkgs = {
        {
          .name = "P1",
          .libs = {
            { "L1", SPN_LIB_KIND_STATIC, .deps = { "L2" } },
            { "L2", SPN_LIB_KIND_STATIC },
          }
        },
      },
    },
    .expect = { "L1", "L2" },
  },
  // A shared sibling is its own link unit: it appears, but its deps are its
  // own problem and are not walked into the consumer
  {
    .name = "stop_at_shared",
    .graph = {
      .root = "P1",
      .root_deps = { "L1" },
      .pkgs = {
        {
          .name = "P1",
          .libs = {
            { "L1", SPN_LIB_KIND_SHARED, .deps = { "L2" } },
            { "L2", SPN_LIB_KIND_STATIC },
          }
        },
      },
    },
    .expect = { "L1" },
  },
  {
    .name = "dedupe_diamond",
    .graph = {
      .root = "P1",
      .root_deps = { "L1", "L2" },
      .pkgs = {
        {
          .name = "P1",
          .libs = {
            { "L1", SPN_LIB_KIND_STATIC, .deps = { "L3" } },
            { "L2", SPN_LIB_KIND_STATIC, .deps = { "L3" } },
            { "L3", SPN_LIB_KIND_STATIC },
          }
        },
      },
    },
    .expect = { "L2", "L1", "L3" },
  },
};

sp_test_each(siblings, collect, target_list_test_t, tests) {
  closure_graph_t g = build_graph(&it->graph);

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);
  sp_must_le(t, 1u, sp_da_size(closure));
  sp_expect(t, closure[0].pkg == g.root->pkg);

  return expect_target_names(t, closure[0].targets, it->expect, CLOSURE_TEST_MAX_NAMES);
}
