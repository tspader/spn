#include "toolchain.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  spn_triple_t declared [FIXTURE_MAX_TARGETS];
  spn_triple_t host;
  spn_triple_t expect [FIXTURE_MAX_TARGETS];
} targets_test_t;

static const targets_test_t tests [] = {
  {
    .name = "gcc_on_linux_adds_host_arch_bare_metal",
    .driver = SPN_CC_DRIVER_GCC,
    .host = HOST_X64_LINUX,
    .expect = { HOST_X64_LINUX, { SPN_ARCH_X64, SPN_OS_FREESTANDING, SPN_ABI_BARE } },
  },
  {
    .name = "clang_on_linux_adds_host_arch_bare_metal",
    .driver = SPN_CC_DRIVER_CLANG,
    .host = HOST_ARM_LINUX,
    .expect = { HOST_ARM_LINUX, { SPN_ARCH_ARM64, SPN_OS_FREESTANDING, SPN_ABI_BARE } },
  },
  {
    .name = "clang_on_macos_is_host_only",
    .driver = SPN_CC_DRIVER_CLANG,
    .host = HOST_ARM_MACOS,
    .expect = { HOST_ARM_MACOS },
  },
  {
    .name = "msvc_is_host_only",
    .driver = SPN_CC_DRIVER_MSVC,
    .host = HOST_X64_LINUX,
    .expect = { HOST_X64_LINUX },
  },
  {
    .name = "declared_targets_are_exact",
    .driver = SPN_CC_DRIVER_GCC,
    .declared = { HOST_ARM_LINUX },
    .host = HOST_X64_LINUX,
    .expect = { HOST_ARM_LINUX },
  },
};

sp_test_each(targets, resolve, targets_test_t, tests) {
  sp_mem_t mem = sp_test_arena(t);

  spn_toolchain_info_t info = { .driver = it->driver, .targets = sp_da_new(mem, spn_triple_t) };
  u32 declared = 0;
  sp_carr_detect_len(it->declared, declared, !fixture_triple_empty(it->declared[declared]));
  sp_for(at, declared) {
    sp_da_push(info.targets, it->declared[at]);
  }

  u32 expected = 0;
  sp_carr_detect_len(it->expect, expected, !fixture_triple_empty(it->expect[expected]));

  sp_da(spn_triple_t) targets = spn_toolchain_targets(mem, &info, it->host);
  sp_must_eq(t, expected, (u32)sp_da_size(targets));
  sp_for(at, expected) {
    sp_expect(t, fixture_triple_equal(it->expect[at], targets[at]));
  }
  return SP_OK;
}
