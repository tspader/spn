#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "ctx/types.h"
#include "intern/intern.h"
#include "pkg/pkg.h"
#include "pkg/mutate.h"
#include "profile/types.h"
#include "target/mutate.h"

spn_ctx_t spn;

UTEST_MAIN();

static sp_mem_t pkg_test_init(void) {
  if (!spn.intern) {
    spn.mem = sp_mem_os_new();
    spn.intern = sp_intern_new(spn.mem);
  }
  return spn.mem;
}

static spn_pkg_info_t make_pkg(sp_mem_t mem) {
  spn_pkg_info_t pkg = spn_pkg_new(mem, sp_str_lit("A"));
  pkg.macos.min_os = (spn_os_version_t) { .major = 12 };
  sp_da_push(pkg.macos.frameworks, sp_str_lit("A"));

  spn_target_info_t bin = {
    .name = sp_str_lit("A"),
    .kind = SPN_TARGET_EXE,
  };
  spn_target_info_init(mem, &bin);
  sp_da_push(bin.macos.frameworks, sp_str_lit("B"));
  bin.windows.subsystem = SPN_WIN_SUBSYSTEM_WINDOWS;
  sp_str_om_insert(pkg.exes, bin.name, bin);

  return pkg;
}

typedef enum {
  PKG_EDIT_NONE,
  PKG_EDIT_MACOS_MIN_OS,
  PKG_EDIT_TARGET_SUBSYSTEM,
  PKG_EDIT_PROFILE_SYSROOT,
} pkg_edit_t;

typedef struct {
  spn_profile_info_t profile;
  pkg_edit_t edit;
  struct {
    bool hashed;
    bool changed;
  } expect;
} hash_platform_test_t;

static void run_hash_platform_test(s32* utest_result, hash_platform_test_t t) {
  sp_mem_t mem = pkg_test_init();
  spn_pkg_info_t pkg = make_pkg(mem);

  sp_hash_t before = spn_pkg_hash_platform(&pkg, &t.profile);
  EXPECT_EQ(before != 0, t.expect.hashed);

  switch (t.edit) {
    case PKG_EDIT_NONE: {
      return;
    }
    case PKG_EDIT_MACOS_MIN_OS: {
      pkg.macos.min_os = (spn_os_version_t) { .major = 13 };
      break;
    }
    case PKG_EDIT_TARGET_SUBSYSTEM: {
      spn_pkg_get_target(&pkg, "A")->windows.subsystem = SPN_WIN_SUBSYSTEM_CONSOLE;
      break;
    }
    case PKG_EDIT_PROFILE_SYSROOT: {
      t.profile.sysroot = sp_str_lit("/A");
      break;
    }
  }

  EXPECT_EQ(spn_pkg_hash_platform(&pkg, &t.profile) != before, t.expect.changed);
}

UTEST(pkg, hash_platform) {
  hash_platform_test_t tests [] = {
    { .profile = { .os = SPN_OS_LINUX } },
    { .profile = { .os = SPN_OS_MACOS },   .expect = { .hashed = true } },
    { .profile = { .os = SPN_OS_WINDOWS }, .expect = { .hashed = true } },
    { .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_MACOS_MIN_OS,     .expect = { .hashed = true, .changed = true } },
    { .profile = { .os = SPN_OS_WINDOWS }, .edit = PKG_EDIT_MACOS_MIN_OS,     .expect = { .hashed = true } },
    { .profile = { .os = SPN_OS_WINDOWS }, .edit = PKG_EDIT_TARGET_SUBSYSTEM, .expect = { .hashed = true, .changed = true } },
    { .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_TARGET_SUBSYSTEM, .expect = { .hashed = true } },
    { .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_PROFILE_SYSROOT,  .expect = { .hashed = true, .changed = true } },
  };

  sp_carr_for(tests, it) {
    run_hash_platform_test(utest_result, tests[it]);
  }
}
