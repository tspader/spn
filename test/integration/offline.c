static u32 count_store_dirs(fixture_t* fixture, const c8* dir) {
  sp_str_t path = fixture_path(fixture, sp_cstr_as_str(dir));
  sp_da(sp_fs_entry_t) entries = sp_fs_collect(fixture->mem, path);
  u32 dirs = 0;
  sp_da_for(entries, it) {
    if (entries[it].kind == SP_FS_KIND_DIR) {
      dirs++;
    }
  }
  return dirs;
}

sp_test(offline, store_only) {
  return run_test(t, (test_t) {
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

sp_test(offline, store_only_unlocked) {
  return run_rebuild_test(t, (rebuild_test_t) {
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

sp_test(offline, no_source_cache) {
  return sp_test_skip(t, "a store hit still syncs sources when the checkout is gone; passes once store-only consumption covers resolve");

  return run_test(t, (test_t) {
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

sp_test(offline, shared_store) {
  fixture_t fixture = sp_zero;
  sp_try(fixture_init(t, &fixture));
  sp_mem_t mem = fixture.mem;

  sp_try(prepare_test(t, &fixture, "test/integration/fixtures/offline/shared_store", (const c8*[]) {
    "second/*",
    SP_NULLPTR,
  }));

  sp_try(run_actions(t, &fixture, (action_t[]) {
    { .kind = ACTION_RUN_CLI, .cli = { "build" } },
    { .kind = ACTION_VERIFY_EXISTS, .exists = store_file("bin/main") },
    { .kind = ACTION_REMOVE_DIR, .rm = { .dir = "remote/spum" } },
    { .kind = ACTION_NONE },
  }));

  u32 entries = count_store_dirs(&fixture, ".home/storage/cache/store/core/spum");

  sp_ps_config_t config = {
    .command = fixture.paths.spn,
    .cwd = fixture_path(&fixture, sp_str_lit("second")),
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
    .env = {
      .extra = {
        { sp_str_lit("SPN_STORAGE_DIR"), fixture.paths.storage },
        { sp_str_lit("SPN_TOOLCHAIN_DIR"), fixture.paths.toolchain },
        { sp_str_lit("SPN_CONFIG_DIR"), fixture.paths.config },
        { sp_str_lit("SPN_PATCH_DIR"), fixture.paths.patches },
      },
    },
  };
  sp_ps_config_add_arg(mem, &config, sp_str_lit("--ci"));
  sp_ps_config_add_arg(mem, &config, sp_str_lit("build"));

  sp_ps_output_t output = sp_ps_run(mem, config);
  sp_test_kv(t, "command", config.command);
  sp_test_kv(t, "cwd", config.cwd);
  sp_test_kv(t, "output", output.out);
  sp_expect_eq(t, 0, output.status.exit_code);

  sp_str_t second_bin = fixture_path(&fixture, sp_fs_join_path(mem, sp_str_lit("second"), store_file("bin/main")));
  expect_exists(t, &fixture, second_bin, true, __FILE__, __LINE__);

  sp_expect_eq(t, entries, count_store_dirs(&fixture, ".home/storage/cache/store/core/spum"));
  return SP_OK;
}
