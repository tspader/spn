#include "toolchain.h"

typedef struct {
  spn_path_check_t check;
  test_path_t path;
} expect_t;

typedef struct {
  const c8* name;
  spn_toolchain_source_t source;
  spn_path_root_t base;
  const c8* str;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { "local_absolute",                     SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_NONE,    "/S",     .expect = { .path = { "/S" } } },
  { "local_absolute_ignores_base",        SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, "/S",     .expect = { .path = { "/S" } } },
  { "local_relative_under_base",          SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, "S",      .expect = { .path = { "S", SPN_PATH_ROOT_PROJECT } } },
  { "local_relative_without_base",        SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_NONE,    "S",      .expect = { .check = SPN_PATH_UNROOTED } },
  { "local_malformed",                    SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, "./S",    .expect = { .check = SPN_PATH_MALFORMED } },
  { "mixed_roots_like_local",             SPN_TOOLCHAIN_SOURCE_MIXED,        SPN_PATH_ROOT_PROJECT, "S",      .expect = { .path = { "S", SPN_PATH_ROOT_PROJECT } } },
  { "distribution_relative",              SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "S",      .expect = { .path = { "S" } } },
  { "distribution_relative_ignores_base", SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_PROJECT, "S/T",    .expect = { .path = { "S/T" } } },
  { "distribution_absolute",              SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "/S",     .expect = { .check = SPN_PATH_UNROOTED } },
  { "distribution_malformed",             SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "S/../T", .expect = { .check = SPN_PATH_MALFORMED } },
};

sp_test_each(path, classify, test_t, tests) {
  spn_path_t path = sp_zero;
  spn_path_check_t check = spn_toolchain_path(it->source, it->base, sp_cstr_as_str(it->str), &path);
  sp_must_eq(t, (u32)it->expect.check, (u32)check);
  return test_check_path(t, path, it->expect.path);
}
