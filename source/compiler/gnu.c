#include "compiler/driver.h"

#include "enum/enum.h"
#include "sp/macro.h"
#include "triple/triple.h"

static void push_flag(sp_da(sp_str_t)* flags, sp_str_t flag) {
  if (!sp_str_empty(flag)) {
    sp_da_push(*flags, flag);
  }
}

static void push_arg_fmt(sp_mem_t mem, sp_ps_config_t* ps, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_r str = sp_fmt_mem_v(mem, sp_cstr_as_str(fmt), args);
  va_end(args);

  sp_ps_config_add_arg(mem, ps, str.value);
}

static void push_arg(sp_mem_t mem, sp_ps_config_t* ps, const c8* arg) {
  sp_ps_config_add_arg(mem, ps, sp_cstr_as_str(arg));
}

static void push_arg_str(sp_mem_t mem, sp_ps_config_t* ps, sp_str_t arg) {
  sp_ps_config_add_arg(mem, ps, arg);
}

static void push_args(sp_mem_t mem, sp_ps_config_t* ps, sp_da(sp_str_t) args) {
  sp_da_for(args, it) {
    sp_ps_config_add_arg(mem, ps, args[it]);
  }
}


static sp_str_t render_define(sp_mem_t mem, sp_str_t value) {
  return sp_fmt(mem, "-D{}", sp_fmt_str(value)).value;
}

static sp_str_t render_define_c(sp_mem_t mem, const c8* value) {
  return render_define(mem, sp_cstr_as_str(value));
}

static sp_str_t render_include(sp_mem_t mem, sp_str_t value) {
  return sp_fmt(mem, "-I{}", sp_fmt_str(value)).value;
}

static sp_str_t opt_switch(spn_opt_level_t level) {
  switch (level) {
    case SPN_OPT_LEVEL_0: return sp_str_lit("-O0");
    case SPN_OPT_LEVEL_1: return sp_str_lit("-O1");
    case SPN_OPT_LEVEL_2: return sp_str_lit("-O2");
    case SPN_OPT_LEVEL_3: return sp_str_lit("-O3");
    case SPN_OPT_LEVEL_S: return sp_str_lit("-Os");
    case SPN_OPT_LEVEL_Z: return sp_str_lit("-Oz");
    case SPN_OPT_LEVEL_NONE: return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t c_standard_switch(spn_c_standard_t standard) {
  switch (standard) {
    case SPN_C89: return sp_str_lit("-std=c89");
    case SPN_C99: return sp_str_lit("-std=c99");
    case SPN_C11: return sp_str_lit("-std=c11");
    case SPN_C_STANDARD_NONE: return sp_str_lit("-std=c99");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t cxx_standard_switch(spn_cxx_standard_t standard) {
  switch (standard) {
    case SPN_CXX11: return sp_str_lit("-std=c++11");
    case SPN_CXX14: return sp_str_lit("-std=c++14");
    case SPN_CXX17: return sp_str_lit("-std=c++17");
    case SPN_CXX20: return sp_str_lit("-std=c++20");
    case SPN_CXX23: return sp_str_lit("-std=c++23");
    case SPN_CXX_STANDARD_NONE: return sp_str_lit("-std=c++17");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_sanitizer_set_t spn_gcc_supported_sanitizers(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI:
    case SPN_OS_WINDOWS: return 0;
    case SPN_OS_MACOS: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED;
    case SPN_OS_LINUX:
    case SPN_OS_NONE: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
  }
  SP_UNREACHABLE_RETURN(0);
}

spn_sanitizer_set_t spn_clang_supported_sanitizers(spn_triple_t target) {
  switch (target.os) {
    case SPN_OS_WASI: return 0;
    case SPN_OS_WINDOWS: return target.abi == SPN_ABI_MSVC && target.arch == SPN_ARCH_X64 ? SPN_SANITIZER_ADDRESS : 0;
    case SPN_OS_MACOS: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_LEAK;
    case SPN_OS_LINUX:
    case SPN_OS_NONE: return SPN_SANITIZER_ADDRESS | SPN_SANITIZER_THREAD | SPN_SANITIZER_UNDEFINED | SPN_SANITIZER_MEMORY | SPN_SANITIZER_LEAK;
  }
  SP_UNREACHABLE_RETURN(0);
}

// @spader Same hack as the bootstrap CMakeLists: zig cc turns on UBSan traps
// by default in debug builds, and we have UB to clean up before it can stay on
static void zig_default_sanitizer_off(const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (!sp_str_equal_cstr(toolchain->name, "zig")) return;
  if (profile->sanitizers) return;
  sp_da_push(flags->compile, sp_str_lit("-fno-sanitize=undefined"));
  sp_da_push(flags->link, sp_str_lit("-fno-sanitize=undefined"));
}

void spn_gnu_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    sp_da_push(flags->compile, sp_str_lit("-g"));
  }
  push_flag(&flags->compile, opt_switch(profile->opt));
  if (profile->mode == SPN_BUILD_MODE_RELEASE) {
    sp_da_push(flags->compile, render_define_c(mem, "NDEBUG"));
  }
  if (profile->sanitizers) {
    sp_str_t sanitizer = sp_fmt(mem, "-fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers))).value;
    sp_da_push(flags->compile, sanitizer);
    sp_da_push(flags->link, sanitizer);
    sp_da_push(flags->compile, sp_str_lit("-fno-sanitize-recover=all"));
    sp_da_push(flags->compile, sp_str_lit("-fno-omit-frame-pointer"));
  }
}

static void add_launcher(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, spn_lang_t lang, sp_ps_config_t* ps) {
  spn_toolchain_launcher_t launcher = lang == SPN_LANG_CXX ? toolchain->cxx : toolchain->compiler;
  sp_assert(!sp_str_empty(launcher.program));
  ps->command = launcher.program;
  push_args(mem, ps, launcher.args);
  if (toolchain->driver == SPN_CC_DRIVER_CLANG) {
    spn_triple_t triple = { profile->arch, profile->os, profile->abi };
    sp_str_t target = spn_triple_to_cc_target(mem, triple);
    if (!sp_str_empty(target)) {
      push_arg_fmt(mem, ps, "--target={}", sp_fmt_str(target));
    }
  }
}

static void add_include(sp_mem_t mem, sp_ps_config_t* ps, sp_str_t value) {
  sp_ps_config_add_arg(mem, ps, render_include(mem, value));
}

static void add_define(sp_mem_t mem, sp_ps_config_t* ps, sp_str_t value) {
  sp_ps_config_add_arg(mem, ps, render_define(mem, value));
}

void spn_gnu_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, sp_ps_config_t* ps) {
  add_launcher(mem, toolchain, profile, compile->lang, ps);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  zig_default_sanitizer_off(toolchain, profile, &flags);
  if (compile->lang == SPN_LANG_C) {
    sp_ps_config_add_arg(mem, ps, c_standard_switch(profile->standard));
  } else if (compile->lang == SPN_LANG_CXX) {
    sp_ps_config_add_arg(mem, ps, cxx_standard_switch(compile->cxx.standard));
  }
  push_args(mem, ps, flags.compile);
  push_arg(mem, ps, "-c");
  sp_ps_config_add_arg(mem, ps, compile->source);
  sp_da_for(compile->include, it) {
    add_include(mem, ps, compile->include[it]);
  }
  sp_da_for(compile->define, it) {
    add_define(mem, ps, compile->define[it]);
  }
  if (compile->lang == SPN_LANG_CXX) {
    if (compile->cxx.no_exceptions) {
      push_arg(mem, ps, "-fno-exceptions");
    }
    if (compile->cxx.no_rtti) {
      push_arg(mem, ps, "-fno-rtti");
    }
  }
  if (compile->pic) {
    push_arg(mem, ps, "-fPIC");
  }
  if (compile->visibility == SPN_SYMBOL_VISIBILITY_HIDDEN) {
    push_arg(mem, ps, "-fvisibility=hidden");
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!sp_str_empty(profile->sysroot)) {
      push_arg(mem, ps, "-isysroot");
      push_arg_str(mem, ps, profile->sysroot);
    }
    if (spn_os_version_present(compile->min_os)) {
      push_arg_fmt(mem, ps, "-mmacosx-version-min={}.{}", sp_fmt_uint(compile->min_os.major), sp_fmt_uint(compile->min_os.minor));
    }
  }
  push_args(mem, ps, compile->args);
  push_arg(mem, ps, "-Werror=return-type");
  push_arg(mem, ps, "-o");
  sp_ps_config_add_arg(mem, ps, compile->output);
}

void spn_gnu_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, sp_ps_config_t* ps) {
  add_launcher(mem, toolchain, profile, link->lang, ps);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_gnu_render_flags(mem, profile, &flags);
  zig_default_sanitizer_off(toolchain, profile, &flags);
  push_args(mem, ps, flags.link);
  switch (link->kind) {
    case SPN_CC_OUTPUT_REACTOR: {
      push_arg(mem, ps, "-mexec-model=reactor");
      push_arg(mem, ps, "-Wl,--no-entry");
      push_arg(mem, ps, "-Wl,--import-symbols");
      push_arg(mem, ps, "-Wl,--export-dynamic");
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      push_arg(mem, ps, "-shared");
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      if (profile->linkage == SPN_LIB_KIND_STATIC && profile->os != SPN_OS_MACOS) {
        push_arg(mem, ps, "-static");
      }
      if (profile->os == SPN_OS_WINDOWS && link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS) {
        push_arg(mem, ps, "-Wl,--subsystem,windows");
      }
      break;
    }
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  push_args(mem, ps, link->objects);
  push_args(mem, ps, link->args);
  sp_da_for(link->lib_dirs, it) {
    push_arg_fmt(mem, ps, "-L{}", sp_fmt_str(link->lib_dirs[it]));
  }
  push_args(mem, ps, link->libs);
  sp_da_for(link->hidden_libs, it) {
    if (profile->os == SPN_OS_MACOS) {
      push_arg_fmt(mem, ps, "-Wl,-hidden-l{}", sp_fmt_str(link->hidden_libs[it]));
    } else {
      push_arg_fmt(mem, ps, "-l{}", sp_fmt_str(link->hidden_libs[it]));
      spn_triple_t triple = { profile->arch, profile->os, profile->abi };
      sp_str_t archive = spn_triple_lib_file_name(mem, triple, link->hidden_libs[it], SP_OS_LIB_STATIC);
      push_arg_fmt(mem, ps, "-Wl,--exclude-libs,{}", sp_fmt_str(archive));
    }
  }
  sp_da_for(link->system_libs, it) {
    push_arg_fmt(mem, ps, "-l{}", sp_fmt_str(link->system_libs[it]));
  }
  if (profile->os == SPN_OS_MACOS) {
    if (!sp_str_empty(profile->sysroot)) {
      push_arg(mem, ps, "-isysroot");
      sp_ps_config_add_arg(mem, ps, profile->sysroot);
    }
    if (spn_os_version_present(link->min_os)) {
      push_arg_fmt(mem, ps, "-mmacosx-version-min={}.{}", sp_fmt_uint(link->min_os.major), sp_fmt_uint(link->min_os.minor));
    }
    sp_da_for(link->frameworks, it) {
      push_arg(mem, ps, "-framework");
      sp_ps_config_add_arg(mem, ps, link->frameworks[it]);
    }
  }
  sp_da_for(link->rpath, it) {
    push_arg_fmt(mem, ps, "-Wl,-rpath,{}", sp_fmt_str(link->rpath[it]));
  }
  sp_ps_config_add_arg(mem, ps, sp_str_lit("-o"));
  sp_ps_config_add_arg(mem, ps, link->output);
}

void spn_gnu_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_t* archive, sp_ps_config_t* ps) {
  ps->command = toolchain->archiver.program;
  push_args(mem, ps, toolchain->archiver.args);
  sp_ps_config_add_arg(mem, ps, sp_str_lit("rcs"));
  push_args(mem, ps, archive->args);
  sp_ps_config_add_arg(mem, ps, archive->output);
  push_args(mem, ps, archive->objects);
}
