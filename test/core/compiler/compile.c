#include "compiler.h"

typedef struct {
  const c8* name;
  spn_cc_driver_t driver;
  test_profile_t profile;
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

static const compile_test_t tests [] = {
  {
    .name = "gcc_linux",
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
  },
  {
    .name = "gcc_depfile",
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
  },
  {
    .name = "clang_wasi",
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
  },
  {
    .name = "msvc_windows",
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
  },
  {
    .name = "msvc_asm_uses_masm",
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
  },
  {
    .name = "msvc_c99_has_no_switch",
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
  },
  {
    .name = "msvc_debug",
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
  },
  {
    .name = "msvc_cxx_defaults",
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
  },
  {
    .name = "msvc_cxx_options",
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
  },
  {
    .name = "clang_macos_sdk",
    .driver = SPN_CC_DRIVER_CLANG,
    .profile = {
      .arch = SPN_ARCH_ARM64,
      .os = SPN_OS_MACOS,
      .standard = SPN_C99,
      .sysroot = "/sdk",
    },
    .min_os = { 13 },
    .expect = {
      .command = "cc",
      .args = { "--target=aarch64-macos", "-std=c99", "-c", "-isysroot", "/sdk", "-iframework", "/sdk/System/Library/Frameworks", "-mmacosx-version-min=13.0", "-Werror=return-type", "main.c", "-o", "main.o" },
    },
  },
  {
    .name = "cxx_defaults",
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
  },
  {
    .name = "cxx_standard",
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
  },
  {
    .name = "includes_and_defines",
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
  },
  {
    .name = "sanitizers_on_compile_line",
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
  },
  {
    .name = "macos_min_os_minor",
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
  },
  {
    .name = "foreign_platform_config_never_renders",
    .driver = SPN_CC_DRIVER_GCC,
    .profile = {
      .arch = SPN_ARCH_X64,
      .os = SPN_OS_LINUX,
      .abi = SPN_ABI_GNU,
      .standard = SPN_C99,
      .sysroot = "/sdk",
    },
    .min_os = { 13 },
    .expect = {
      .command = "cc",
      .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-o", "main.o" },
    },
  },
};

sp_test_each(render_compile, render, compile_test_t, tests, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(it->driver);
  spn_cc_compile_t compile = {
    .lang = it->lang,
    .cxx = it->cxx,
    .pic = it->pic,
    .min_os = it->min_os,
  };
  sp_da_init(mem, compile.include);
  sp_da_init(mem, compile.define);
  sp_da_init(mem, compile.args);
  if (it->arg) {
    sp_da_push(compile.args, sp_str_from_cstr(mem, it->arg));
  }
  if (it->include) {
    sp_da_push(compile.include, test_arg_path(it->include));
  }
  if (it->define) {
    sp_da_push(compile.define, sp_str_from_cstr(mem, it->define));
  }

  spn_profile_info_t profile = test_profile(it->profile);
  spn_invocation_t base = sp_zero;
  spn_err_t err = spn_cc_render_compile(mem, &toolchain, &profile, &compile, &base);
  sp_expect_eq(t, err, it->expect.err);
  if (it->expect.err) {
    sp_da(spn_event_t) errs = spn_test_drain_errs(mem);
    sp_must_eq(t, 1, sp_da_size(errs));
    sp_expect_eq(t, errs[0].err.kind, it->expect.err);
    sp_expect_eq(t, errs[0].err.compiler.feature, it->expect.feature);
    return SP_OK;
  }

  spn_cc_compile_files_t files = {
    .source = test_arg_path("main.c"),
    .output = test_arg_path("main.o"),
    .depfile = it->depfile ? test_arg_path(it->depfile) : sp_zero_s(spn_path_t),
  };
  spn_invocation_t invocation = spn_cc_render_compile_command(mem, &toolchain, &base, &files);
  return expect_args(t, &invocation, it->expect);
}

sp_test(render_compile, base_shared_across_commands, .setup = spn_test_ctx_setup) {
  sp_mem_t mem = sp_test_arena(t);
  spn_cc_toolchain_t toolchain = test_toolchain(SPN_CC_DRIVER_GCC);
  spn_cc_compile_t compile = {
    .lang = SPN_LANG_C,
  };
  sp_da_init(mem, compile.include);
  sp_da_init(mem, compile.define);
  sp_da_init(mem, compile.args);
  spn_profile_info_t profile = {
    .arch = SPN_ARCH_X64,
    .os = SPN_OS_LINUX,
    .abi = SPN_ABI_GNU,
    .standard = SPN_C99,
  };

  spn_invocation_t base = sp_zero;
  spn_err_t err = spn_cc_render_compile(mem, &toolchain, &profile, &compile, &base);
  sp_expect_eq(t, err, SPN_OK);
  u64 args = sp_da_size(base.args);

  spn_cc_compile_files_t first = {
    .source = test_arg_path("main.c"),
    .output = test_arg_path("a.o"),
  };
  spn_cc_compile_files_t second = {
    .source = test_arg_path("main.c"),
    .output = test_arg_path("b.o"),
    .depfile = test_arg_path("b.o.d"),
  };
  spn_invocation_t a = spn_cc_render_compile_command(mem, &toolchain, &base, &first);
  spn_invocation_t b = spn_cc_render_compile_command(mem, &toolchain, &base, &second);

  sp_expect_eq(t, sp_da_size(base.args), args);
  if (expect_args(t, &a, (render_expect_t) {
    .command = "cc",
    .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-o", "a.o" },
  })) {
    return SP_ERR;
  }
  return expect_args(t, &b, (render_expect_t) {
    .command = "cc",
    .args = { "-std=c99", "-c", "-Werror=return-type", "main.c", "-MD", "-MF", "b.o.d", "-o", "b.o" },
  });
}
