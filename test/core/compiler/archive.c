#include "compiler.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t compiler;
  spn_ar_driver_t archiver;
  render_expect_t expect;
} archive_test_t;

static const archive_test_t tests [] = {
  {
    .name = "gnu_archiver",
    .compiler = SPN_CC_DRIVER_MSVC,
    .archiver = SPN_AR_DRIVER_GNU,
    .expect = {
      .command = "ar",
      .args = { "rcs", "libmain.a", "main.o" },
    },
  },
  {
    .name = "msvc_archiver",
    .compiler = SPN_CC_DRIVER_GCC,
    .archiver = SPN_AR_DRIVER_MSVC,
    .expect = {
      .command = "ar",
      .args = { "/nologo", "/OUT:libmain.a", "main.o" },
    },
  },
};

sp_test_each(render_archive, render, archive_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(it->compiler);
  toolchain.archiver_driver = it->archiver;
  spn_profile_info_t profile = {
    .arch = SPN_ARCH_X64,
    .os = SPN_OS_LINUX,
    .abi = SPN_ABI_GNU,
  };
  spn_cc_archive_files_t files = {
    .output = test_arg_path("libmain.a"),
  };
  sp_da_init(mem, files.objects);
  sp_da_push(files.objects, test_arg_path("main.o"));

  spn_invocation_t invocation = sp_zero;
  spn_err_t err = spn_cc_render_archive(mem, &toolchain, &profile, &files, &invocation);
  sp_expect_eq(t, err, it->expect.err);
  if (it->expect.err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, it->expect.err);
    sp_expect_eq(t, errs[0].err.compiler.feature, SPN_CC_FEATURE_ARCHIVE);
    return SP_OK;
  }
  return expect_args(t, &invocation, it->expect);
}
