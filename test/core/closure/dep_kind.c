#include "closure.h"

typedef struct {
  bool lib;
  bool exe;
  bool script;
  bool test;
  bool configure;
  bool build;
} expect_t;

typedef struct {
  const c8* name;
  spn_dep_kind_t kind;
  expect_t expect;
} test_t;

static const test_t tests [] = {
  {
    .name = "package",
    .kind = SPN_DEP_KIND_PACKAGE,
    .expect = { .lib = true, .exe = true, .script = true, .test = true },
  },
  {
    .name = "test",
    .kind = SPN_DEP_KIND_TEST,
    .expect = { .test = true },
  },
  {
    .name = "build",
    .kind = SPN_DEP_KIND_BUILD,
    .expect = { .configure = true, .build = true },
  },
};

sp_test_each(dep_kind, applies, test_t, tests) {
  sp_expect_eq(t, it->expect.lib, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_LIB));
  sp_expect_eq(t, it->expect.exe, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_EXE));
  sp_expect_eq(t, it->expect.script, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_SCRIPT));
  sp_expect_eq(t, it->expect.test, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_TEST));
  sp_expect_eq(t, it->expect.configure, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_CONFIGURE_METAPROGRAM));
  sp_expect_eq(t, it->expect.build, spn_dep_kind_applies(it->kind, SPN_TARGET_KIND_BUILD_METAPROGRAM));

  return SP_OK;
}
