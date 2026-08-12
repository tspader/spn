#include "harness.h"

sp_test(patch, applies_to_dep_source) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/patch/basic",
    .copy = { "vendor/spum/spn.toml", "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_SYNC_PATCH } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(patch, edit_rebuilds_with_new_content) {
  return run_rebuild_test(t, (rebuild_test_t) {
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

sp_test(patch, unused_entry_fails) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/patch/unused",
    .copy = { "patches/*" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "kind", .value = "patch_unused" } },
      { .kind = ACTION_VERIFY_NOT_EXISTS, .exists = exe("main") },
    },
  });
}
