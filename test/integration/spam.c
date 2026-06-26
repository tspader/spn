#define SP_IMPLEMENTATION
#include "sp.h"
#include "test.h"
#include "utest.h"
#include "action.h"
#include "harness.h"

UTEST_MAIN()

#define SPN_TEST_SPANDEX_URL "https://github.com/tspader/spandex.git"
#define SPN_TEST_SPANDEX_PIN "55e546c8859d56f2cae08256bed6e0cce23d1bc8"

struct e2e {
  fixture_t fixture;
};

UTEST_INITIALIZER(e2e_init_tmpfs_top_level) {
  sp_str_t tmp = sp_fs_normalize_path(spn_allocator, sp_os_env_get(sp_str_lit("SPN_TEST_TMP")));
  if (sp_str_empty(tmp)) {
    tmp = sp_str_lit(".tmp");
  }

  sp_tm_epoch_t now = sp_tm_now_epoch();
  sp_str_t iso = sp_tm_epoch_to_iso8601(spn_allocator, now);
  c8* sanitized = (c8*)sp_alloc(spn_allocator, iso.len);
  sp_for(it, iso.len) {
    sanitized[it] = iso.data[it] == ':' ? '-' : iso.data[it];
  }

  tmpfs_set_top_level(sp_fs_join_path(spn_allocator, tmp, sp_str(sanitized, iso.len)));
}

UTEST_F_SETUP(e2e) {
#if defined(SPN_TEST_ROOT) && defined(SPN_TEST_BIN)
  uf->fixture.paths.root = sp_str_lit(SPN_TEST_ROOT);
  uf->fixture.paths.spn = sp_str_lit(SPN_TEST_BIN);
#else
  uf->fixture.paths.root = sp_fs_get_cwd(spn_allocator);
  uf->fixture.paths.spn = sp_fs_join_path(spn_allocator, uf->fixture.paths.root, sp_str_lit("build/debug/store/bin/spn"));
#endif
  ASSERT_TRUE(sp_fs_exists(uf->fixture.paths.spn));
}

UTEST_F_TEARDOWN(e2e) {
}

static bool e2e_enabled(void) {
  sp_str_t flag = sp_os_env_get(sp_str_lit("SPN_TEST_LIVE"));
  return sp_str_equal(flag, sp_str_lit("1"));
}

static void e2e_prepare(s32* utest_result, fixture_t* fixture, const c8* project) {
  fixture->paths.config = tmpfs_get(&fixture->fs, sp_str_lit(".home/config"));
  fixture->paths.storage = sp_fs_join_path(spn_allocator, fixture->paths.root, sp_str_lit(".cache/e2e/storage"));
  fixture->paths.include = sp_fs_join_path(spn_allocator, fixture->paths.storage, sp_str_lit("spn/include"));
  sp_fs_create_dir(fixture->paths.config);
  sp_fs_create_dir(fixture->paths.storage);
  sp_fs_create_dir(fixture->paths.include);

  setup_e2e_config(
    &fixture->fs,
    fixture->paths.config,
    fixture->paths.root,
    sp_str_lit(SPN_TEST_SPANDEX_URL),
    sp_str_lit(SPN_TEST_SPANDEX_PIN)
  );

  sp_fs_copy(sp_fs_join_path(spn_allocator, fixture->paths.root, sp_str_lit("include/spn.h")), fixture->paths.include);

  sp_str_t path = sp_fs_join_path(spn_allocator, fixture->paths.root, sp_str_view(project));
  fixture_copy_project(utest_result, fixture, path, SP_NULLPTR);
}

UTEST_F(e2e, sp) {
  if (!e2e_enabled()) {
    sp_log("{.fg brightyellow} (set SPN_TEST_LIVE=1 to run)", SP_FMT_CSTR("skipped e2e.sp"));
    return;
  }

  tmpfs_init_named(&uf->fixture.fs, "e2e_sp");
  e2e_prepare(utest_result, &uf->fixture, "test/fixtures/e2e/sp");

  run_actions(utest_result, &uf->fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_VERIFY_PKG_LOCKED, .verify_locked = { .name = "spader/sp" } },
    { .kind = ACTION_RUN_BIN, .bin = { .name = "main", .rc = 0 } },
  });
}
