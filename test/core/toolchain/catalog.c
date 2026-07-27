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
  const c8* file;
  catalog_add_t adds [CATALOG_MAX_ADDS];
  catalog_expect_t expect;
} catalog_test_t;

static void run_catalog_test(s32* utest_result, catalog_test_t t) {
  spn_toolchain_catalog_t catalog = sp_zero;
  fixture_catalog(utest_result, &catalog, t.file);

  sp_carr_for(t.adds, it) {
    if (!t.adds[it].name) {
      break;
    }
    spn_toolchain_catalog_add(&catalog, fixture_local_toolchain(t.adds[it].name, t.adds[it].compiler));
  }

  ASSERT_EQ(t.expect.entries, fixture_catalog_size(&catalog));

  sp_carr_for(t.expect.present, it) {
    if (!t.expect.present[it]) {
      break;
    }
    EXPECT_TRUE(spn_toolchain_catalog_get(&catalog, sp_str_view(t.expect.present[it])));
  }

  sp_carr_for(t.expect.order, it) {
    if (!t.expect.order[it]) {
      break;
    }
    EXPECT_STR(fixture_catalog_at(&catalog, it)->name, t.expect.order[it]);
  }

  sp_carr_for(t.expect.toolchains, it) {
    fixture_toolchain_t toolchain = t.expect.toolchains[it];
    if (!toolchain.name) {
      break;
    }
    fixture_check_entry(utest_result, spn_toolchain_catalog_get(&catalog, sp_str_view(toolchain.name)), toolchain);
  }
}

UTEST(catalog, add_overrides_by_name) {
  run_catalog_test(utest_result, (catalog_test_t) {
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
  });
}

UTEST(catalog, add_coexists_with_entries) {
  run_catalog_test(utest_result, (catalog_test_t) {
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
  });
}
