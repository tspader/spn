#include "toolchain.h"

typedef struct {
  spn_path_check_t check;
  test_arg_t arg;
} expect_t;

typedef struct {
  const c8* name;
  spn_toolchain_source_t source;
  spn_path_root_t base;
  const c8* program;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  { "local_name",                  SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_NONE,    "A",    .expect = { .arg = { .name = "A" } } },
  { "local_name_ignores_base",     SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, "A",    .expect = { .arg = { .name = "A" } } },
  { "local_dot_name",              SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, ".",    .expect = { .check = SPN_PATH_MALFORMED } },
  { "local_absolute",              SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_NONE,    "/A/B", .expect = { .arg = { .path = "/A/B" } } },
  { "local_relative_under_base",   SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_PROJECT, "A/B",  .expect = { .arg = { .path = "A/B", .root = SPN_PATH_ROOT_PROJECT } } },
  { "local_relative_without_base", SPN_TOOLCHAIN_SOURCE_LOCAL,        SPN_PATH_ROOT_NONE,    "A/B",  .expect = { .check = SPN_PATH_UNROOTED } },
  { "distribution_name",           SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "A",    .expect = { .arg = { .path = "A" } } },
  { "distribution_relative",       SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "A/B",  .expect = { .arg = { .path = "A/B" } } },
  { "distribution_absolute",       SPN_TOOLCHAIN_SOURCE_DISTRIBUTION, SPN_PATH_ROOT_NONE,    "/A",   .expect = { .check = SPN_PATH_ABSOLUTE } },
};

sp_test_each(program, classify, test_t, tests) {
  spn_arg_t arg = sp_zero;
  spn_path_check_t check = spn_toolchain_program(it->source, it->base, sp_cstr_as_str(it->program), &arg);
  sp_must_eq(t, (u32)it->expect.check, (u32)check);
  return test_check_arg(t, arg, it->expect.arg);
}
