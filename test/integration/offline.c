#include "common.h"

static u32 count_store_dirs(fixture_t* fixture, const c8* dir) {
  sp_str_t path = tmpfs_get(&fixture->fs, sp_str_view(dir));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(fixture->fs.mem, path);
  u32 dirs = 0;
  sp_da_for(entries, it) {
    if (entries[it].kind == SP_FS_KIND_DIR) {
      dirs++;
    }
  }
  return dirs;
}

SPN_TEST_SUITE(offline)

UTEST_F(offline, store_only) {
  tmpfs_init_named(&uf->fixture.fs, "offline_store_only");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_NO_EVENT, .verify_event = { .event = SPN_EVENT_SYNC_FAILED } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(offline, store_only_unlocked) {
  tmpfs_init_named(&uf->fixture.fs, "offline_store_only_unlocked");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .first = {
      .args = { "build" },
      .expect.exists = { store_file("bin/main") },
    },
    .rebuilds = {
      {
        .change = {
          .remove_files = { sp_str_lit("spn.lock") },
          .remove_dirs = { sp_str_lit("remote/spum"), sp_str_lit("build") },
        },
        .command = {
          .args = { "build" },
          .expect = {
            .events = { { .event = SPN_EVENT_SYNC_FAILED, .absent = true } },
            .exists = { store_file("bin/main") },
            .packages = { "core/spum" },
          },
        },
      },
    },
  });
}

UTEST_F(offline, no_source_cache) {
  UTEST_SKIP("a store hit still syncs sources when the checkout is gone; passes once store-only consumption covers resolve");
  tmpfs_init_named(&uf->fixture.fs, "offline_no_source_cache");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/deps/index/binary_static",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = ".home/storage/cache/source" } },
      { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "build" } },
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    },
  });
}

UTEST_F(offline, shared_store) {
  tmpfs_init_named(&uf->fixture.fs, "offline_shared_store");
  fixture_t* fixture = &uf->fixture;
  sp_mem_t mem = fixture->fs.mem;

  prepare_test(utest_result, fixture, "test/integration/fixtures/offline/shared_store", (const c8*[]) {
    "second/*",
    SP_NULLPTR,
  });

  run_actions(utest_result, fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
    { .kind = ACTION_NONE },
  });

  u32 entries = count_store_dirs(fixture, ".home/storage/cache/store/core/spum");

  sp_ps_config_t config = {
    .command = fixture->paths.spn,
    .cwd = tmpfs_get(&fixture->fs, sp_str_lit("second")),
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
    .env = {
      .extra = {
        { sp_str_lit("SPN_STORAGE_DIR"), fixture->paths.storage },
        { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture->paths.toolchain },
        { sp_str_lit("SPN_CONFIG_DIR"), fixture->paths.config },
        { sp_str_lit("SPN_PATCH_DIR"), fixture->paths.patches },
      },
    },
  };
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--ci"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("build"));

  sp_ps_output_t output = sp_ps_run(mem, config);
  utest_kv("command", config.command);
  utest_kv("cwd", config.cwd);
  utest_kv("output", output.out);
  EXPECT_EQ(0, output.status.exit_code);

  sp_str_t second_bin = tmpfs_get(&fixture->fs, sp_fs_join_path(mem, sp_str_lit("second"), store_file("bin/main")));
  SP_EXPECT_EXISTS_TMPFS(&fixture->fs, second_bin);

  EXPECT_EQ(entries, count_store_dirs(fixture, ".home/storage/cache/store/core/spum"));
}
