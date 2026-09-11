#include "harness.h"

sp_test(linker, script_sets_entry) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/script",
    .copy = { "main.ld" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding-none" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(linker, script_unsupported_by_lld_on_windows_gnu) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/unsupported",
    .copy = { "main.ld" },
    .when = { .target = "x86_64-windows-gnu", .linker = SPN_LD_FAMILY_LLD },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(linker, toolchain_link_args_reach_the_driver) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/fuse",
    .copy = { "bin" },
    .toolchain = "G",
    .when = { .os = SPN_OS_LINUX, .programs = { "gcc" }, .shell = true },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .path = "bin", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("witness") },
    },
  });
}

sp_test(linker, link_flags_gated_on_linker_apply) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/gated",
    .when.linker = SPN_LD_FAMILY_LLD,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
    },
  });
}

sp_test(linker, link_flags_gated_on_linker_skip) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/gated",
    .when.linker = SPN_LD_FAMILY_GNU,
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = exe("main") },
    },
  });
}

sp_test(linker, cross_script_sets_entry) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/cross",
    .copy = { "main.ld" },
    .when = { .lanes = { "aarch64-gnu" }, .target = "aarch64-freestanding-none" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "aarch64-freestanding-none" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", "aarch64-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(linker, mingw_gcc_honors_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/mingw",
    .copy = { "main.ld" },
    .when = { .lanes = { "mingw-gnu", "clang-mingw" }, .target = "x86_64-windows-gnu" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = target_exe("main", "x86_64-windows-gnu") },
    },
  });
}
