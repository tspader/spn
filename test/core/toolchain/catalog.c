#include "fixture.h"

#define CATALOG_MAX_ADDS 2
#define CATALOG_MAX_PRESENT 4
#define CATALOG_MAX_TOOLCHAINS 2

typedef struct {
  const c8* name;
  const c8* compiler;
} catalog_add_t;

typedef struct {
  u32 entries;
  const c8* present [CATALOG_MAX_PRESENT];
  const c8* order [CATALOG_MAX_PRESENT];
  fixture_toolchain_t toolchains [CATALOG_MAX_TOOLCHAINS];
} catalog_expect_t;

typedef struct {
  const c8* name;
  const c8* file;
  catalog_add_t adds [CATALOG_MAX_ADDS];
  catalog_expect_t expect;
} catalog_test_t;

static const catalog_test_t tests [] = {
  {
    .name = "overrides_by_name",
    .file = "multiple.json",
    .adds = {
      { .name = "A", .compiler = "/A" },
    },
    .expect = {
      .entries = 2,
      .present = { "B" },
      .order = { "A", "B" },
      .toolchains = {
        {
          .name = "A",
          .driver = SPN_CC_DRIVER_GCC,
          .compiler = { .program = "/A" },
        },
      },
    },
  },
  {
    .name = "coexists_with_entries",
    .file = "multiple.json",
    .adds = {
      { .name = "C", .compiler = "C" },
    },
    .expect = {
      .entries = 3,
      .present = { "A", "B", "C" },
      .order = { "A", "B", "C" },
      .toolchains = {
        { .name = "D", .absent = true },
      },
    },
  },
};

sp_test_each(catalog, add, catalog_test_t, tests) {
  spn_toolchain_catalog_t catalog = sp_zero;
  if (fixture_catalog(t, &catalog, it->file)) return SP_ERR;

  sp_carr_for(it->adds, at) {
    if (!it->adds[at].name) {
      break;
    }
    spn_toolchain_catalog_add(&catalog, fixture_local_toolchain(it->adds[at].name, it->adds[at].compiler));
  }

  sp_must_eq(t, it->expect.entries, fixture_catalog_size(&catalog));

  sp_carr_for(it->expect.present, at) {
    if (!it->expect.present[at]) {
      break;
    }
    sp_expect(t, spn_toolchain_catalog_get(&catalog, sp_str_view(it->expect.present[at])));
  }

  sp_carr_for(it->expect.order, at) {
    if (!it->expect.order[at]) {
      break;
    }
    sp_expect_str_eq_c(t, fixture_catalog_at(&catalog, at)->name, it->expect.order[at]);
  }

  sp_carr_for(it->expect.toolchains, at) {
    fixture_toolchain_t toolchain = it->expect.toolchains[at];
    if (!toolchain.name) {
      break;
    }
    if (fixture_check_entry(t, spn_toolchain_catalog_get(&catalog, sp_str_view(toolchain.name)), toolchain)) return SP_ERR;
  }

  return SP_OK;
}
