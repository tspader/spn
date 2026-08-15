#include "unit.h"

typedef struct {
  spn_path_root_t root;
  const c8* sub;
  const c8* prefix;
  const c8* suffix;
} path_expect_t;

typedef struct {
  const c8* name;
  spn_pkg_source_t source;
  path_expect_t work;
  path_expect_t store;
  path_expect_t include;
  path_expect_t object;
  path_expect_t stamp;
} paths_test_t;

static const paths_test_t tests [] = {
  {
    .name = "root_package_lives_under_the_build_root",
    .source = SPN_PKG_SOURCE_ROOT,
    .work =    { .sub = "/build/debug/.spn/A" },
    .store =   { .sub = "/build/debug/store/A" },
    .include = { .sub = "/build/debug/store/A/include" },
    .object =  { .sub = "/build/debug/.spn/A/object" },
    .stamp =   { .sub = "/build/debug/.spn/A/stamp/configure.stamp" },
  },
  {
    .name = "file_package_lives_under_the_build_root",
    .source = SPN_PKG_SOURCE_FILE,
    .work =    { .sub = "/build/debug/.spn/A" },
    .store =   { .sub = "/build/debug/store/A" },
    .include = { .sub = "/build/debug/store/A/include" },
    .object =  { .sub = "/build/debug/.spn/A/object" },
    .stamp =   { .sub = "/build/debug/.spn/A/stamp/configure.stamp" },
  },
  {
    .name = "index_package_lives_under_the_build_and_store_roots",
    .source = SPN_PKG_SOURCE_INDEX,
    .work =    { .root = SPN_PATH_ROOT_BUILD, .prefix = "A/" },
    .store =   { .root = SPN_PATH_ROOT_STORE, .prefix = "A/" },
    .include = { .root = SPN_PATH_ROOT_STORE, .prefix = "A/", .suffix = "/include" },
    .object =  { .root = SPN_PATH_ROOT_BUILD, .prefix = "A/", .suffix = "/object" },
    .stamp =   { .root = SPN_PATH_ROOT_BUILD, .prefix = "A/", .suffix = "/stamp/configure.stamp" },
  },
};

static sp_err_t expect_path(sp_test_t* t, spn_path_t actual, const path_expect_t* expect) {
  sp_expect_eq(t, actual.root, expect->root);
  if (expect->sub) {
    sp_expect_str_eq_c(t, actual.sub, expect->sub);
  }
  if (expect->prefix) {
    sp_expect(t, sp_str_starts_with(actual.sub, sp_str_view(expect->prefix)));
  }
  if (expect->suffix) {
    sp_expect(t, sp_str_ends_with(actual.sub, sp_str_view(expect->suffix)));
  }
  return SP_OK;
}

sp_test_each(unit_paths, init, paths_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  unit_graph_test_t graph = { .pkgs = { { .name = "A" } } };
  spn_session_t* s = build_session(mem, &graph);

  spn_pkg_id_t id = find_pkg_id(s, &graph, "A");
  spn_loaded_pkg_t* loaded = sp_ht_getp(s->packages, id);
  loaded->source = it->source;

  spn_pkg_unit_t unit = {
    .id = { .pkg = id, .build = s->units.target->id },
    .build = s->units.target,
    .session = s,
  };
  spn_unit_paths_init(&unit, loaded);

  sp_try(expect_path(t, unit.paths.work, &it->work));
  sp_try(expect_path(t, unit.paths.store, &it->store));
  sp_try(expect_path(t, unit.paths.include, &it->include));
  sp_try(expect_path(t, unit.paths.object, &it->object));
  sp_try(expect_path(t, unit.paths.stamp.configure, &it->stamp));

  return SP_OK;
}
