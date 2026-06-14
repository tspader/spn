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

static void compare_walk_preorder(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  walk_collect_t c = { .paths = sp_da_new(sp_str_t) };
  jtd_walk(root, walk_collect_visit, &c);

  EXPECT_EQ((u64)4, (u64)sp_da_size(c.paths));
  if (sp_da_size(c.paths) == 4) {
    jtd_expect_str(utest_result, c.paths[0], "#");
    jtd_expect_str(utest_result, c.paths[1], "#/elements");
    jtd_expect_str(utest_result, c.paths[2], "#/definitions/dep");
    jtd_expect_str(utest_result, c.paths[3], "#/definitions/dep/properties/v");
  }

  EXPECT_TRUE(jtd_resolve(root, root->root) == root->root);
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

static void compare_walk_stops(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  walk_stop_t w = { .stop = 2, .seen = 0 };
  jtd_walk(root, walk_stop_visit, &w);
  EXPECT_EQ((u64)2, (u64)w.seen);
}

static void compare_walk_escaped_paths(s32* utest_result, const jtd_root_t* root, const void* expect) {
  (void)expect;
  walk_collect_t c = { .paths = sp_da_new(sp_str_t) };
  jtd_walk(root, walk_collect_visit, &c);

  EXPECT_EQ((u64)3, (u64)sp_da_size(c.paths));
  if (sp_da_size(c.paths) == 3) {
    jtd_expect_str(utest_result, c.paths[0], "#");
    jtd_expect_str(utest_result, c.paths[1], "#/properties/a~1b");
    jtd_expect_str(utest_result, c.paths[2], "#/optionalProperties/c~0d");
  }
}

UTEST(walk, preorder_paths) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "walk.preorder.json",
    .compare = compare_walk_preorder,
  });
}

UTEST(walk, stops_early) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "walk.nested.json",
    .compare = compare_walk_stops,
  });
}

UTEST(walk, escaped_paths) {
  run_jtd_case(utest_result, (jtd_case_t){
    .json    = "walk.escaped.json",
    .compare = compare_walk_escaped_paths,
  });
}
