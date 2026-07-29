#include "dag_test.h"

typedef struct {
  spn_dag_root_t root;
  const c8* sub;
} roots_expect_t;

typedef struct {
  const c8* name;
  dag_test_roots_t roots;
  const c8* path;
  roots_expect_t expect;
} roots_test_t;

static const roots_test_t roots_tests [] = {
  {
    .name = "no_roots_passes_through",
    .path = "/A/H",
    .expect = { .sub = "/A/H" }
  },
  {
    .name = "no_match_passes_through",
    .roots = { .project = "/A" },
    .path = "/X/H",
    .expect = { .sub = "/X/H" }
  },
  {
    .name = "match_strips_prefix",
    .roots = { .project = "/A" },
    .path = "/A/D/H",
    .expect = { .root = SPN_DAG_ROOT_PROJECT, .sub = "D/H" }
  },
  {
    .name = "exact_match_has_empty_sub",
    .roots = { .project = "/A" },
    .path = "/A",
    .expect = { .root = SPN_DAG_ROOT_PROJECT, .sub = "" }
  },
  {
    .name = "longest_root_wins",
    .roots = { .project = "/A", .store = "/A/S" },
    .path = "/A/S/H",
    .expect = { .root = SPN_DAG_ROOT_STORE, .sub = "H" }
  },
  {
    .name = "match_requires_component_boundary",
    .roots = { .project = "/A" },
    .path = "/AB/H",
    .expect = { .sub = "/AB/H" }
  },
};

sp_test_each(roots, collapse_expand, roots_test_t, roots_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build(it->roots, &storage);
  sp_str_t path = sp_str_view(it->path);

  spn_dag_prefixed_t prefixed = spn_dag_root_collapse(roots, path);
  sp_expect_eq(t, it->expect.root, prefixed.root);
  sp_expect_str_eq_c(t, prefixed.sub, it->expect.sub);

  sp_str_t expanded = sp_zero;
  sp_expect(t, spn_dag_root_expand(roots, prefixed, mem, &expanded));
  sp_expect_str_eq_c(t, expanded, it->path);
  return SP_OK;
}

sp_test(roots, expand_unknown_root_fails) {
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build((dag_test_roots_t) { .project = "/A" }, &storage);
  spn_dag_prefixed_t prefixed = { .root = SPN_DAG_ROOT_STORE, .sub = sp_str_lit("H") };
  sp_str_t expanded = sp_zero;
  sp_expect(t, !spn_dag_root_expand(roots, prefixed, sp_test_arena(t), &expanded));
  return SP_OK;
}
