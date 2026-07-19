#include "common.h"

SPN_TEST_SUITE(profile)

UTEST_F(profile, sanitize_trigger) {
  tmpfs_init_named(&uf->fixture.fs, "profile_sanitize_trigger");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
      { .profile = "asan", .expect = { .bin = { .name = "main", .rc = 1, .contains = { "AddressSanitizer" } } } },
    },
  });
}

UTEST_F(profile, sanitize_clear) {
  tmpfs_init_named(&uf->fixture.fs, "profile_sanitize_clear");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/clear",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-fsanitize=address", "/fsanitize=address" } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-p", "clean" } } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-fsanitize=address", "/fsanitize=address" } },
    },
  });
}

UTEST_F(profile, identity) {
  tmpfs_init_named(&uf->fixture.fs, "profile_identity");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/profile/identity",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
      { .profile = "fast", .expect = { .bin = { .name = "main" } } },
      { .profile = "asan", .when.sanitize = SPN_SANITIZER_ADDRESS, .expect = { .bin = { .name = "main" } } },
    },
  });
}

UTEST_F(profile, override_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "profile_override_rebuild");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/profile/override",
    .first = {
      .args = { "build" },
      .expect = {
        .bin = { .name = "main", .rc = 1 },
        .events = { { .event = "target_build_passed" } },
        .files = { { .file = sp_str_lit("compile_commands.json"), .excludes = { "DFAST" } } },
      },
    },
    .rebuilds = {
      {
        .command = {
          .args = { "build", "--opt", "3" },
          .expect = {
            .bin = { .name = "main", .rc = 2 },
            .events = { { .event = "target_build_passed" } },
            .files = { { .file = sp_str_lit("compile_commands.json"), .contains = { "DFAST" } } },
          },
        },
      },
      {
        .command = {
          .args = { "build" },
          .expect = {
            .bin = { .name = "main", .rc = 1 },
            .events = { { .event = "target_build_passed", .absent = true } },
            .files = { { .file = sp_str_lit("compile_commands.json"), .excludes = { "DFAST" } } },
          },
        },
      },
    },
  });
}

UTEST_F(profile, flags) {
  tmpfs_init_named(&uf->fixture.fs, "profile_flags");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-O0", "/Od" } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-m", "release" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-O2", "/O2" } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-fsanitize=address", "/fsanitize=address" } },
    },
  });
}

UTEST_F(profile, flags_sanitize) {
  tmpfs_init_named(&uf->fixture.fs, "profile_flags_sanitize");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-p", "asan" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-fsanitize=address", "/fsanitize=address" } },
    },
  });
}
