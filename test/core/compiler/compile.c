#include "common.h"

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_lang_t lang;
  spn_cxx_options_t cxx;
  spn_symbol_visibility_t visibility;
  bool pic;
  const c8* arg;
  const c8* include;
  const c8* define;
  spn_os_version_t min_os;
  render_expect_t expect;
} compile_test_t;

static void run_compile_test(s32* utest_result, compile_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.driver);
  spn_cc_compile_t compile = {
    .lang = test.lang,
    .cxx = test.cxx,
    .source = sp_str_lit("main.c"),
    .output = sp_str_lit("main.o"),
    .visibility = test.visibility,
    .pic = test.pic,
    .min_os = test.min_os,
  };
  sp_da_init(scratch.mem, compile.include);
  sp_da_init(scratch.mem, compile.define);
  sp_da_init(scratch.mem, compile.args);
  if (test.arg) {
    sp_da_push(compile.args, sp_str_from_cstr(scratch.mem, test.arg));
  }
  if (test.include) {
    sp_da_push(compile.include, sp_str_from_cstr(scratch.mem, test.include));
  }
  if (test.define) {
    sp_da_push(compile.define, sp_str_from_cstr(scratch.mem, test.define));
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

UTEST(render_compile, gcc_linux) {
  run_compile_test(utest_result, (compile_test_t) {
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
  });
}

UTEST(render_compile, clang_wasi) {
  run_compile_test(utest_result, (compile_test_t) {
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
  });
}

UTEST(render_compile, msvc_unsupported) {
  run_compile_test(utest_result, (compile_test_t) {
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
  });
}

UTEST(render_compile, clang_macos_sdk) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .standard = SPN_C99,
      .sysroot = sp_str_lit("/sdk"),
    },
    .min_os = { 13 },
    .expect = {
      .command = "cc",
      .args = { "--target=aarch64-macos", "-std=c99", "-c", "main.c", "-isysroot", "/sdk", "-mmacosx-version-min=13.0", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, cxx_defaults) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .lang = SPN_LANG_CXX,
    .cxx = { .no_exceptions = true, .no_rtti = true },
    .expect = {
      .command = "c++",
      .args = { "-std=c++17", "-c", "main.c", "-fno-exceptions", "-fno-rtti", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, cxx_standard) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .lang = SPN_LANG_CXX,
    .cxx = { .standard = SPN_CXX20 },
    .expect = {
      .command = "c++",
      .args = { "-std=c++20", "-c", "main.c", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, includes_and_defines) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .standard = SPN_C99,
    },
    .include = "inc",
    .define = "SPUM=1",
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-c", "main.c", "-Iinc", "-DSPUM=1", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, sanitizers_on_compile_line) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .standard = SPN_C99,
      .sanitizers = SPN_SANITIZER_ADDRESS,
    },
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-fsanitize=address", "-c", "main.c", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, macos_min_os_minor) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .standard = SPN_C99,
    },
    .min_os = { 13, 1 },
    .expect = {
      .command = "cc",
      .args = { "--target=aarch64-macos", "-std=c99", "-c", "main.c", "-mmacosx-version-min=13.1", "-Werror=return-type", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, foreign_platform_config_never_renders) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .standard = SPN_C99,
      .sysroot = sp_str_lit("/sdk"),
    },
    .min_os = { 13 },
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-c", "main.c", "-Werror=return-type", "-o", "main.o" },
    },
  });
}
