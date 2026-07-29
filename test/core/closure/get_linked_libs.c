#include "closure.h"

typedef struct {
  const c8* pkg;
  const c8* lib;
  bool private;
} expect_t;

typedef struct {
  const c8* name;
  closure_graph_test_t graph;
  expect_t expect [CLOSURE_TEST_MAX_NAMES];
} test_t;

static const test_t tests [] = {
  {
    .name = "flatten_by_linkage",
    .graph = {
      .root = "P1",
      .pkgs = {
        {
          .name = "P1",
          .deps = { { "D1" } }
        },
        {
          .name = "D1",
          .kind = SPN_LIB_KIND_STATIC,
          .libs = {
            { "L1", SPN_LIB_KIND_SHARED },
            { "L2", SPN_LIB_KIND_OBJECT },
            { "L3", SPN_LIB_KIND_SOURCE },
            { "L4", SPN_LIB_KIND_STATIC, .no_link = true },
          }
        },
      },
    },
    .expect = {
      { "D1", "D1" },
      { "D1", "L1" }
    },
  },
  {
    .name = "order_follows_closure",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1" } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1", "D1" },
      { "D2", "D2" }
    },
  },
  {
    .name = "carry_privacy",
    .graph = {
      .root = "P1",
      .pkgs = {
        { .name = "P1", .deps = { { "D1", .private = true } } },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC, .deps = { { "D2" } } },
        { .name = "D2", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "D1", "D1", .private = true },
      { "D2", "D2", .private = true } },
  },
  {
    .name = "lead_with_siblings",
    .graph = {
      .root = "P1",
      .root_deps = { "L1" },
      .pkgs = {
        {
          .name = "P1",
          .deps = { { "D1" } },
          .libs = {
            { "L1", SPN_LIB_KIND_STATIC, .deps = { "L2" } },
            { "L2", SPN_LIB_KIND_STATIC },
            { "L3", SPN_LIB_KIND_STATIC, .no_link = true },
          }
        },
        { .name = "D1", .kind = SPN_LIB_KIND_STATIC },
      },
    },
    .expect = {
      { "P1", "L1" },
      { "P1", "L2" },
      { "D1", "D1" }
    },
  },
};

sp_test_each(get_linked_libs, flatten, test_t, tests) {
  closure_graph_t g = build_graph(&it->graph);

  sp_da(spn_closure_entry_t) closure = spn_target_link_closure(g.mem, g.root);
  sp_da(spn_link_lib_t) libs = spn_closure_get_linked_libs(g.mem, closure);

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, it->expect[expected].lib);
  sp_must_eq(t, expected, sp_da_size(libs));
  sp_for(row, expected) {
    sp_expect_str_eq_c(t, libs[row].pkg->info->name, it->expect[row].pkg);
    sp_expect_str_eq_c(t, libs[row].lib->info->name, it->expect[row].lib);
    sp_expect_eq(t, it->expect[row].private, libs[row].private);
  }

  return SP_OK;
}
