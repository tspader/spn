#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "compiler/driver.h"
#include "test.h"

UTEST_MAIN();

#define flags_max 5

typedef struct {
  const c8* compile [flags_max];
  const c8* link [flags_max];
  spn_sanitizer_set_t unsupported;
} flags_expect_t;

typedef struct {
  spn_profile_info_t profile;
  spn_cc_driver_t driver;
  flags_expect_t expect;
} flags_test_t;

static void run_flags_test(s32* utest_result, flags_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_flags_t flags = sp_zero;
  spn_err_union_t err = spn_cc_render_flags(scratch.mem, test.driver, &test.profile, &flags);

  if (test.expect.unsupported) {
    EXPECT_EQ(err.kind, SPN_ERR_SANITIZER_UNSUPPORTED);
    EXPECT_EQ(err.sanitizer.unsupported, test.expect.unsupported);
    EXPECT_EQ(err.sanitizer.target.arch, test.profile.arch);
    EXPECT_EQ(err.sanitizer.target.os, test.profile.os);
    EXPECT_EQ(err.sanitizer.target.abi, test.profile.abi);
    sp_mem_end_scratch(scratch);
    return;
  }

  EXPECT_EQ(err.kind, SPN_OK);
  u32 compile = 0;
  sp_carr_for(test.expect.compile, it) {
    if (!test.expect.compile[it]) {
      break;
    }
    compile++;
  }
  u32 link = 0;
  sp_carr_for(test.expect.link, it) {
    if (!test.expect.link[it]) {
      break;
    }
    link++;
  }
  EXPECT_EQ(sp_da_size(flags.compile), compile);
  EXPECT_EQ(sp_da_size(flags.link), link);
  sp_for(it, compile) {
    EXPECT_TRUE(sp_str_equal_cstr(flags.compile[it], test.expect.compile[it]));
  }
  sp_for(it, link) {
    EXPECT_TRUE(sp_str_equal_cstr(flags.link[it], test.expect.link[it]));
  }
  sp_mem_end_scratch(scratch);
}

UTEST(compiler_flags, resolve) {
  flags_test_t tests [] = {
    {
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
        .mode = SPN_BUILD_MODE_DEBUG,
        .opt = SPN_OPT_LEVEL_0,
      },
      .driver = SPN_CC_DRIVER_GCC,
      .expect = { .compile = { "-g", "-O0" } },
    },
    {
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
        .mode = SPN_BUILD_MODE_RELEASE,
        .opt = SPN_OPT_LEVEL_2,
      },
      .driver = SPN_CC_DRIVER_CLANG,
      .expect = { .compile = { "-O2", "-DNDEBUG" } },
    },
    {
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
        .mode = SPN_BUILD_MODE_DEBUG,
        .opt = SPN_OPT_LEVEL_0,
        .sanitizers = SPN_SANITIZER_ADDRESS | SPN_SANITIZER_UNDEFINED,
      },
      .driver = SPN_CC_DRIVER_CLANG,
      .expect = {
        .compile = { "-g", "-O0", "-fsanitize=address,undefined" },
        .link = { "-fsanitize=address,undefined" },
      },
    },
    {
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_WINDOWS,
        .abi = SPN_ABI_MSVC,
        .mode = SPN_BUILD_MODE_RELEASE,
        .opt = SPN_OPT_LEVEL_2,
        .sanitizers = SPN_SANITIZER_ADDRESS,
      },
      .driver = SPN_CC_DRIVER_MSVC,
      .expect = {
        .compile = { "/O2", "/DNDEBUG", "/fsanitize=address" },
        .link = { "/fsanitize=address" },
      },
    },
    {
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_MACOS,
        .sanitizers = SPN_SANITIZER_MEMORY,
      },
      .driver = SPN_CC_DRIVER_CLANG,
      .expect = { .unsupported = SPN_SANITIZER_MEMORY },
    },
    {
      .profile = {
        .arch = SPN_ARCH_WASM32,
        .os = SPN_OS_WASI,
        .sanitizers = SPN_SANITIZER_ADDRESS,
      },
      .driver = SPN_CC_DRIVER_CLANG,
      .expect = { .unsupported = SPN_SANITIZER_ADDRESS },
    },
  };

  sp_carr_for(tests, it) {
    run_flags_test(utest_result, tests[it]);
  }
}
