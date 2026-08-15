#include "paths/paths_test.h"

typedef struct {
  spn_path_root_t root;
  const c8* sub;
} path_t;

typedef struct {
  const c8* name;
  paths_test_roots_t roots;
  path_t path;
  path_t expect;
} anchor_test_t;

static const anchor_test_t anchor_tests [] = {
  {
    .name = "store_relative_is_identity",
    .roots = { .cache = "/S/cache", .store = "/S/cache/store", .build = "/S/cache/build" },
    .path = { SPN_PATH_ROOT_STORE, "pkg/fp/include/a.h" },
    .expect = { SPN_PATH_ROOT_STORE, "pkg/fp/include/a.h" }
  },
  {
    .name = "cache_relative_is_identity_outside_nested_roots",
    .roots = { .cache = "/S/cache", .store = "/S/cache/store" },
    .path = { SPN_PATH_ROOT_CACHE, "dag" },
    .expect = { SPN_PATH_ROOT_CACHE, "dag" }
  },
  {
    .name = "sub_reaching_into_a_nested_root_takes_it",
    .roots = { .project = "/A", .toolchain = "/A/vendor/tc" },
    .path = { SPN_PATH_ROOT_PROJECT, "vendor/tc/include/x.h" },
    .expect = { SPN_PATH_ROOT_TOOLCHAIN, "include/x.h" }
  },
  {
    .name = "name_sharing_nested_root_prefix_stays",
    .roots = { .project = "/A", .toolchain = "/A/vendor/tc" },
    .path = { SPN_PATH_ROOT_PROJECT, "vendor/tcx/x.h" },
    .expect = { SPN_PATH_ROOT_PROJECT, "vendor/tcx/x.h" }
  },
  {
    .name = "unrooted_path_under_root_takes_it",
    .roots = { .project = "/A" },
    .path = { SPN_PATH_ROOT_NONE, "/A/src/m.c" },
    .expect = { SPN_PATH_ROOT_PROJECT, "src/m.c" }
  },
  {
    .name = "unregistered_root_passes_through",
    .roots = { .project = "/A" },
    .path = { SPN_PATH_ROOT_BUILD, "pkg/fp" },
    .expect = { SPN_PATH_ROOT_BUILD, "pkg/fp" }
  },
};

sp_test_each(paths_anchor, check, anchor_test_t, anchor_tests) {
  sp_mem_t mem = sp_test_arena(t);
  spn_path_roots_t storage = sp_zero;
  const spn_path_roots_t* roots = paths_test_roots_build(it->roots, &storage);

  spn_path_t path = { .root = it->path.root, .sub = sp_str_view(it->path.sub) };
  spn_path_t anchored = spn_path_anchor(mem, roots, path);
  sp_expect_eq(t, anchored.root, it->expect.root);
  sp_expect_str_eq_c(t, anchored.sub, it->expect.sub);
  return SP_OK;
}
