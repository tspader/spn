#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"
#include "ctx/types.h"
#include "intern/intern.h"
#include "profile/profile.h"
#include "pkg/types.h"

spn_ctx_t spn;

UTEST_MAIN();

#define PROFILE_TEST_MAX_PROFILES 2

static const spn_triple_t PROFILE_HOST_LINUX_GNU  = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU };
static const spn_triple_t PROFILE_HOST_WIN_MSVC   = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC };
static const spn_triple_t PROFILE_HOST_ARM_MACOS  = { SPN_ARCH_ARM64, SPN_OS_MACOS, SPN_ABI_NONE };

typedef struct {
  spn_err_t err;
  spn_triple_t target;
  const c8* toolchain;
  bool targeted;
} profile_expect_t;

typedef struct {
  spn_profile_info_t profiles [PROFILE_TEST_MAX_PROFILES];
  spn_profile_info_t overrides;
  spn_triple_t host;
  profile_expect_t expect;
} profile_test_t;

static sp_mem_t profile_test_init(void) {
  if (!spn.intern) {
    spn.mem = sp_mem_os_new();
    spn.intern = sp_intern_new(spn.mem);
  }
  return spn.mem;
}

static void run_profile_test(s32* utest_result, profile_test_t t) {
  sp_mem_t mem = profile_test_init();

  spn_pkg_info_t pkg = sp_zero;
  sp_carr_for(t.profiles, it) {
    if (sp_str_empty(t.profiles[it].name)) {
      break;
    }
    sp_str_om_insert(pkg.profiles, t.profiles[it].name, t.profiles[it]);
  }

  spn_profile_table_t table = SP_NULLPTR;
  sp_str_ht_init(mem, table);
  spn_profile_populate(&table, &pkg);

  spn_profile_info_t result = sp_zero;
  spn_err_union_t err = spn_profile_resolve(table, &t.overrides, t.host, &result);
  ASSERT_EQ((u32)t.expect.err, (u32)err.kind);
  if (t.expect.err) {
    return;
  }

  EXPECT_EQ(t.expect.target.arch, result.arch);
  EXPECT_EQ(t.expect.target.os, result.os);
  EXPECT_EQ(t.expect.target.abi, result.abi);
  EXPECT_EQ(t.expect.targeted, result.targeted);
  if (t.expect.toolchain) {
    EXPECT_TRUE(sp_str_equal_cstr(result.toolchain, t.expect.toolchain));
  }
}

UTEST(profile, default_target_is_host) {
  run_profile_test(utest_result, (profile_test_t) {
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .toolchain = "auto",
    },
  });
}

UTEST(profile, windows_host_defaults_to_gnu) {
  run_profile_test(utest_result, (profile_test_t) {
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
    },
  });
}

UTEST(profile, macos_target_keeps_abi_empty) {
  run_profile_test(utest_result, (profile_test_t) {
    .overrides = { .arch = SPN_ARCH_ARM64, .os = SPN_OS_MACOS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_MACOS },
      .targeted = true,
    },
  });
}

UTEST(profile, cross_windows_defaults_to_gnu) {
  run_profile_test(utest_result, (profile_test_t) {
    .overrides = { .os = SPN_OS_WINDOWS },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_GNU },
      .targeted = true,
    },
  });
}

UTEST(profile, cross_linux_defaults_to_gnu) {
  run_profile_test(utest_result, (profile_test_t) {
    .overrides = { .os = SPN_OS_LINUX },
    .host = PROFILE_HOST_ARM_MACOS,
    .expect = {
      .target = { SPN_ARCH_ARM64, SPN_OS_LINUX, SPN_ABI_GNU },
      .targeted = true,
    },
  });
}

UTEST(profile, explicit_abi_wins) {
  run_profile_test(utest_result, (profile_test_t) {
    .overrides = { .os = SPN_OS_WINDOWS, .abi = SPN_ABI_MSVC },
    .host = PROFILE_HOST_WIN_MSVC,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_WINDOWS, SPN_ABI_MSVC },
      .targeted = true,
    },
  });
}

UTEST(profile, profile_toolchain_overrides_auto) {
  run_profile_test(utest_result, (profile_test_t) {
    .profiles = {
      { .name = sp_str_lit("default"), .toolchain = sp_str_lit("system") },
    },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .toolchain = "system",
    },
  });
}

UTEST(profile, override_toolchain_wins) {
  run_profile_test(utest_result, (profile_test_t) {
    .profiles = {
      { .name = sp_str_lit("default"), .toolchain = sp_str_lit("system") },
    },
    .overrides = { .toolchain = sp_str_lit("msvc") },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = {
      .target = { SPN_ARCH_X64, SPN_OS_LINUX, SPN_ABI_GNU },
      .toolchain = "msvc",
    },
  });
}

UTEST(profile, undefined_profile) {
  run_profile_test(utest_result, (profile_test_t) {
    .overrides = { .name = sp_str_lit("missing") },
    .host = PROFILE_HOST_LINUX_GNU,
    .expect = { .err = SPN_ERR_PROFILE_UNDEFINED },
  });
}
