#include "harness.h"

sp_test(config, toolchain_is_selectable) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/config/toolchain",
    .config = "config.toml",
    .toolchain = "T",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_TOOLCHAIN_MISSING },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "program", .value = "/Q" } },
    },
  });
}

sp_test(config, toolchain_overrides_builtin) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/config/builtin",
    .config = "config.toml",
    .toolchain = "gcc",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_TOOLCHAIN_MISSING },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "program", .value = "/Q" } },
    },
  });
}

sp_test(config, project_toolchain_wins) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/config/override",
    .config = "config.toml",
    .toolchain = "T",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_TOOLCHAIN_MISSING },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_ERR, .key = "program", .value = "/R" } },
    },
  });
}
