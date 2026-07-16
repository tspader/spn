#include "common.h"

typedef struct {
  spn_cc_driver_t compiler;
  spn_ar_driver_t archiver;
  render_expect_t expect;
} archive_test_t;

static void run_archive_test(s32* utest_result, archive_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.compiler);
  toolchain.archiver_driver = test.archiver;
  spn_profile_info_t profile = {
    .arch = SPN_ARCH_X64,
    .os = SPN_OS_LINUX,
    .abi = SPN_ABI_GNU,
  };
  spn_cc_archive_t archive = { .output = sp_str_lit("libmain.a") };
  sp_da_init(scratch.mem, archive.objects);
  sp_da_init(scratch.mem, archive.args);
  sp_da_push(archive.objects, sp_str_lit("main.o"));
  sp_ps_config_t ps = sp_zero;
  spn_err_union_t err = spn_cc_render_archive(scratch.mem, &toolchain, &profile, &archive, &ps);
  EXPECT_EQ(err.kind, test.expect.err);
  if (test.expect.err) {
    EXPECT_EQ(err.compiler.feature, SPN_CC_FEATURE_ARCHIVE);
  } else {
    expect_args(utest_result, &ps, test.expect);
  }
  sp_mem_end_scratch(scratch);
}

UTEST(render_archive, gnu_archiver) {
  run_archive_test(utest_result, (archive_test_t) {
    .compiler = SPN_CC_DRIVER_MSVC,
    .archiver = SPN_AR_DRIVER_GNU,
    .expect = {
      .command = "ar",
      .args = { "rcs", "libmain.a", "main.o" },
    },
  });
}

UTEST(render_archive, msvc_archiver) {
  run_archive_test(utest_result, (archive_test_t) {
    .compiler = SPN_CC_DRIVER_GCC,
    .archiver = SPN_AR_DRIVER_MSVC,
    .expect = {
      .command = "ar",
      .args = { "/nologo", "/OUT:libmain.a", "main.o" },
    },
  });
}
