#include "harness.h"

sp_test(profile, sanitize_trigger) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
      { .profile = "asan", .expect = { .bin = { .name = "main", .rc = 1, .contains = { "AddressSanitizer" } } } },
    },
  });
}

sp_test(profile, sanitize_clear) {
  return run_test(t, (test_t) {
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

sp_test(profile, identity) {
  return run_opt_test(t, (opt_test_t) {
    .project = "test/integration/fixtures/profile/identity",
    .builds = {
      { .expect = { .bin = { .name = "main" } } },
      { .profile = "fast", .expect = { .bin = { .name = "main" } } },
      { .profile = "asan", .when.sanitize = SPN_SANITIZER_ADDRESS, .expect = { .bin = { .name = "main" } } },
    },
  });
}

sp_test(profile, override_rebuild) {
  return run_rebuild_test(t, (rebuild_test_t) {
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

sp_test(profile, default_is_musl_static) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/override",
    .when = { .os = SPN_OS_LINUX, .driver = SPN_CC_DRIVER_ZIG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-musl" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = "zig" } },
    },
  });
}

sp_test(profile, config_shared_demand_defaults_to_gnu) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/shared",
    .copy = { "packages/*" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-gnu" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-gnu" } },
    },
  });
}

sp_test(profile, root_shared_lib_demand_defaults_to_gnu) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/target/shared_lib",
    .copy = { "spum.c" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-gnu" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-gnu" } },
    },
  });
}

sp_test(profile, static_config_is_not_a_shared_demand) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/consume/multi_kind/static",
    .copy = { "packages/*" },
    .when = { .os = SPN_OS_LINUX, .driver = SPN_CC_DRIVER_ZIG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-musl" } },
    },
  });
}

sp_test(profile, target_with_foreign_arch) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/override",
    .when.msvc_todo = true,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-wasi" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result = { .err = SPN_ERR_PROFILE_ARCH } },
    },
  });
}

sp_test(profile, cross_target_macos) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/profile/override",
    .when.driver = SPN_CC_DRIVER_ZIG,
    .args = { "build", "--target", "aarch64-macos" },
    .expect = {
      .exists = { target_exe("main", "aarch64-macos-apple") },
      .events = {
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = "aarch64-macos-apple" },
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = "zig" },
      },
    },
  });
}

sp_test(profile, freestanding_target) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/profile/freestanding",
    .copy = { "a.c" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .args = { "build", "--target", SPN_TEST_ARCH "-freestanding-none" },
    .expect = {
      .exists = { target_exe("main", SPN_TEST_ARCH "-freestanding-none") },
      .events = {
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-freestanding-none" },
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = test_toolchain()->name },
      },
    },
  });
}

sp_test(profile, freestanding_exe_has_no_interp) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/freestanding",
    .copy = { "a.c" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding-none" } } },
      { .kind = ACTION_VERIFY_NO_INTERP, .verify_no_interp = target_exe("main", SPN_TEST_ARCH "-freestanding-none") },
    },
  });
}

sp_test(profile, freestanding_image_has_no_sanitizer_runtime) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/freestanding",
    .copy = { "a.c" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding-none" } } },
      { .kind = ACTION_VERIFY_ELF_NO_SYMBOL, .verify_elf_no_symbol = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), "__ubsan_" } },
    },
  });
}

sp_test(profile, freestanding_libs_not_pic) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/freestanding",
    .copy = { "a.c" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding-none" } } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-fPIC" } },
    },
  });
}

sp_test(profile, nolibc_target) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/profile/nolibc",
    .copy = { "a.c" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .args = { "build", "--abi", "none" },
    .expect = {
      .exists = { target_exe("main", SPN_TEST_ARCH "-linux-none") },
      .events = {
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "target", .value = SPN_TEST_ARCH "-linux-none" },
        { .event = SPN_EVENT_INIT_BUILD_GRAPH, .key = "toolchain", .value = test_toolchain()->name },
      },
    },
  });
}

sp_test(profile, nolibc_runs) {
  return run_command_test(t, (command_test_t) {
    .project = "test/integration/fixtures/profile/nolibc",
    .copy = { "a.c" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .args = { "build", "--abi", "none" },
    .expect = {
      .bin = { .path = target_exe("main", SPN_TEST_ARCH "-linux-none"), .rc = 7, .contains = { "A" } },
    },
  });
}

sp_test(profile, nolibc_exe_has_no_interp) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/nolibc",
    .copy = { "a.c" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--abi", "none" } } },
      { .kind = ACTION_VERIFY_NO_INTERP, .verify_no_interp = target_exe("main", SPN_TEST_ARCH "-linux-none") },
    },
  });
}

sp_test(profile, nolibc_libs_not_pic) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/nolibc",
    .copy = { "a.c" },
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--abi", "none" } } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-fPIC" } },
    },
  });
}

sp_test(profile, coff_libs_not_pic) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/platform/fingerprint",
    .copy = { "main.c", "packages/*" },
    .when = { .os = SPN_OS_WINDOWS, .driver = SPN_CC_DRIVER_CLANG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { "-fPIC" } },
    },
  });
}

sp_test(profile, flags) {
  return run_test(t, (test_t) {
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

sp_test(profile, flags_sanitize) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-p", "asan" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { "-fsanitize=address", "/fsanitize=address" } },
    },
  });
}
