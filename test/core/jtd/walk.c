#include "jtd_test.h"

typedef struct {
  sp_da(sp_str_t) paths;
} walk_collect_t;

static bool walk_collect_visit(jtd_schema_t* s, sp_str_t path, void* user) {
  (void)s;
  walk_collect_t* c = (walk_collect_t*)user;
  sp_da_push(c->paths, path);
  return true;
}

static sp_err_t compare_walk_preorder(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  walk_collect_t c = { .paths = sp_da_new(sp_test_arena(t), sp_str_t) };
  jtd_walk(sp_test_arena(t), root, walk_collect_visit, &c);

  sp_must_eq(t, (u64)4, (u64)sp_da_size(c.paths));
  sp_expect_str_eq_c(t, c.paths[0], "#");
  sp_expect_str_eq_c(t, c.paths[1], "#/elements");
  sp_expect_str_eq_c(t, c.paths[2], "#/definitions/dep");
  sp_expect_str_eq_c(t, c.paths[3], "#/definitions/dep/properties/v");

  sp_expect(t, jtd_resolve(root, root->root) == root->root);
  return SP_OK;
}

typedef struct {
  u32 stop;
  u32 seen;
} walk_stop_t;

static bool walk_stop_visit(jtd_schema_t* s, sp_str_t path, void* user) {
  (void)s;
  (void)path;
  walk_stop_t* w = (walk_stop_t*)user;
  w->seen++;
  return w->seen < w->stop;
}

static sp_err_t compare_walk_stops(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  walk_stop_t w = { .stop = 2, .seen = 0 };
  jtd_walk(sp_test_arena(t), root, walk_stop_visit, &w);
  sp_expect_eq(t, (u64)2, (u64)w.seen);
  return SP_OK;
}

static sp_err_t compare_walk_escaped_paths(sp_test_t* t, const jtd_result_t* root, const void* expect) {
  (void)expect;
  walk_collect_t c = { .paths = sp_da_new(sp_test_arena(t), sp_str_t) };
  jtd_walk(sp_test_arena(t), root, walk_collect_visit, &c);

  sp_must_eq(t, (u64)3, (u64)sp_da_size(c.paths));
  sp_expect_str_eq_c(t, c.paths[0], "#");
  sp_expect_str_eq_c(t, c.paths[1], "#/properties/a~1b");
  sp_expect_str_eq_c(t, c.paths[2], "#/optionalProperties/c~0d");
  return SP_OK;
}

static const jtd_case_t cases [] = {
  {
    .name    = "preorder_paths",
    .json    = "walk.preorder.json",
    .compare = compare_walk_preorder,
  },
  {
    .name    = "stops_early",
    .json    = "walk.nested.json",
    .compare = compare_walk_stops,
  },
  {
    .name    = "escaped_paths",
    .json    = "walk.escaped.json",
    .compare = compare_walk_escaped_paths,
  },
};

sp_test_each_fn(walk, visit, jtd_case_t, cases, run_jtd_case);
