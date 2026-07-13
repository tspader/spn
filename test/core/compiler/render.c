#include "compiler/driver.h"
#include "sp/macro.h"
#include "utest.h"

#define render_args_max 20

typedef struct {
  spn_err_t err;
  spn_cc_feature_t feature;
  const c8* command;
  const c8* args [render_args_max];
} render_expect_t;

static void expect_args(s32* utest_result, sp_ps_config_t* ps, render_expect_t expect) {
  EXPECT_TRUE(sp_str_equal_cstr(ps->command, expect.command));
  u32 count = 0;
  sp_carr_for(expect.args, it) {
    if (!expect.args[it]) {
      break;
    }
    count++;
  }
  EXPECT_EQ(sp_da_size(ps->dyn_args), count);
  sp_for(it, count) {
    EXPECT_TRUE(sp_str_equal_cstr(ps->dyn_args[it], expect.args[it]));
  }
}

static spn_cc_toolchain_t test_toolchain(spn_cc_driver_t driver) {
  return (spn_cc_toolchain_t) {
    .name = sp_str_lit("test"),
    .driver = driver,
    .compiler = { .program = sp_str_lit("cc") },
    .cxx = { .program = sp_str_lit("c++") },
    .archiver = { .program = sp_str_lit("ar") },
  };
}

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_symbol_visibility_t visibility;
  bool pic;
  const c8* arg;
  render_expect_t expect;
} compile_test_t;

static void run_compile_test(s32* utest_result, compile_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.driver);
  spn_cc_compile_t compile = {
    .lang = SPN_LANG_C,
    .source = sp_str_lit("main.c"),
    .output = sp_str_lit("main.o"),
    .visibility = test.visibility,
    .pic = test.pic,
  };
  sp_da_init(scratch.mem, compile.include);
  sp_da_init(scratch.mem, compile.define);
  sp_da_init(scratch.mem, compile.args);
  if (test.arg) {
    sp_da_push(compile.args, sp_str_from_cstr(scratch.mem, test.arg));
  }
  sp_ps_config_t ps = sp_zero;
  spn_err_union_t err = spn_cc_render_compile(scratch.mem, &toolchain, &test.profile, &compile, &ps);
  EXPECT_EQ(err.kind, test.expect.err);
  if (test.expect.err) {
    EXPECT_EQ(err.compiler.feature, test.expect.feature);
  } else {
    expect_args(utest_result, &ps, test.expect);
  }
  sp_mem_end_scratch(scratch);
}

UTEST(compiler_render, compile) {
  compile_test_t tests [] = {
    {
      .driver = SPN_CC_DRIVER_GCC,
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
        .standard = SPN_C99,
      },
      .visibility = SPN_SYMBOL_VISIBILITY_HIDDEN,
      .pic = true,
      .arg = "-fno-common",
      .expect = {
        .command = "cc",
        .args = { "-std=c99", "-c", "main.c", "-fPIC", "-fvisibility=hidden", "-fno-common", "-Werror=return-type", "-o", "main.o" },
      },
    },
    {
      .driver = SPN_CC_DRIVER_CLANG,
      .profile = {
        .arch = SPN_ARCH_WASM32,
        .os = SPN_OS_WASI,
        .standard = SPN_C99,
        .opt = SPN_OPT_LEVEL_2,
      },
      .visibility = SPN_SYMBOL_VISIBILITY_HIDDEN,
      .expect = {
        .command = "cc",
        .args = { "--target=wasm32-wasi", "-std=c99", "-O2", "-c", "main.c", "-fvisibility=hidden", "-Werror=return-type", "-o", "main.o" },
      },
    },
    {
      .driver = SPN_CC_DRIVER_MSVC,
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_WINDOWS,
        .abi = SPN_ABI_MSVC,
      },
      .expect = {
        .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
        .feature = SPN_CC_FEATURE_COMPILE,
      },
    },
  };
  sp_carr_for(tests, it) {
    run_compile_test(utest_result, tests[it]);
  }
}

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_cc_output_kind_t kind;
  const c8* hidden_lib;
  const c8* system_lib;
  render_expect_t expect;
} link_test_t;

static void run_link_test(s32* utest_result, link_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.driver);
  spn_cc_link_t link = {
    .lang = SPN_LANG_C,
    .kind = test.kind,
    .output = sp_str_lit("main"),
  };
  sp_da_init(scratch.mem, link.objects);
  sp_da_init(scratch.mem, link.args);
  sp_da_init(scratch.mem, link.libs);
  sp_da_init(scratch.mem, link.system_libs);
  sp_da_init(scratch.mem, link.hidden_libs);
  sp_da_init(scratch.mem, link.lib_dirs);
  sp_da_init(scratch.mem, link.rpath);
  sp_da_push(link.objects, sp_str_lit("main.o"));
  if (test.hidden_lib) {
    sp_da_push(link.hidden_libs, sp_str_from_cstr(scratch.mem, test.hidden_lib));
  }
  if (test.system_lib) {
    sp_da_push(link.system_libs, sp_str_from_cstr(scratch.mem, test.system_lib));
  }
  sp_ps_config_t ps = sp_zero;
  spn_err_union_t err = spn_cc_render_link(scratch.mem, &toolchain, &test.profile, &link, &ps);
  EXPECT_EQ(err.kind, test.expect.err);
  if (test.expect.err) {
    EXPECT_EQ(err.compiler.feature, test.expect.feature);
  } else {
    expect_args(utest_result, &ps, test.expect);
  }
  sp_mem_end_scratch(scratch);
}

UTEST(compiler_render, link) {
  link_test_t tests [] = {
    {
      .driver = SPN_CC_DRIVER_GCC,
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
      },
      .kind = SPN_CC_OUTPUT_EXE,
      .hidden_lib = "spum",
      .system_lib = "m",
      .expect = {
        .command = "cc",
        .args = { "main.o", "-lspum", "-Wl,--exclude-libs,libspum.a", "-lm", "-o", "main" },
      },
    },
    {
      .driver = SPN_CC_DRIVER_CLANG,
      .profile = {
        .arch = SPN_ARCH_WASM32,
        .os = SPN_OS_WASI,
      },
      .kind = SPN_CC_OUTPUT_REACTOR,
      .expect = {
        .command = "cc",
        .args = { "--target=wasm32-wasi", "-mexec-model=reactor", "-Wl,--no-entry", "-Wl,--import-symbols", "-Wl,--export-dynamic", "main.o", "-o", "main" },
      },
    },
    {
      .driver = SPN_CC_DRIVER_CLANG,
      .profile = {
        .arch = SPN_ARCH_WASM32,
        .os = SPN_OS_WASI,
      },
      .kind = SPN_CC_OUTPUT_SHARED_LIB,
      .expect = {
        .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
        .feature = SPN_CC_FEATURE_LINK_SHARED,
      },
    },
    {
      .driver = SPN_CC_DRIVER_GCC,
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_LINUX,
        .abi = SPN_ABI_GNU,
      },
      .kind = SPN_CC_OUTPUT_REACTOR,
      .expect = {
        .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
        .feature = SPN_CC_FEATURE_LINK_REACTOR,
      },
    },
    {
      .driver = SPN_CC_DRIVER_MSVC,
      .profile = {
        .arch = SPN_ARCH_X64,
        .os = SPN_OS_WINDOWS,
        .abi = SPN_ABI_MSVC,
      },
      .kind = SPN_CC_OUTPUT_SHARED_LIB,
      .expect = {
        .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
        .feature = SPN_CC_FEATURE_LINK_SHARED,
      },
    },
  };
  sp_carr_for(tests, it) {
    run_link_test(utest_result, tests[it]);
  }
}

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

UTEST(compiler_render, archive) {
  archive_test_t tests [] = {
    {
      .compiler = SPN_CC_DRIVER_MSVC,
      .archiver = SPN_AR_DRIVER_GNU,
      .expect = {
        .command = "ar",
        .args = { "rcs", "libmain.a", "main.o" },
      },
    },
    {
      .compiler = SPN_CC_DRIVER_GCC,
      .archiver = SPN_AR_DRIVER_MSVC,
      .expect = { .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED },
    },
  };
  sp_carr_for(tests, it) {
    run_archive_test(utest_result, tests[it]);
  }
}
