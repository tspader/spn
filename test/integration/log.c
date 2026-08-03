#include "harness.h"

sp_test(user_log, hidden_normally) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .args = { "build" },
    .expect.excludes = { "spn-script-probe-log" },
  });
}

sp_test(user_log, shown_on_failure) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log_fail",
    .args = { "build" },
    .expect = {
      .rc = 1,
      .contains = { "spn-script-probe-fail" },
    },
  });
}
