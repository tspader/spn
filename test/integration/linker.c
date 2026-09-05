#include "harness.h"

sp_test(linker, script_sets_entry) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/script",
    .copy = { "main.ld" },
    .when.target = SPN_TEST_ARCH "-freestanding",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(linker, script_unsupported_by_lld_on_windows_gnu) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/unsupported",
    .copy = { "main.ld" },
    .when = { .target = "x86_64-windows-gnu", .driver = SPN_CC_DRIVER_ZIG },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(linker, script_unsupported_on_macos) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/unsupported",
    .copy = { "main.ld" },
    .when.target = SPN_TEST_ARCH "-macos",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-macos" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(linker, script_unsupported_on_wasi) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/unsupported",
    .copy = { "main.ld" },
    .when.target = "wasm32-wasi",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "wasm32-wasi" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(linker, script_unsupported_on_msvc) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/unsupported",
    .copy = { "main.ld" },
    .when.target = SPN_TEST_ARCH "-windows-msvc",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-windows-msvc" }, .rc = 1 } },
      { .kind = ACTION_VERIFY_RESULT, .verify_result.err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  });
}

sp_test(linker, declared_program_is_invoked) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/program",
    .copy = { "ld" },
    .when = { .toolchain = "F", .programs = { "clang", "ar" }, .shell = true },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("witness") },
    },
  });
}

sp_test(linker, lld_family_is_requested_from_gcc) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/fuse",
    .copy = { "bin" },
    .when = { .toolchain = "G", .host = SPN_OS_LINUX, .programs = { "gcc", "ar" }, .shell = true },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .path = "bin", .rc = 1 } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_FAILED } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = sp_str_lit("witness") },
    },
  });
}

sp_test(linker, gcc_with_lld_honors_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/lld",
    .copy = { "main.ld" },
    .when = { .toolchain = "L", .host = SPN_OS_LINUX, .programs = { "gcc", "ar", "ld.lld" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(linker, cross_gcc_honors_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/cross",
    .copy = { "main.ld" },
    .when = { .toolchain = "X", .programs = { "aarch64-linux-gnu-gcc", "aarch64-linux-gnu-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "aarch64-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", "aarch64-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(linker, mingw_gcc_honors_script) {
  return run_test(t, (test_t) {
    .project = "test/integration/fixtures/linker/mingw",
    .copy = { "main.ld" },
    .when = { .toolchain = "M", .programs = { "x86_64-w64-mingw32-gcc", "x86_64-w64-mingw32-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "x86_64-windows-gnu" } } },
      { .kind = ACTION_VERIFY_EXISTS, .exists = target_exe("main", "x86_64-windows-gnu") },
    },
  });
}
