#include "common.h"

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_lang_t lang;
  spn_cxx_options_t cxx;
  bool pic;
  const c8* arg;
  const c8* include;
  const c8* define;
  const c8* depfile;
  spn_os_version_t min_os;
  render_expect_t expect;
} compile_test_t;

static void run_compile_test(s32* utest_result, compile_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.driver);
  spn_cc_compile_t compile = {
    .lang = test.lang,
    .cxx = test.cxx,
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
  spn_invocation_t base = sp_zero;
  spn_err_union_t err = spn_cc_render_compile(scratch.mem, &toolchain, &test.profile, &compile, &base);
  EXPECT_EQ(err.kind, test.expect.err);
  if (test.expect.err) {
    EXPECT_EQ(err.compiler.feature, test.expect.feature);
  } else {
    spn_cc_compile_files_t files = {
      .source = sp_str_lit("main.c"),
      .output = sp_str_lit("main.o"),
      .depfile = test.depfile ? sp_str_from_cstr(scratch.mem, test.depfile) : sp_str_lit(""),
    };
    spn_invocation_t invocation = spn_cc_render_compile_command(scratch.mem, &toolchain, &base, &files);
    expect_args(utest_result, &invocation, test.expect);
  }
  sp_mem_end_scratch(scratch);
}

UTEST(render_compile, base_shared_across_commands) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(SPN_CC_DRIVER_GCC);
  spn_cc_compile_t compile = {
    .lang = SPN_LANG_C,
  };
  sp_da_init(scratch.mem, compile.include);
  sp_da_init(scratch.mem, compile.define);
  sp_da_init(scratch.mem, compile.args);
  spn_profile_info_t profile = {
    .arch = SPN_ARCH_X64,
    .os = SPN_OS_LINUX,
    .abi = SPN_ABI_GNU,
    .standard = SPN_C99,
  };

  spn_invocation_t base = sp_zero;
  spn_err_union_t err = spn_cc_render_compile(scratch.mem, &toolchain, &profile, &compile, &base);
  EXPECT_EQ(err.kind, SPN_OK);
  u64 args = sp_da_size(base.args);

  spn_cc_compile_files_t first = {
    .source = sp_str_lit("main.c"),
    .output = sp_str_lit("a.o"),
  };
  spn_cc_compile_files_t second = {
    .source = sp_str_lit("main.c"),
    .output = sp_str_lit("b.o"),
    .depfile = sp_str_lit("b.o.d"),
  };
  spn_invocation_t a = spn_cc_render_compile_command(scratch.mem, &toolchain, &base, &first);
  spn_invocation_t b = spn_cc_render_compile_command(scratch.mem, &toolchain, &base, &second);

  EXPECT_EQ(sp_da_size(base.args), args);
  expect_args(utest_result, &a, (render_expect_t) {
    .command = "cc",
    .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-o", "a.o" },
  });
  expect_args(utest_result, &b, (render_expect_t) {
    .command = "cc",
    .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-MD", "-MF", "b.o.d", "-o", "b.o" },
  });
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
    .pic = true,
    .arg = "-fno-common",
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-c", "-fPIC", "-fno-common", "-Werror=return-type", "main.c", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, gcc_depfile) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .standard = SPN_C99,
    },
    .depfile = "main.o.d",
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-MD", "-MF", "main.o.d", "-o", "main.o" },
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
    .expect = {
      .command = "cc",
      .args = { "--target=wasm32-wasi", "-std=c99", "-O2", "-c", "-Werror=return-type", "main.c", "-o", "main.o" },
    },
  });
}

UTEST(render_compile, msvc_windows) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .standard = SPN_C11,
    },
    .include = "inc",
    .define = "SPUM=1",
    .expect = {
      .command = "cc",
      .args = { "/nologo", "/utf-8", "/std:c11", "/c", "/Iinc", "/DSPUM=1", "/we4715", "/Fomain.o", "main.c" },
    },
  });
}

UTEST(render_compile, msvc_asm_uses_masm) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .lang = SPN_LANG_ASM,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .standard = SPN_C11,
      .mode = SPN_BUILD_MODE_DEBUG,
    },
    .include = "inc",
    .define = "SPUM=1",
    .expect = {
      .command = "ml64",
      .args = { "/nologo", "/c", "/Fomain.o", "main.c" },
    },
  });
}

UTEST(render_compile, msvc_c99_has_no_switch) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .standard = SPN_C99,
    },
    .expect = {
      .command = "cc",
      .args = { "/nologo", "/utf-8", "/c", "/we4715", "/Fomain.o", "main.c" },
    },
  });
}

UTEST(render_compile, msvc_debug) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .standard = SPN_C11,
      .mode = SPN_BUILD_MODE_DEBUG,
      .opt = SPN_OPT_LEVEL_0,
    },
    .expect = {
      .command = "cc",
      .args = { "/nologo", "/utf-8", "/std:c11", "/Z7", "/Od", "/c", "/we4715", "/Fomain.o", "main.c" },
    },
  });
}

UTEST(render_compile, msvc_cxx_defaults) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .lang = SPN_LANG_CXX,
    .expect = {
      .command = "c++",
      .args = { "/nologo", "/utf-8", "/std:c++17", "/c", "/EHsc", "/we4715", "/Fomain.o", "main.c" },
    },
  });
}

UTEST(render_compile, msvc_cxx_options) {
  run_compile_test(utest_result, (compile_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .lang = SPN_LANG_CXX,
    .cxx = { .standard = SPN_CXX20, .no_exceptions = true, .no_rtti = true },
    .expect = {
      .command = "c++",
      .args = { "/nologo", "/utf-8", "/std:c++20", "/c", "/GR-", "/we4715", "/Fomain.o", "main.c" },
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
      .args = { "--target=aarch64-macos", "-std=c99", "-c", "-isysroot", "/sdk", "-mmacosx-version-min=13.0", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "-std=c++17", "-c", "-fno-exceptions", "-fno-rtti", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "-std=c++20", "-c", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "-std=c99", "-c", "-Iinc", "-DSPUM=1", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "-std=c99", "-fsanitize=address", "-fno-sanitize-recover=all", "-fno-omit-frame-pointer", "-c", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "--target=aarch64-macos", "-std=c99", "-c", "-mmacosx-version-min=13.1", "-Werror=return-type", "main.c", "-o", "main.o" },
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
      .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-o", "main.o" },
    },
  });
}
