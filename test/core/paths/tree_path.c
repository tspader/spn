#include "paths/paths_test.h"

typedef struct {
  spn_path_root_t root;
  const c8* sub;
  const c8* resolved;
} tree_path_expect_t;

typedef struct {
  const c8* name;
  paths_test_roots_t roots;
  const c8* recipe;
  const c8* source;
  spn_tree_t decl;
  const c8* path;
  tree_path_expect_t expect;
} tree_path_test_t;

static const tree_path_test_t tree_path_tests [] = {
  {
    .name = "relative_source_joins_the_source_tree",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .decl = SPN_TREE_SOURCE, .path = "D/M.c",
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "S/D/M.c", .resolved = "/A/S/D/M.c" }
  },
  {
    .name = "relative_manifest_joins_the_recipe_tree",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .decl = SPN_TREE_MANIFEST, .path = "T/B.c",
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "R/T/B.c", .resolved = "/A/R/T/B.c" }
  },
  {
    .name = "empty_names_the_tree_root",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .decl = SPN_TREE_SOURCE, .path = "",
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "S", .resolved = "/A/S" }
  },
  {
    .name = "absolute_ignores_the_declared_tree",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .decl = SPN_TREE_MANIFEST, .path = "/A/S/Y.c",
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "S/Y.c", .resolved = "/A/S/Y.c" }
  },
  {
    .name = "absolute_outside_the_trees_keeps_its_root",
    .roots = { .project = "/P", .store = "/P/T" },
    .recipe = "/P/R", .source = "/P/T/S",
    .decl = SPN_TREE_SOURCE, .path = "/P/T/O/H.c",
    .expect = { .root = SPN_PATH_ROOT_STORE, .sub = "O/H.c", .resolved = "/P/T/O/H.c" }
  },
  {
    .name = "absolute_outside_the_roots_is_unrooted",
    .roots = { .project = "/A" },
    .recipe = "/A/R", .source = "/A/S",
    .decl = SPN_TREE_SOURCE, .path = "/B/Z.c",
    .expect = { .sub = "/B/Z.c", .resolved = "/B/Z.c" }
  },
  {
    .name = "unset_trees_leave_a_relative_declaration_relative",
    .decl = SPN_TREE_SOURCE, .path = "M.c",
    .expect = { .sub = "M.c", .resolved = "M.c" }
  },
  {
    .name = "relative_reaching_into_a_nested_root_takes_it",
    .roots = { .project = "/A", .toolchain = "/A/vendor/tc" },
    .recipe = "/A", .source = "/A",
    .decl = SPN_TREE_SOURCE, .path = "vendor/tc/include/x.h",
    .expect = { .root = SPN_PATH_ROOT_TOOLCHAIN, .sub = "include/x.h", .resolved = "/A/vendor/tc/include/x.h" }
  },
};

sp_test_each(paths_tree_path, resolve, tree_path_test_t, tree_path_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t storage = sp_zero;
  const spn_path_roots_t* roots = paths_test_roots_build(it->roots, &storage);

  spn_tree_roots_t trees = sp_zero;
  if (it->recipe) trees.recipe = spn_path_make(roots, sp_str_view(it->recipe));
  if (it->source) trees.source = spn_path_make(roots, sp_str_view(it->source));

  spn_path_t path = spn_tree_path(mem, roots, trees, it->decl, sp_str_view(it->path));
  sp_expect_eq(t, path.root, it->expect.root);
  sp_expect_str_eq_c(t, path.sub, it->expect.sub);
  sp_expect_str_eq_c(t, spn_path_str(roots, mem, path), it->expect.resolved);
  return SP_OK;
}
