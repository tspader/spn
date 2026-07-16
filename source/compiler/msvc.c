#include "compiler/driver.h"

#include "enum/enum.h"
#include "sp/macro.h"

spn_sanitizer_set_t spn_msvc_supported_sanitizers(spn_triple_t target) {
  return target.os == SPN_OS_WINDOWS && target.abi == SPN_ABI_MSVC && target.arch == SPN_ARCH_X64 ? SPN_SANITIZER_ADDRESS : 0;
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

static void push_args(sp_mem_t mem, sp_ps_config_t* ps, sp_da(sp_str_t) args) {
  sp_da_for(args, it) {
    sp_ps_config_add_arg(mem, ps, args[it]);
  }
}

static sp_str_t opt_switch(spn_opt_level_t level) {
  switch (level) {
    case SPN_OPT_LEVEL_0: return sp_str_lit("/Od");
    case SPN_OPT_LEVEL_1: return sp_str_lit("/O1");
    case SPN_OPT_LEVEL_2:
    case SPN_OPT_LEVEL_3: return sp_str_lit("/O2");
    case SPN_OPT_LEVEL_S:
    case SPN_OPT_LEVEL_Z: return sp_str_lit("/O1");
    case SPN_OPT_LEVEL_NONE: return sp_str_lit("");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t c_standard_switch(spn_c_standard_t standard) {
  switch (standard) {
    // cl has no c89/c99 modes; its default is the closest it gets
    case SPN_C89:
    case SPN_C99:
    case SPN_C_STANDARD_NONE: return sp_str_lit("");
    case SPN_C11: return sp_str_lit("/std:c11");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

static sp_str_t cxx_standard_switch(spn_cxx_standard_t standard) {
  switch (standard) {
    // cl bottoms out at c++14
    case SPN_CXX11:
    case SPN_CXX14: return sp_str_lit("/std:c++14");
    case SPN_CXX17: return sp_str_lit("/std:c++17");
    case SPN_CXX20: return sp_str_lit("/std:c++20");
    case SPN_CXX23: return sp_str_lit("/std:c++latest");
    case SPN_CXX_STANDARD_NONE: return sp_str_lit("/std:c++17");
  }
  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

void spn_msvc_render_flags(sp_mem_t mem, const spn_profile_info_t* profile, spn_cc_flags_t* flags) {
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    // /Z7 embeds debug info in the object; /Zi would funnel every parallel
    // cl in a package's work directory into one vc140.pdb (C1041)
    sp_da_push(flags->compile, sp_str_lit("/Z7"));
  }
  sp_str_t opt = opt_switch(profile->opt);
  if (!sp_str_empty(opt)) {
    sp_da_push(flags->compile, opt);
  }
  if (profile->mode == SPN_BUILD_MODE_RELEASE) {
    sp_da_push(flags->compile, sp_str_lit("/DNDEBUG"));
  }
  if (profile->sanitizers) {
    sp_str_t sanitizer = sp_fmt(mem, "/fsanitize={}", sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers))).value;
    sp_da_push(flags->compile, sanitizer);
    sp_da_push(flags->link, sanitizer);
  }
}

static void add_launcher(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, spn_lang_t lang, sp_ps_config_t* ps) {
  spn_toolchain_launcher_t launcher = lang == SPN_LANG_CXX ? toolchain->cxx : toolchain->compiler;
  sp_assert(!sp_str_empty(launcher.program));
  ps->command = launcher.program;
  push_args(mem, ps, launcher.args);
  push_arg(mem, ps, "/nologo");
}

void spn_msvc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, sp_ps_config_t* ps) {
  add_launcher(mem, toolchain, compile->lang, ps);
  // cl reads sources in the system ANSI codepage by default; non-ASCII
  // string literals are mangled without this
  push_arg(mem, ps, "/utf-8");
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_msvc_render_flags(mem, profile, &flags);
  sp_str_t standard = compile->lang == SPN_LANG_CXX ?
    cxx_standard_switch(compile->cxx.standard) :
    c_standard_switch(profile->standard);
  if (!sp_str_empty(standard)) {
    sp_ps_config_add_arg(mem, ps, standard);
  }
  push_args(mem, ps, flags.compile);
  push_arg(mem, ps, "/c");
  sp_ps_config_add_arg(mem, ps, compile->source);
  sp_da_for(compile->include, it) {
    push_arg_fmt(mem, ps, "/I{}", sp_fmt_str(compile->include[it]));
  }
  sp_da_for(compile->define, it) {
    push_arg_fmt(mem, ps, "/D{}", sp_fmt_str(compile->define[it]));
  }
  if (compile->lang == SPN_LANG_CXX) {
    if (!compile->cxx.no_exceptions) {
      push_arg(mem, ps, "/EHsc");
    }
    if (compile->cxx.no_rtti) {
      push_arg(mem, ps, "/GR-");
    }
  }
  // PIC and symbol visibility have no cl equivalents; code is always
  // relocatable and symbols are hidden unless exported
  push_args(mem, ps, compile->args);
  // Parity with -Werror=return-type: C4715 is "not all control paths
  // return a value"
  push_arg(mem, ps, "/we4715");
  push_arg_fmt(mem, ps, "/Fo{}", sp_fmt_str(compile->output));
}

void spn_msvc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, sp_ps_config_t* ps) {
  add_launcher(mem, toolchain, link->lang, ps);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_msvc_render_flags(mem, profile, &flags);
  push_args(mem, ps, flags.link);
  switch (link->kind) {
    case SPN_CC_OUTPUT_SHARED_LIB: {
      push_arg(mem, ps, "/LD");
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      // Static linkage means the CRT choice at compile time on MSVC; there
      // is nothing to render on the link line
      break;
    }
    case SPN_CC_OUTPUT_REACTOR:
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  push_args(mem, ps, link->objects);
  push_args(mem, ps, link->args);
  push_args(mem, ps, link->libs);
  // cl has no --exclude-libs; hidden libs link like any other
  sp_da_for(link->hidden_libs, it) {
    push_arg_fmt(mem, ps, "{}.lib", sp_fmt_str(link->hidden_libs[it]));
  }
  sp_da_for(link->system_libs, it) {
    push_arg_fmt(mem, ps, "{}.lib", sp_fmt_str(link->system_libs[it]));
  }
  push_arg_fmt(mem, ps, "/Fe{}", sp_fmt_str(link->output));

  // Everything past /link goes to link.exe verbatim
  sp_da(sp_str_t) linker = sp_da_new(mem, sp_str_t);
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    sp_da_push(linker, sp_str_lit("/DEBUG"));
  }
  sp_da_for(link->lib_dirs, it) {
    sp_da_push(linker, sp_fmt(mem, "/LIBPATH:{}", sp_fmt_str(link->lib_dirs[it])).value);
  }
  if (link->kind == SPN_CC_OUTPUT_EXE && link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS) {
    sp_da_push(linker, sp_str_lit("/SUBSYSTEM:WINDOWS"));
  }
  // rpath does not exist on Windows; DLLs resolve from the exe's directory
  if (!sp_da_empty(linker)) {
    push_arg(mem, ps, "/link");
    push_args(mem, ps, linker);
  }
}

void spn_msvc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_t* archive, sp_ps_config_t* ps) {
  ps->command = toolchain->archiver.program;
  push_args(mem, ps, toolchain->archiver.args);
  push_arg(mem, ps, "/nologo");
  push_args(mem, ps, archive->args);
  push_arg_fmt(mem, ps, "/OUT:{}", sp_fmt_str(archive->output));
  push_args(mem, ps, archive->objects);
}
