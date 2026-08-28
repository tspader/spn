#include "spn_test.h"

#include "reflect/reflect.h"
#include "source_deps.gen.h"

#define REFLECT_TEST_MAX_INCLUDES 16

typedef struct {
  const c8* version;
  const c8* source;
  const c8* includes [REFLECT_TEST_MAX_INCLUDES];
} expect_t;

typedef struct {
  const c8* name;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "empty",
    .expect = {
      .version = "V",
      .source = "A",
    },
  },
  {
    .name = "one",
    .expect = {
      .version = "V",
      .source = "A",
      .includes = { "B" },
    },
  },
  {
    .name = "many",
    .expect = {
      .version = "V",
      .source = "A",
      .includes = { "B", "C", "D", "E", "F", "G", "H\\I", "J" },
    },
  },
  {
    .name = "modules",
    .expect = {
      .version = "V",
      .source = "A",
      .includes = { "B" },
    },
  },
};

sp_test_each(reflect_read, deps, test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_str_t path = test_repo_path(mem, sp_fmt(mem, "test/core/reflect/deps/{}.json", sp_fmt_cstr(it->name)).value);
  sp_str_t json = sp_zero;
  sp_must(t, !sp_io_read_file(mem, path, &json));

  spn_cg_source_deps_t deps = sp_zero;
  sp_must_eq(t, SPN_OK, spn_reflect_json_read(json, &spn_reflect_source_deps, &deps, mem));

  sp_expect_str_eq_c(t, deps.version, it->expect.version);
  sp_expect_str_eq_c(t, deps.data.source, it->expect.source);
  sp_must_strs_eq(t, deps.data.includes, sp_da_size(deps.data.includes), it->expect.includes);
  return SP_OK;
}
