#include "common.h"

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_cc_output_kind_t kind;
  const c8* hidden_lib;
  const c8* system_lib;
  const c8* framework;
  spn_os_version_t min_os;
  spn_win_subsystem_t subsystem;
  render_expect_t expect;
} link_test_t;

static void run_link_test(s32* utest_result, link_test_t test) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_cc_toolchain_t toolchain = test_toolchain(test.driver);
  spn_cc_link_t link = {
    .lang = SPN_LANG_C,
    .kind = test.kind,
    .output = sp_str_lit("main"),
    .min_os = test.min_os,
  };
  sp_da_init(scratch.mem, link.objects);
  sp_da_init(scratch.mem, link.args);
  sp_da_init(scratch.mem, link.libs);
  sp_da_init(scratch.mem, link.system_libs);
  sp_da_init(scratch.mem, link.hidden_libs);
  sp_da_init(scratch.mem, link.lib_dirs);
  sp_da_init(scratch.mem, link.rpath);
  sp_da_init(scratch.mem, link.frameworks);
  sp_da_push(link.objects, sp_str_lit("main.o"));
  if (test.hidden_lib) {
    sp_da_push(link.hidden_libs, sp_str_from_cstr(scratch.mem, test.hidden_lib));
  }
  if (test.system_lib) {
    sp_da_push(link.system_libs, sp_str_from_cstr(scratch.mem, test.system_lib));
  }
  if (test.framework) {
    sp_da_push(link.frameworks, sp_str_from_cstr(scratch.mem, test.framework));
  }
  link.subsystem = test.subsystem;
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

UTEST(render_link, gcc_linux_libs) {
  run_link_test(utest_result, (link_test_t) {
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
      .args = {
        "main.o",
        "-lspum", "-Wl,--exclude-libs,libspum.a", "-lm",
        "-o", "main"
      },
    },
  });
}

UTEST(render_link, clang_wasi_reactor) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
    },
    .kind = SPN_CC_OUTPUT_REACTOR,
    .expect = {
      .command = "cc",
      .args = {
        "--target=wasm32-wasi",
        "-mexec-model=reactor",
        "-Wl,--no-entry", "-Wl,--import-symbols", "-Wl,--export-dynamic",
        "main.o", "-o", "main"
      },
    },
  });
}

UTEST(render_link, wasi_shared_lib_unsupported) {
  run_link_test(utest_result, (link_test_t) {
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
  });
}

UTEST(render_link, gcc_reactor_unsupported) {
  run_link_test(utest_result, (link_test_t) {
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
  });
}

UTEST(render_link, msvc_shared_lib_unsupported) {
  run_link_test(utest_result, (link_test_t) {
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
  });
}

UTEST(render_link, clang_macos_frameworks) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .sysroot = sp_str_lit("/sdk"),
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .framework = "Cocoa",
    .min_os = { 13 },
    .expect = {
      .command = "cc",
      .args = {
        "--target=aarch64-macos",
        "main.o",
        "-isysroot", "/sdk",
        "-mmacosx-version-min=13.0",
        "-framework", "Cocoa",
        "-o", "main"
      },
    },
  });
}

UTEST(render_link, frameworks_require_sdk) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .framework = "Cocoa",
    .expect = {
      .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
      .feature = SPN_CC_FEATURE_FRAMEWORKS,
    },
  });
}

UTEST(render_link, mingw_subsystem) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MINGW,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .subsystem = SPN_WIN_SUBSYSTEM_WINDOWS,
    .expect = {
      .command = "cc",
      .args = { "-Wl,--subsystem,windows", "main.o", "-o", "main" },
    },
  });
}

UTEST(render_link, foreign_platform_config_never_renders) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .sysroot = sp_str_lit("/sdk"),
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .framework = "Cocoa",
    .min_os = { 13 },
    .subsystem = SPN_WIN_SUBSYSTEM_WINDOWS,
    .expect = {
      .command = "cc",
      .args = { "main.o", "-o", "main" },
    },
  });
}
