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
        .events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
        .cc = { { .args = { "DFAST" }, .absent = true } },
      },
    },
    .rebuilds = {
      {
        .command = {
          .args = { "build", "--opt", "3" },
          .expect = {
            .bin = { .name = "main", .rc = 2 },
            .events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED } },
            .cc = { { .args = { "DFAST" } } },
          },
        },
      },
      {
        .command = {
          .args = { "build" },
          .expect = {
            .bin = { .name = "main", .rc = 1 },
            .events = { { .event = SPN_EVENT_TARGET_BUILD_PASSED, .absent = true } },
            .cc = { { .args = { "DFAST" }, .absent = true } },
          },
        },
      },
    },
  });
}

UTEST_F(profile, default_is_musl_static) {
  tmpfs_init_named(&uf->fixture.fs, "profile_default_musl_static");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/override",
    .when.os = SPN_OS_LINUX,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-musl" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = "zig" } },
    },
  });
}

UTEST_F(profile, config_shared_demand_defaults_to_gnu) {
  tmpfs_init_named(&uf->fixture.fs, "profile_config_shared_demand");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/shared",
    .copy = { "packages/*" },
    .when.os = SPN_OS_LINUX,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-gnu" } },
    },
  });
}

UTEST_F(profile, root_shared_lib_demand_defaults_to_gnu) {
  tmpfs_init_named(&uf->fixture.fs, "profile_root_shared_lib_demand");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/target/shared_lib",
    .copy = { "spum.c" },
    .when.os = SPN_OS_LINUX,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-gnu" } },
    },
  });
}

UTEST_F(profile, static_config_is_not_a_shared_demand) {
  tmpfs_init_named(&uf->fixture.fs, "profile_static_config_no_demand");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/static",
    .copy = { "packages/*" },
    .when.os = SPN_OS_LINUX,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-musl" } },
    },
  });
}

UTEST_F(profile, target_without_toolchain) {
  tmpfs_init_named(&uf->fixture.fs, "profile_target_without_toolchain");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/override",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-wasi" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_TOOLCHAIN_NONE } },
    },
  });
}

UTEST_F(profile, cross_target_macos) {
  tmpfs_init_named(&uf->fixture.fs, "profile_cross_target_macos");

  run_command_test(utest_result, &uf->fixture, (command_test_t) {
    .project = "test/integration/fixtures/profile/override",
    .args = { "build", "--target", "aarch64-macos" },
    .expect = {
      .exists = { target_exe("main", "aarch64-macos") },
      .events = {
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = "aarch64-macos" },
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = "zig" },
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
