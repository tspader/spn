#include "paths/paths_test.h"

typedef struct {
  spn_tree_t tree;
  const c8* sub;
} tree_rel_expect_t;

typedef struct {
  const c8* name;
  paths_test_roots_t roots;
  const c8* recipe;
  const c8* source;
  const c8* path;
  tree_rel_expect_t expect;
} tree_rel_test_t;

static const tree_rel_test_t tree_rel_tests [] = {
  {
    .name = "equal_roots_prefer_the_recipe",
    .roots = { .project = "/A" },
    .recipe = "/A", .source = "/A",
    .path = "/A/M.c",
    .expect = { .tree = SPN_TREE_MANIFEST, .sub = "M.c" }
  },
  {
    .name = "recipe_nested_in_source_wins",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A",
    .path = "/A/R/M.c",
    .expect = { .tree = SPN_TREE_MANIFEST, .sub = "M.c" }
  },
  {
    .name = "source_nested_in_recipe_wins",
    .roots = { .project = "/A" },
    .recipe = "/A", .source = "/A/S",
    .path = "/A/S/F.c",
    .expect = { .tree = SPN_TREE_SOURCE, .sub = "F.c" }
  },
  {
    .name = "outside_a_nested_source_stays_manifest",
    .roots = { .project = "/A" },
    .recipe = "/A", .source = "/A/S",
    .path = "/A/G.c",
    .expect = { .tree = SPN_TREE_MANIFEST, .sub = "G.c" }
  },
  {
    .name = "sibling_of_a_tree_root_is_outside",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .path = "/A/SX/I.c",
    .expect = { .sub = "SX/I.c" }
  },
  {
    .name = "rooted_path_outside_the_trees_is_outside",
    .roots = { .project = "/P", .store = "/P/T" },
    .recipe = "/P/R", .source = "/P/T/S",
    .path = "/P/T/O/H.c",
    .expect = { .sub = "O/H.c" }
  },
  {
    .name = "trees_under_distinct_roots_stay_distinct",
    .roots = { .project = "/P", .store = "/P/T" },
    .recipe = "/P/R", .source = "/P/T/S",
    .path = "/P/T/S/H.c",
    .expect = { .tree = SPN_TREE_SOURCE, .sub = "H.c" }
  },
  {
    .name = "unset_trees_strip_the_leading_separator",
    .path = "/B/Q.c",
    .expect = { .sub = "B/Q.c" }
  },
};

sp_test_each(paths_tree_rel, classify, tree_rel_test_t, tree_rel_tests) {
  spn_path_roots_t storage = sp_zero;
  const spn_path_roots_t* roots = paths_test_roots_build(it->roots, &storage);

  spn_tree_roots_t trees = sp_zero;
  if (it->recipe) trees.recipe = spn_path_make(roots, sp_str_view(it->recipe));
  if (it->source) trees.source = spn_path_make(roots, sp_str_view(it->source));

  spn_tree_rel_t rel = spn_tree_rel(trees, spn_path_make(roots, sp_str_view(it->path)));
  sp_expect_eq(t, rel.tree, it->expect.tree);
  sp_expect_str_eq_c(t, rel.sub, it->expect.sub);
  return SP_OK;
}
