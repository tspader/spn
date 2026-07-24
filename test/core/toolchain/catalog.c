#include "sp.h"
#include "utest.h"

#include "fixture.h"

#define CATALOG_MAX_ADDS 2
#define CATALOG_MAX_PRESENT 2
#define CATALOG_MAX_TOOLCHAINS 2

typedef struct {
  const c8* name;
  const c8* compiler;
} catalog_add_t;

typedef struct {
  const c8* file;
  catalog_add_t adds [CATALOG_MAX_ADDS];
  const c8* present [CATALOG_MAX_PRESENT];
  fixture_toolchain_t toolchains [CATALOG_MAX_TOOLCHAINS];
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

  sp_carr_for(t.present, it) {
    if (!t.present[it]) {
      break;
    }
    EXPECT_TRUE(spn_toolchain_catalog_get(&catalog, sp_str_view(t.present[it])));
  }

  sp_carr_for(t.toolchains, it) {
    fixture_toolchain_t toolchain = t.toolchains[it];
    if (!toolchain.name) {
      break;
    }
    fixture_check_entry(utest_result, spn_toolchain_catalog_get(&catalog, sp_str_view(toolchain.name)), toolchain);
  }
}

UTEST(catalog, add_overrides_builtin) {
  run_catalog_test(utest_result, (catalog_test_t) {
    .file = "zig.json",
    .adds = {
      { .name = "zig", .compiler = "/opt/zig/zig" },
    },
    .toolchains = {
      {
        .name = "zig",
        .driver = SPN_CC_DRIVER_GCC,
        .compiler = { .program = "/opt/zig/zig" },
      },
    },
  });
}

UTEST(catalog, add_keeps_builtins) {
  run_catalog_test(utest_result, (catalog_test_t) {
    .file = "zig_system.json",
    .adds = {
      { .name = "mingw", .compiler = "x86_64-w64-mingw32-gcc" },
    },
    .present = { "zig", "system" },
    .toolchains = {
      {
        .name = "mingw",
        .driver = SPN_CC_DRIVER_GCC,
        .compiler = { .program = "x86_64-w64-mingw32-gcc" },
      },
    },
  });
}

UTEST(catalog, unknown_name_is_null) {
  run_catalog_test(utest_result, (catalog_test_t) {
    .file = "zig.json",
    .toolchains = {
      { .name = "gcc-13", .absent = true },
    },
  });
}
