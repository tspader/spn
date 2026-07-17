#include "common.h"

SPN_TEST_SUITE(profile)

// The planted heap overflow in the fixture is benign in a plain build;
// [profile.asan] must compile and link with the address sanitizer, which
// reports the overflow and fails the binary at runtime
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
// must be able to drop the sanitizers it inherits from default
UTEST_F(profile, sanitize_clear) {
  tmpfs_init_named(&uf->fixture.fs, "profile_sanitize_clear");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/profile/clear",
    .when.sanitize = SPN_SANITIZER_ADDRESS,
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 1, .contains = { "AddressSanitizer" } } } },
      { .profile = "clean", .expect = { .bin = { .name = "main" } } },
    },
  });
}

// opt and sanitizers are build facts: each variant must get its own
// dependency fingerprint and store path, observed via a build script that
// generates a header from the profile
UTEST_F(profile, identity) {
  tmpfs_init_named(&uf->fixture.fs, "profile_identity");

  run_opt_test(utest_result, &uf->fixture, (opt_test_t) {
    .project = "test/integration/fixtures/profile/identity",
    .builds = {
      { .expect = { .bin = { .name = "main", .rc = 1 } } },
      { .profile = "fast", .expect = { .bin = { .name = "main", .rc = 2 } } },
      { .profile = "asan", .when.sanitize = SPN_SANITIZER_ADDRESS, .expect = { .bin = { .name = "main", .rc = 3 } } },
    },
  });
}

// A CLI override changes the resolved profile without changing its output
// directory; the profile stamp must dirty the objects anyway, both when the
// override appears and when it goes away
UTEST_F(profile, override_rebuild) {
  tmpfs_init_named(&uf->fixture.fs, "profile_override_rebuild");

  run_rebuild_test(utest_result, &uf->fixture, (rebuild_test_t) {
    .project = "test/integration/fixtures/profile/override",
    .first = {
      .args = { "build" },
      .expect.bin = { .name = "main", .rc = 1 },
    },
    .rebuilds = {
      {
        .command = {
          .args = { "build", "--opt", "3" },
          .expect.bin = { .name = "main", .rc = 2 },
        },
      },
      {
        .command = {
          .args = { "build" },
          .expect.bin = { .name = "main", .rc = 1 },
        },
      },
    },
  });
}

// The resolved profile's opt level lands on the compiler command line for
// both defaulted and explicit levels
UTEST_F(profile, flags) {
  tmpfs_init_named(&uf->fixture.fs, "profile_flags");

  run_test(utest_result, &uf->fixture, (test_t) {
    .project = "test/integration/fixtures/profile/sanitize",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("compile_commands.json"), .needle = sp_str_lit("-O0") } },
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "-m", "release" } } },
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("compile_commands.json"), .needle = sp_str_lit("-O2") } },
      { .kind = ACTION_VERIFY_FILE_NOT_CONTAINS, .verify_file_not_contains = { .file = sp_str_lit("compile_commands.json"), .needle = sp_str_lit("-fsanitize") } },
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
      { .kind = ACTION_VERIFY_FILE_CONTAINS, .verify_file_contains = { .file = sp_str_lit("compile_commands.json"), .needle = sp_str_lit("-fsanitize=address") } },
    },
  });
}
