#include "paths/paths_test.h"

typedef struct {
  spn_path_root_t root;
  const c8* sub;
} roots_expect_t;

typedef struct {
  const c8* name;
  paths_test_roots_t roots;
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
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "D/H" }
  },
  {
    .name = "exact_match_has_empty_sub",
    .roots = { .project = "/A" },
    .path = "/A",
    .expect = { .root = SPN_PATH_ROOT_PROJECT, .sub = "" }
  },
  {
    .name = "longest_root_wins",
    .roots = { .project = "/A", .store = "/A/S" },
    .path = "/A/S/H",
    .expect = { .root = SPN_PATH_ROOT_STORE, .sub = "H" }
  },
  {
    .name = "match_requires_component_boundary",
    .roots = { .project = "/A" },
    .path = "/AB/H",
    .expect = { .sub = "/AB/H" }
  },
  {
    .name = "index_collapses",
    .roots = { .index = "/S/I" },
    .path = "/S/I/P/M",
    .expect = { .root = SPN_PATH_ROOT_INDEX, .sub = "P/M" }
  },
  {
    .name = "runtime_collapses",
    .roots = { .runtime = "/S/R" },
    .path = "/S/R/include/H",
    .expect = { .root = SPN_PATH_ROOT_RUNTIME, .sub = "include/H" }
  },
  {
    .name = "cache_collapses",
    .roots = { .cache = "/S/C" },
    .path = "/S/C/dag/store/H",
    .expect = { .root = SPN_PATH_ROOT_CACHE, .sub = "dag/store/H" }
  },
  {
    .name = "store_wins_inside_cache",
    .roots = { .cache = "/S/C", .store = "/S/C/store" },
    .path = "/S/C/store/H",
    .expect = { .root = SPN_PATH_ROOT_STORE, .sub = "H" }
  },
  {
    .name = "cache_keeps_siblings_of_store",
    .roots = { .cache = "/S/C", .store = "/S/C/store" },
    .path = "/S/C/dag/H",
    .expect = { .root = SPN_PATH_ROOT_CACHE, .sub = "dag/H" }
  },
  {
    .name = "toolchain_store_collapses",
    .roots = { .cache = "/S/C", .toolchain = "/S/C/toolchain" },
    .path = "/S/C/toolchain/D/bin/cc",
    .expect = { .root = SPN_PATH_ROOT_TOOLCHAIN, .sub = "D/bin/cc" }
  },
};

sp_test_each(paths_roots, collapse_expand, roots_test_t, roots_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t storage = sp_zero;
  const spn_path_roots_t* roots = paths_test_roots_build(it->roots, &storage);
  sp_str_t path = sp_str_view(it->path);

  spn_path_t collapsed = spn_path_make(roots, path);
  sp_expect_eq(t, it->expect.root, collapsed.root);
  sp_expect_str_eq_c(t, collapsed.sub, it->expect.sub);

  sp_str_t expanded = spn_path_str(roots, mem, collapsed);
  sp_expect_str_eq_c(t, expanded, it->path);
  return SP_OK;
}
