sp_test(harness, dir) {
  sp_str_t dir = sp_test_dir(t);
  sp_must(t, sp_fs_exists(dir));
  sp_must(t, sp_str_contains(dir, sp_test_get_name(t)));

  fixture_t fixture = fixture_new(t);
  fixture_create(&fixture, sp_str_lit("foo/bar.txt"), sp_str_lit("A"));

  sp_str_t path = fixture_path(&fixture, sp_str_lit("foo/bar.txt"));
  sp_must(t, sp_fs_exists(path));
  sp_must(t, sp_str_starts_with(path, fixture.root));
  sp_expect_str_eq_c(t, test_read_file(fixture.mem, path), "A");
  return SP_OK;
}

sp_test(harness, dir_distinct) {
  sp_str_t dir = sp_test_dir(t);
  sp_must(t, sp_fs_exists(dir));
  sp_must(t, sp_str_contains(dir, sp_test_get_name(t)));

  fixture_t fixture = fixture_new(t);
  fixture_create(&fixture, sp_str_lit("marker.txt"), sp_str_lit("B"));
  sp_must(t, sp_fs_exists(fixture_path(&fixture, sp_str_lit("marker.txt"))));
  return SP_OK;
}

sp_test(harness, paths) {
  fixture_t fixture = fixture_new(t);
  sp_must(t, sp_fs_exists(fixture.paths.root));
  sp_must(t, sp_fs_exists(fixture.paths.spn));
  return SP_OK;
}

sp_test(harness, init) {
  return run_test(t, (test_t) {
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "init" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("spn.toml") },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_RESULT, .key = "err", .value = "ok" } },
      { .kind = ACTION_VERIFY_RESULT },
    },
  });
}

sp_test(harness, init_command) {
  return run_command_test(t, (command_test_t) {
    .args = { "init" },
    .expect = {
      .events = {
        { .event = SPN_EVENT_RESULT, .key = "err", .value = "ok" },
      },
      .exists = { sp_str_lit("spn.toml") },
    },
  });
}
