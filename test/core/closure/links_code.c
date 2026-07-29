#include "closure.h"

typedef struct {
  const c8* pkg;
  bool links_code;
} expect_t;

typedef struct {
  const c8* name;
  closure_graph_test_t graph;
  expect_t expect [CLOSURE_TEST_MAX_NAMES];
} test_t;

static const test_t tests [] = {
  // links_code gates per-package link baggage (frameworks): header-only and
  // static packages contribute, shared-only and no_link-only packages do not
  {
    .name = "mark_static_linkers",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" }, { "D2" }, { "D3" }, { "D4" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
        { .name = "D2", .kind = SPN_LIB_KIND_SHARED },
        { .name = "D3" },
        { .name = "D4", .libs = { { "L1", SPN_LIB_KIND_STATIC, .no_link = true } } },
      },
    },
    .expect = {
      { "P1", .links_code = true },
      { "D4", .links_code = false },
      { "D3", .links_code = true },
      { "D2", .links_code = false },
      { "D1", .links_code = true },
    },
  },
};

sp_test_each(links_code, mark, test_t, tests) {
  closure_graph_t g = build_graph(&it->graph);

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected].pkg);
  sp_must_eq(t, expected, sp_da_size(closure));
  sp_for(row, expected) {
    sp_expect_str_eq_c(t, closure[row].pkg->info->name, it->expect[row].pkg);
    sp_expect_eq(t, it->expect[row].links_code, closure[row].links_code);
  }

  return SP_OK;
}
