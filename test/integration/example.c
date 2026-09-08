#include "harness.h"

sp_test(example, kernel) {
  return run_test(t, (test_t) {
    .project = "example/kernel",
    .copy = { "start", "kernel.ld" },
    .when.target = SPN_TEST_ARCH "-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
      { .kind = ACTION_VERIFY_ELF_NO_SYMBOL, .verify_elf_no_symbol = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), "__ubsan_" } },
    },
  });
}

sp_test(example, kernel_aarch64) {
  return run_test(t, (test_t) {
    .project = "example/kernel",
    .copy = { "start", "kernel.ld" },
    .when.target = "aarch64-freestanding-none",
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "aarch64-freestanding-none" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", "aarch64-freestanding-none"), 0x400000 } },
      { .kind = ACTION_VERIFY_ELF_NO_SYMBOL, .verify_elf_no_symbol = { target_exe("main", "aarch64-freestanding-none"), "__ubsan_" } },
    },
  });
}

sp_test(example, nolibc_runs) {
  return run_command_test(t, (command_test_t) {
    .project = "example/nolibc",
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .args = { "build" },
    .expect = {
      .bin = { .path = target_exe("main", SPN_TEST_ARCH "-linux-none"), .contains = { "hello from nolibc" } },
    },
  });
}

sp_test(example, nolibc_exe_has_no_interp) {
  return run_test(t, (test_t) {
    .project = "example/nolibc",
    .when = { .host = SPN_OS_LINUX, .target = SPN_TEST_ARCH "-linux-none" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_NO_INTERP, .verify_no_interp = target_exe("main", SPN_TEST_ARCH "-linux-none") },
    },
  });
}
