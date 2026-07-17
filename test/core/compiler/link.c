#include "common.h"

typedef struct {
  spn_cc_driver_t driver;
  spn_profile_info_t profile;
  spn_cc_output_kind_t kind;
  const c8* exports;
  const c8* export_symbols [2];
  const c8* whole_archive;
  const c8* private_lib;
  const c8* system_lib;
  const c8* framework;
  const c8* lib_dir;
  const c8* rpath;
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
  sp_da_init(scratch.mem, link.whole_archives);
  sp_da_init(scratch.mem, link.private_libs);
  sp_da_init(scratch.mem, link.system_libs);
  sp_da_init(scratch.mem, link.lib_dirs);
  sp_da_init(scratch.mem, link.rpath);
  sp_da_init(scratch.mem, link.exports.symbols);
  sp_da_init(scratch.mem, link.frameworks);
  sp_da_push(link.objects, sp_str_lit("main.o"));
  if (test.exports) {
    link.exports.path = sp_str_from_cstr(scratch.mem, test.exports);
  }
  sp_carr_for(test.export_symbols, it) {
    if (!test.export_symbols[it]) break;
    sp_da_push(link.exports.symbols, sp_str_from_cstr(scratch.mem, test.export_symbols[it]));
  }
  if (test.whole_archive) {
    sp_da_push(link.whole_archives, sp_str_from_cstr(scratch.mem, test.whole_archive));
  }
  if (test.private_lib) {
    sp_da_push(link.private_libs, sp_str_from_cstr(scratch.mem, test.private_lib));
  }
  if (test.system_lib) {
    sp_da_push(link.system_libs, sp_str_from_cstr(scratch.mem, test.system_lib));
  }
  if (test.framework) {
    sp_da_push(link.frameworks, sp_str_from_cstr(scratch.mem, test.framework));
  }
  if (test.lib_dir) {
    sp_da_push(link.lib_dirs, sp_str_from_cstr(scratch.mem, test.lib_dir));
  }
  if (test.rpath) {
    sp_da_push(link.rpath, sp_str_from_cstr(scratch.mem, test.rpath));
  }
  link.subsystem = test.subsystem;
  spn_invocation_t invocation = sp_zero;
  spn_err_union_t err = spn_cc_render_link(scratch.mem, &toolchain, &test.profile, &link, &invocation);
  EXPECT_EQ(err.kind, test.expect.err);
  if (test.expect.err) {
    EXPECT_EQ(err.compiler.feature, test.expect.feature);
  } else {
    expect_args(utest_result, &invocation, test.expect);
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
    .system_lib = "m",
    .expect = {
      .command = "cc",
      .args = {
        "main.o",
        "-lm",
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
        "-Wl,--no-entry", "-Wl,--import-symbols",
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

UTEST(render_link, msvc_exe_libs) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .private_lib = "spum",
    .system_lib = "ws2_32",
    .lib_dir = "deps/lib",
    .expect = {
      .command = "cc",
      .args = {
        "/nologo",
        "/LD",
        "main.o",
        "spum.lib", "ws2_32.lib",
        "/Femain",
        "/link", "/LIBPATH:deps/lib"
      },
    },
  });
}

UTEST(render_link, msvc_shared_lib) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .expect = {
      .command = "cc",
      .args = { "/nologo", "/LD", "main.o", "/Femain" },
    },
  });
}

UTEST(render_link, msvc_debug_pdb) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .mode = SPN_BUILD_MODE_DEBUG,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .expect = {
      .command = "cc",
      .args = { "/nologo", "main.o", "/Femain", "/link", "/DEBUG" },
    },
  });
}

UTEST(render_link, msvc_subsystem) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .subsystem = SPN_WIN_SUBSYSTEM_WINDOWS,
    .expect = {
      .command = "cc",
      .args = { "/nologo", "main.o", "/Femain", "/link", "/SUBSYSTEM:WINDOWS" },
    },
  });
}

UTEST(render_link, msvc_reactor_unsupported) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
    },
    .kind = SPN_CC_OUTPUT_REACTOR,
    .expect = {
      .err = SPN_ERR_COMPILER_FEATURE_UNSUPPORTED,
      .feature = SPN_CC_FEATURE_LINK_REACTOR,
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

UTEST(render_link, linux_shared_lib) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .expect = {
      .command = "cc",
      .args = { "-shared", "main.o", "-o", "main" },
    },
  });
}

UTEST(render_link, static_linkage) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .linkage = SPN_LIB_KIND_STATIC,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .expect = {
      .command = "cc",
      .args = { "-static", "main.o", "-o", "main" },
    },
  });
}

UTEST(render_link, macos_static_linkage_suppressed) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .linkage = SPN_LIB_KIND_STATIC,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .expect = {
      .command = "cc",
      .args = { "--target=aarch64-macos", "main.o", "-o", "main" },
    },
  });
}

UTEST(render_link, lib_dirs_and_rpath) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .lib_dir = "deps/lib",
    .rpath = "$ORIGIN",
    .expect = {
      .command = "cc",
      .args = { "main.o", "-Ldeps/lib", "-Wl,-rpath,$ORIGIN", "-o", "main" },
    },
  });
}

UTEST(render_link, sanitizers_on_link_line) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .sanitizers = SPN_SANITIZER_ADDRESS,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .expect = {
      .command = "cc",
      .args = { "-fsanitize=address", "main.o", "-o", "main" },
    },
  });
}

UTEST(render_link, linux_shared_exports) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .exports = "S.map",
    .whole_archive = "libD.a",
    .private_lib = "P",
    .expect = {
      .command = "cc",
      .args = {
        "-shared", "-Wl,--version-script,S.map",
        "main.o",
        "-Wl,--whole-archive", "libD.a", "-Wl,--no-whole-archive",
        "-lP",
        "-o", "main"
      },
    },
  });
}

UTEST(render_link, macos_shared_exports) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .exports = "S.exp",
    .whole_archive = "libD.a",
    .private_lib = "P",
    .expect = {
      .command = "cc",
      .args = {
        "--target=aarch64-macos",
        "-shared", "-Wl,-exported_symbols_list,S.exp",
        "main.o",
        "-Wl,-force_load,libD.a",
        "-lP",
        "-o", "main"
      },
    },
  });
}

UTEST(render_link, mingw_shared_def) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MINGW,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .exports = "S.def",
    .whole_archive = "libD.a",
    .private_lib = "P",
    .expect = {
      .command = "cc",
      .args = {
        "-shared", "S.def",
        "main.o",
        "-Wl,--whole-archive", "libD.a", "-Wl,--no-whole-archive",
        "-lP", "-Wl,--exclude-libs,libP.a",
        "-o", "main"
      },
    },
  });
}

UTEST(render_link, wasi_reactor_exports) {
  run_link_test(utest_result, (link_test_t) {
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
    },
    .kind = SPN_CC_OUTPUT_REACTOR,
    .export_symbols = { "A", "B" },
    .expect = {
      .command = "cc",
      .args = {
        "--target=wasm32-wasi",
        "-mexec-model=reactor",
        "-Wl,--no-entry", "-Wl,--import-symbols",
        "-Wl,--export=A", "-Wl,--export=B",
        "main.o", "-o", "main"
      },
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
