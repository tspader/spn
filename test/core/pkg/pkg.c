#include "spn_test.h"

#include "ctx/types.h"
#include "intern/intern.h"
#include "pkg/pkg.h"
#include "pkg/mutate.h"
#include "pkg/patch.h"
#include "profile/types.h"
#include "target/mutate.h"

sp_test_suite(pkg, .serial = true);

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
  bool hashed;
  bool changed;
} hash_expect_t;

typedef struct {
  const c8* name;
  spn_profile_info_t profile;
  pkg_edit_t edit;
  hash_expect_t expect;
} hash_platform_test_t;

static const hash_platform_test_t hash_platform_tests [] = {
  { .name = "skip_linux",                     .profile = { .os = SPN_OS_LINUX } },
  { .name = "hash_macos",                     .profile = { .os = SPN_OS_MACOS },   .expect = { .hashed = true } },
  { .name = "hash_windows",                   .profile = { .os = SPN_OS_WINDOWS }, .expect = { .hashed = true } },
  { .name = "track_macos_min_os",             .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_MACOS_MIN_OS,     .expect = { .hashed = true, .changed = true } },
  { .name = "ignore_macos_edit_on_windows",   .profile = { .os = SPN_OS_WINDOWS }, .edit = PKG_EDIT_MACOS_MIN_OS,     .expect = { .hashed = true } },
  { .name = "track_windows_subsystem",        .profile = { .os = SPN_OS_WINDOWS }, .edit = PKG_EDIT_TARGET_SUBSYSTEM, .expect = { .hashed = true, .changed = true } },
  { .name = "ignore_subsystem_edit_on_macos", .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_TARGET_SUBSYSTEM, .expect = { .hashed = true } },
  { .name = "track_profile_sysroot",          .profile = { .os = SPN_OS_MACOS },   .edit = PKG_EDIT_PROFILE_SYSROOT,  .expect = { .hashed = true, .changed = true } },
};

sp_test_each(pkg, hash_platform, hash_platform_test_t, hash_platform_tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = spn.mem;
  spn_pkg_info_t pkg = make_pkg(mem);

  sp_hash_t before = spn_pkg_hash_platform(&pkg, &it->profile);
  sp_expect_eq(t, before != 0, it->expect.hashed);

  switch (it->edit) {
    case PKG_EDIT_NONE: {
      return SP_OK;
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
      it->profile.sysroot = sp_str_lit("/A");
      break;
    }
  }

  sp_expect_eq(t, spn_pkg_hash_platform(&pkg, &it->profile) != before, it->expect.changed);
  return SP_OK;
}

typedef struct {
  spn_pkg_patch_stamp_result_t result;
  sp_hash_t hash;
} stamp_expect_t;

typedef struct {
  const c8* name;
  const c8* patches [2];
  const c8* qualified;
  spn_pkg_tree_kind_t tree;
  stamp_expect_t expect;
} stamp_test_t;

static const stamp_test_t stamp_tests [] = {
  { .name = "stamp_matching_patch",  .patches = { "core/a" }, .qualified = "core/a", .tree = SPN_PKG_TREE_GIT,   .expect = { SPN_PKG_PATCH_STAMP_APPLIED, 1 } },
  { .name = "skip_unmatched_patch",  .patches = { "core/a" }, .qualified = "core/b", .tree = SPN_PKG_TREE_GIT,   .expect = { SPN_PKG_PATCH_STAMP_NONE } },
  { .name = "stamp_second_patch",    .patches = { "core/a", "core/b" }, .qualified = "core/b", .tree = SPN_PKG_TREE_GIT, .expect = { SPN_PKG_PATCH_STAMP_APPLIED, 2 } },
  { .name = "reject_tree_none",      .patches = { "core/a" }, .qualified = "core/a", .tree = SPN_PKG_TREE_NONE,  .expect = { SPN_PKG_PATCH_STAMP_NOT_GIT } },
  { .name = "reject_tree_local",     .patches = { "core/a" }, .qualified = "core/a", .tree = SPN_PKG_TREE_LOCAL, .expect = { SPN_PKG_PATCH_STAMP_NOT_GIT } },
  { .name = "skip_empty_patches",    .qualified = "core/a",   .tree = SPN_PKG_TREE_NONE, .expect = { SPN_PKG_PATCH_STAMP_NONE } },
};

sp_test_each(pkg, patch_stamp, stamp_test_t, stamp_tests) {
  sp_mem_t mem = sp_test_arena(t);

  sp_da(spn_pkg_patch_t) patches = sp_da_new(mem, spn_pkg_patch_t);
  sp_carr_for(it->patches, p) {
    if (!it->patches[p]) break;
    sp_da_push(patches, ((spn_pkg_patch_t) {
      .qualified = sp_str_view(it->patches[p]),
      .set.hash = (sp_hash_t)(p + 1),
    }));
  }

  spn_pkg_tree_t source = { .kind = it->tree };
  spn_pkg_patch_stamp_result_t result = spn_pkg_patch_stamp(patches, sp_str_view(it->qualified), &source);

  sp_expect_eq(t, (u32)it->expect.result, (u32)result);
  if (it->tree == SPN_PKG_TREE_GIT) {
    sp_expect_eq(t, it->expect.hash, source.git.patches.hash);
  }

  return SP_OK;
}
