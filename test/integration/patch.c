#include "common.h"

SPN_TEST_SUITE(patch)

UTEST_F(patch, applies_to_dep_source) {
  tmpfs_init_named(&uf->fixture.fs, "patch_applies_to_dep_source");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/patch/basic",
    .copy = { "vendor/spum/spn.toml", "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_SYNC_PATCH } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

UTEST_F(patch, edit_rebuilds_with_new_content) {
  tmpfs_init_named(&uf->fixture.fs, "patch_edit_rebuilds");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/patch/edit",
    .copy = { "vendor/spum/spn.toml", "patches/*" },
    .first = {
      .args = { "build" },
      .expect.bin = { .name = "main", .rc = 2 },
    },
    .rebuilds = {
      {
        .change.moves = {
          { .from = sp_str_lit("patches/spum.edit.patch"), .to = sp_str_lit("patches/spum.patch") },
        },
        .command = {
          .args = { "build" },
          .expect.bin = { .name = "main", .rc = 3 },
        },
      },
    },
  });
}

UTEST_F(patch, unused_entry_fails) {
  tmpfs_init_named(&uf->fixture.fs, "patch_unused_entry_fails");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/patch/unused",
    .copy = { "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR_PATCH } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = exe("main") },
    },
  });
}
