#include "compiler.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  test_profile_t profile;
  spn_cc_output_kind_t kind;
  const c8* exports;
  const c8* export_symbols [2];
  const c8* lib;
  const c8* whole_archive;
  const c8* private_lib;
  const c8* system_lib;
  const c8* framework;
  const c8* lib_dir;
  bool rpath;
  spn_os_version_t min_os;
  spn_win_subsystem_t subsystem;
  render_expect_t expect;
} link_test_t;

static const link_test_t tests [] = {
  {
    .name = "gcc_linux_libs",
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
  },
  {
    .name = "clang_wasi_reactor",
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
  },
  {
    .name = "wasi_shared_lib_unsupported",
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
    .name = "gcc_reactor_unsupported",
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
    .name = "msvc_exe_libs",
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
  },
  {
    .name = "msvc_shared_lib",
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
  },
  {
    .name = "msvc_debug_pdb",
    .driver = SPN_CC_DRIVER_MSVC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_MSVC,
      .mode = SPN_MODE_DEBUG,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .expect = {
      .command = "cc",
      .args = { "/nologo", "main.o", "/Femain", "/link", "/DEBUG" },
    },
  },
  {
    .name = "msvc_subsystem",
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
  },
  {
    .name = "msvc_reactor_unsupported",
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
  },
  {
    .name = "clang_macos_frameworks",
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .sysroot = "/sdk",
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
        "-F/sdk/System/Library/Frameworks",
        "-L/sdk/usr/lib",
        "-mmacosx-version-min=13.0",
        "-framework", "Cocoa",
        "-o", "main"
      },
    },
  },
  {
    .name = "frameworks_require_sdk",
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
  },
  {
    .name = "windows_gnu_subsystem",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .subsystem = SPN_WIN_SUBSYSTEM_WINDOWS,
    .expect = {
      .command = "cc",
      .args = { "-Wl,--subsystem,windows", "main.o", "-o", "main" },
    },
  },
  {
    .name = "linux_shared_lib",
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
  },
  {
    .name = "static_linkage",
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
  },
  {
    .name = "macos_static_linkage_suppressed",
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
  },
  {
    .name = "lib_dirs_and_rpath",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .lib_dir = "deps/lib",
    .rpath = true,
    .expect = {
      .command = "cc",
      .args = { "main.o", "-Ldeps/lib", "-Wl,-rpath,$ORIGIN", "-o", "main" },
    },
  },
  {
    .name = "macos_rpath",
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .rpath = true,
    .expect = {
      .command = "cc",
      .args = { "--target=aarch64-macos", "main.o", "-Wl,-rpath,@loader_path", "-o", "main" },
    },
  },
  {
    .name = "wasi_rpath_never_renders",
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_WASM32,
      .os = SPN_OS_WASI,
    },
    .kind = SPN_CC_OUTPUT_REACTOR,
    .rpath = true,
    .expect = {
      .command = "cc",
      .args = {
        "--target=wasm32-wasi",
        "-mexec-model=reactor",
        "-Wl,--no-entry", "-Wl,--import-symbols",
        "main.o", "-o", "main"
      },
    },
  },
  {
    .name = "libs_precede_system_libs",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .lib = "A",
    .private_lib = "P",
    .system_lib = "m",
    .expect = {
      .command = "cc",
      .args = { "main.o", "-lP", "-lA", "-lm", "-o", "main" },
    },
  },
  {
    .name = "sanitizers_on_link_line",
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
  },
  {
    .name = "linux_shared_exports",
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
  },
  {
    .name = "macos_shared_exports",
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
        "-shared", "-Wl,-install_name,@rpath/main", "-Wl,-exported_symbols_list,S.exp",
        "main.o",
        "-Wl,-force_load,libD.a",
        "-lP",
        "-o", "main"
      },
    },
  },
  {
    .name = "windows_gnu_shared_def",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_GNU,
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
  },
  {
    .name = "windows_zig_shared_def",
    .driver = SPN_CC_DRIVER_ZIG,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_WINDOWS,
      .abi = SPN_ABI_GNU,
    },
    .kind = SPN_CC_OUTPUT_SHARED_LIB,
    .exports = "S.def",
    .whole_archive = "libD.a",
    .private_lib = "P",
    .expect = {
      .command = "cc",
      .args = {
        "--target=x86_64-windows-gnu",
        "-shared", "S.def",
        "main.o",
        "-Wl,--whole-archive", "libD.a", "-Wl,--no-whole-archive",
        "-lP",
        "-o", "main"
      },
    },
  },
  {
    .name = "wasi_reactor_exports",
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
  },
  {
    .name = "foreign_platform_config_never_renders",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .sysroot = "/sdk",
    },
    .kind = SPN_CC_OUTPUT_EXE,
    .framework = "Cocoa",
    .min_os = { 13 },
    .subsystem = SPN_WIN_SUBSYSTEM_WINDOWS,
    .expect = {
      .command = "cc",
      .args = { "main.o", "-o", "main" },
    },
  },
};

sp_test_each(render_link, render, link_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);
  spn_cc_link_t link = {
    .lang = SPN_LANG_C,
    .kind = it->kind,
    .min_os = it->min_os,
    .rpath = it->rpath,
    .subsystem = it->subsystem,
  };
  sp_da_init(mem, link.libs);
  sp_da_init(mem, link.private_libs);
  sp_da_init(mem, link.system_libs);
  sp_da_init(mem, link.lib_dirs);
  sp_da_init(mem, link.frameworks);

  spn_cc_link_files_t files = {
    .output = test_arg_path("main"),
  };
  sp_da_init(mem, files.objects);
  sp_da_init(mem, files.whole_archives);
  sp_da_init(mem, files.exports.symbols);
  sp_da_push(files.objects, test_arg_path("main.o"));
  if (it->exports) {
    files.exports.path = test_arg_path(it->exports);
  }
  sp_carr_for(it->export_symbols, s) {
    if (!it->export_symbols[s]) break;
    sp_da_push(files.exports.symbols, sp_str_from_cstr(mem, it->export_symbols[s]));
  }
  if (it->lib) {
    sp_da_push(link.libs, sp_str_from_cstr(mem, it->lib));
  }
  if (it->whole_archive) {
    sp_da_push(files.whole_archives, test_arg_path(it->whole_archive));
  }
  if (it->private_lib) {
    sp_da_push(link.private_libs, sp_str_from_cstr(mem, it->private_lib));
  }
  if (it->system_lib) {
    sp_da_push(link.system_libs, sp_str_from_cstr(mem, it->system_lib));
  }
  if (it->framework) {
    sp_da_push(link.frameworks, sp_str_from_cstr(mem, it->framework));
  }
  if (it->lib_dir) {
    sp_da_push(link.lib_dirs, test_arg_path(it->lib_dir));
  }

  spn_profile_info_t profile = test_profile(it->profile);
  spn_invocation_t invocation = sp_zero;
  spn_err_t err = spn_cc_render_link(mem, &toolchain, &profile, &link, &files, &invocation);
  sp_expect_eq(t, err, it->expect.err);
  if (it->expect.err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, it->expect.err);
    sp_expect_eq(t, errs[0].err.compiler.feature, it->expect.feature);
    return SP_OK;
  }
  return expect_args(t, &invocation, it->expect);
}
