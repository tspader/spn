#include "common.h"

SPN_TEST_SUITE(user_log)

UTEST_F(user_log, hidden_normally) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_hidden");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log",
    .args = { "build" },
    .expect.excludes = { "spn-script-probe-log" },
  });
}

UTEST_F(user_log, shown_on_failure) {
  tmpfs_init_named(&uf->fixture.fs, "log_script_log_failure");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/log/script_log_fail",
    .args = { "build" },
    .expect = {
      .rc = 1,
      .contains = { "spn-script-probe-fail" },
    },
  });
}

