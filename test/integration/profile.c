#include "common.h"

SPN_TEST_SUITE(profile)

// The planted heap overflow in the fixture is benign in a plain build;
// [profile.asan] must compile and link with the address sanitizer, which
// reports the overflow and fails the binary at runtime. This is the one
// deliberately-runtime test in the suite: it proves the sanitizer runtime
// actually works end to end, so it only executes when the target is the host
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

// sanitize = [] is an explicit clear, not an unset field: a derived profile
// must be able to drop the sanitizers it inherits from default, observed on
// the compiler command line rather than by running the binary
UTEST_F(profile, sanitize_clear) {
  tmpfs_init_named(&uf->fixture.fs, "profile_sanitize_clear");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/clear",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { .args = { "-fsanitize=address", "/fsanitize=address" } } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-p", "clean" } } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { .args = { "-fsanitize=address", "/fsanitize=address" } } },
    },
  });
}

// opt and sanitizers are build facts: each variant must get its own
// dependency fingerprint and store path, observed via a build script that
// generates a header from the profile; the fixture cross-checks the
// generated value against a when-gated define at compile time, so a stale
// store entry fails the build instead of returning the wrong exit code
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

// A CLI override changes the resolved profile without changing its output
// directory; the override appearing must dirty the objects and recompile
// (target_build_passed only fires for real compiles), and the override going
// away must restore the original binary from cache without compiling. Cache
// restoration is only observable by running the restored binary, so this test
// keeps its rc checks; rebuild tests always target the host
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

// The resolved profile's opt level lands on the compiler command line for
// both defaulted and explicit levels, in whichever spelling the driver uses
UTEST_F(profile, flags) {
  tmpfs_init_named(&uf->fixture.fs, "profile_flags");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { .args = { "-O0", "/Od" } } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-m", "release" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { .args = { "-O2", "/O2" } } },
      { .kind = ACTION_VERIFY_NO_CC_ARG, .verify_cc_arg = { .args = { "-fsanitize=address", "/fsanitize=address" } } },
    },
  });
}

// The resolved profile's sanitizers land on the compiler command line
UTEST_F(profile, flags_sanitize) {
  tmpfs_init_named(&uf->fixture.fs, "profile_flags_sanitize");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-p", "asan" } } },
      { .kind = ACTION_VERIFY_CC_ARG, .verify_cc_arg = { .args = { "-fsanitize=address", "/fsanitize=address" } } },
    },
  });
}
