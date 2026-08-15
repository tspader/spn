#include "paths/paths_test.h"

typedef struct {
  const c8* name;
  const c8* path;
  bool normal;
} normal_test_t;

static const normal_test_t normal_tests [] = {
  { .name = "empty",             .path = "",         .normal = true },
  { .name = "bare",              .path = "a",        .normal = true },
  { .name = "nested",            .path = "a/b.c",    .normal = true },
  { .name = "absolute",          .path = "/a/b",     .normal = true },
  { .name = "root",              .path = "/",        .normal = true },
  { .name = "glob",              .path = "src/*.c",  .normal = true },
  { .name = "drive",             .path = "C:/a/b",   .normal = true },
  { .name = "hidden",            .path = ".spn/a",   .normal = true },
  { .name = "dot_prefixed_name", .path = "a/.c",     .normal = true },
  { .name = "ellipsis_name",     .path = "a/...b",   .normal = true },
  { .name = "dot",               .path = ".",        .normal = false },
  { .name = "dotdot",            .path = "..",       .normal = false },
  { .name = "leading_dot",       .path = "./a",      .normal = false },
  { .name = "leading_dotdot",    .path = "../a",     .normal = false },
  { .name = "interior_dotdot",   .path = "a/../b",   .normal = false },
  { .name = "trailing_dot",      .path = "a/.",      .normal = false },
  { .name = "trailing_dotdot",   .path = "a/..",     .normal = false },
  { .name = "absolute_dotdot",   .path = "/a/../b",  .normal = false },
  { .name = "double_slash",      .path = "a//b",     .normal = false },
  { .name = "trailing_slash",    .path = "a/",       .normal = false },
  { .name = "backslash_dotdot",  .path = "a\\..\\b", .normal = false },
};

sp_test_each(paths_normal, check, normal_test_t, normal_tests) {
  sp_expect_eq(t, spn_path_normal(sp_str_view(it->path)), it->normal);
  return SP_OK;
}
