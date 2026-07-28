#include "common.h"

typedef struct {
  spn_dag_root_t root;
  const c8* sub;
} roots_expect_t;

typedef struct {
  dag_test_roots_t roots;
  const c8* path;
  roots_expect_t expect;
} roots_test_t;

UTEST_EMPTY_FIXTURE(roots)

static void run_test(s32* utest_result, roots_test_t t) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build(t.roots, &storage);
  sp_str_t path = sp_str_view(t.path);

  spn_dag_prefixed_t prefixed = spn_dag_root_collapse(roots, path);
  EXPECT_EQ(t.expect.root, prefixed.root);
  EXPECT_STR(prefixed.sub, t.expect.sub);

  sp_str_t expanded = sp_zero;
  EXPECT_TRUE(spn_dag_root_expand(roots, prefixed, s.mem, &expanded));
  EXPECT_STR(expanded, t.path);
  sp_mem_end_scratch(s);
}

UTEST_F(roots, no_roots_passes_through) {
  run_test(&ur, (roots_test_t) {
    .path = "/A/H",
    .expect = { .sub = "/A/H" }
  });
}

UTEST_F(roots, no_match_passes_through) {
  run_test(&ur, (roots_test_t) {
    .roots = { .project = "/A" },
    .path = "/X/H",
    .expect = { .sub = "/X/H" }
  });
}

UTEST_F(roots, match_strips_prefix) {
  run_test(&ur, (roots_test_t) {
    .roots = { .project = "/A" },
    .path = "/A/D/H",
    .expect = { .root = SPN_DAG_ROOT_PROJECT, .sub = "D/H" }
  });
}

UTEST_F(roots, exact_match_has_empty_sub) {
  run_test(&ur, (roots_test_t) {
    .roots = { .project = "/A" },
    .path = "/A",
    .expect = { .root = SPN_DAG_ROOT_PROJECT, .sub = "" }
  });
}

UTEST_F(roots, longest_root_wins) {
  run_test(&ur, (roots_test_t) {
    .roots = { .project = "/A", .store = "/A/S" },
    .path = "/A/S/H",
    .expect = { .root = SPN_DAG_ROOT_STORE, .sub = "H" }
  });
}

UTEST_F(roots, match_requires_component_boundary) {
  run_test(&ur, (roots_test_t) {
    .roots = { .project = "/A" },
    .path = "/AB/H",
    .expect = { .sub = "/AB/H" }
  });
}

UTEST_F(roots, expand_unknown_root_fails) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  spn_dag_roots_t storage = sp_zero;
  const spn_dag_roots_t* roots = dag_test_roots_build((dag_test_roots_t) { .project = "/A" }, &storage);
  spn_dag_prefixed_t prefixed = { .root = SPN_DAG_ROOT_STORE, .sub = sp_str_lit("H") };
  sp_str_t expanded = sp_zero;
  EXPECT_FALSE(spn_dag_root_expand(roots, prefixed, s.mem, &expanded));
  sp_mem_end_scratch(s);
}
