#include "harness.h"

sp_test(elf, gnu_script) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_gnu_script",
    .copy = { "main.ld" },
    .when = { .toolchain = "G", .host = SPN_OS_LINUX, .programs = { "gcc", "ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(elf, lld_script) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_lld_script",
    .copy = { "main.ld" },
    .when = { .toolchain = "GL", .host = SPN_OS_LINUX, .programs = { "gcc", "ar", "ld.lld" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(elf, clang_gnu) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_clang_gnu",
    .when = { .toolchain = "C", .host = SPN_OS_LINUX, .programs = { "clang", "ld", "ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(elf, clang_lld) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_clang_lld",
    .when = { .toolchain = "LV", .host = SPN_OS_LINUX, .programs = { "clang", "ld.lld", "llvm-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(elf, gcc_lld_hosted) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_gcc_lld_hosted",
    .when = { .toolchain = "GLH", .host = SPN_OS_LINUX, .programs = { "gcc", "ar", "ld.lld" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build" } },
      { .kind = ACTION_VERIFY_EVENT, .verify_event = { .event = SPN_EVENT_LINK_PASSED } },
    },
  });
}

sp_test(elf, zig_script) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_zig_script",
    .copy = { "main.ld" },
    .when = { .driver = SPN_CC_DRIVER_ZIG, .target = SPN_TEST_ARCH "-freestanding" },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", SPN_TEST_ARCH "-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", SPN_TEST_ARCH "-freestanding-none"), 0x400000 } },
    },
  });
}

sp_test(elf, cross_gnu_script) {
  return run_test(t, (test_t) {
    .project = "test/smoke/fixtures/elf_cross_gnu_script",
    .copy = { "main.ld" },
    .when = { .toolchain = "X", .programs = { "aarch64-linux-gnu-gcc", "aarch64-linux-gnu-ar" } },
    .actions = {
      { .kind = ACTION_RUN_CLI, .cli = { .cmd = "build", .args = { "--target", "aarch64-freestanding" } } },
      { .kind = ACTION_VERIFY_ELF_ENTRY, .verify_elf_entry = { target_exe("main", "aarch64-freestanding-none"), 0x400000 } },
    },
  });
}
