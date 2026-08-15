#include "compiler/driver.h"
#include "compiler/push.h"

#include "enum/enum.h"
#include "paths/paths.h"
#include "sp/macro.h"

spn_sanitizer_set_t spn_msvc_supported_sanitizers(spn_triple_t target) {
  return target.os == SPN_OS_WINDOWS && target.abi == SPN_ABI_MSVC && target.arch == SPN_ARCH_X64 ? SPN_SANITIZER_ADDRESS : 0;
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

static void add_launcher(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, spn_lang_t lang, spn_invocation_t* invocation) {
  spn_toolchain_launcher_t launcher = lang == SPN_LANG_CXX ? toolchain->cxx : toolchain->compiler;
  sp_assert(!spn_arg_empty(launcher.program));
  invocation->program = launcher.program;
  spn_cc_push_strs(mem, invocation, launcher.args);
  spn_cc_push_c(mem, invocation, "/nologo");
}

void spn_msvc_render_compile(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_compile_t* compile, spn_invocation_t* invocation) {
  if (compile->lang == SPN_LANG_ASM) {
    // cl neither assembles nor errors on assembly sources; it warns and
    // exits zero, so these must go to MASM directly
    invocation->program = spn_arg_lit(sp_str_lit("ml64"));
    spn_cc_push_c(mem, invocation, "/nologo");
    spn_cc_push_c(mem, invocation, "/c");
    return;
  }
  add_launcher(mem, toolchain, compile->lang, invocation);
  // cl reads sources in the system ANSI codepage by default; non-ASCII
  // string literals are mangled without this
  spn_cc_push_c(mem, invocation, "/utf-8");
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_msvc_render_flags(mem, profile, &flags);
  sp_str_t standard = compile->lang == SPN_LANG_CXX ?
    cxx_standard_switch(compile->cxx.standard) :
    c_standard_switch(profile->standard);
  if (!sp_str_empty(standard)) {
    spn_cc_push_str(mem, invocation, standard);
  }
  spn_cc_push_strs(mem, invocation, flags.compile);
  spn_cc_push_c(mem, invocation, "/c");
  sp_da_for(compile->include, it) {
    spn_cc_push_glued(mem, invocation, "/I", compile->include[it]);
  }
  sp_da_for(compile->define, it) {
    spn_cc_push_fmt(mem, invocation, "/D{}", sp_fmt_str(compile->define[it]));
  }
  if (compile->lang == SPN_LANG_CXX) {
    if (!compile->cxx.no_exceptions) {
      spn_cc_push_c(mem, invocation, "/EHsc");
    }
    if (compile->cxx.no_rtti) {
      spn_cc_push_c(mem, invocation, "/GR-");
    }
  }
  // PIC and symbol visibility have no cl equivalents; code is always
  // relocatable and symbols are hidden unless exported
  spn_cc_push_strs(mem, invocation, compile->args);
  // Parity with -Werror=return-type: C4715 is "not all control paths
  // return a value"
  spn_cc_push_c(mem, invocation, "/we4715");
}

void spn_msvc_render_compile_files(sp_mem_t mem, const spn_cc_compile_files_t* files, spn_invocation_t* invocation) {
  sp_assert(spn_path_empty(files->depfile));
  spn_cc_push_glued(mem, invocation, "/Fo", files->output);
  spn_cc_push_path(mem, invocation, files->source);
}

void spn_msvc_render_link(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_profile_info_t* profile, const spn_cc_link_t* link, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  add_launcher(mem, toolchain, link->lang, invocation);
  spn_cc_flags_t flags = sp_zero;
  sp_da_init(mem, flags.compile);
  sp_da_init(mem, flags.link);
  spn_msvc_render_flags(mem, profile, &flags);
  spn_cc_push_strs(mem, invocation, flags.link);
  switch (link->kind) {
    case SPN_CC_OUTPUT_SHARED_LIB: {
      spn_cc_push_c(mem, invocation, "/LD");
      break;
    }
    case SPN_CC_OUTPUT_EXE: {
      break;
    }
    case SPN_CC_OUTPUT_REACTOR:
    case SPN_CC_OUTPUT_OBJECT:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_unreachable_case();
    }
  }
  spn_cc_push_paths(mem, invocation, files->objects);

  sp_da_for(link->private_libs, it) {
    spn_cc_push_fmt(mem, invocation, "{}.lib", sp_fmt_str(link->private_libs[it]));
  }
  sp_da_for(link->libs, it) {
    spn_cc_push_fmt(mem, invocation, "{}.lib", sp_fmt_str(link->libs[it]));
  }
  sp_da_for(link->system_libs, it) {
    spn_cc_push_fmt(mem, invocation, "{}.lib", sp_fmt_str(link->system_libs[it]));
  }
  spn_cc_push_glued(mem, invocation, "/Fe", files->output);

  // Everything past /link goes to link.exe verbatim
  sp_da(spn_arg_t) linker = sp_da_new(mem, spn_arg_t);
  if (profile->mode == SPN_BUILD_MODE_DEBUG) {
    sp_da_push(linker, spn_arg_lit(sp_str_lit("/DEBUG")));
  }
  if (!spn_path_empty(files->exports.path)) {
    sp_da_push(linker, spn_arg_glue(sp_str_lit("/DEF:"), files->exports.path));
  }
  sp_da_for(files->whole_archives, it) {
    sp_da_push(linker, spn_arg_glue(sp_str_lit("/WHOLEARCHIVE:"), files->whole_archives[it]));
  }
  sp_da_for(link->lib_dirs, it) {
    sp_da_push(linker, spn_arg_glue(sp_str_lit("/LIBPATH:"), link->lib_dirs[it]));
  }
  if (link->kind == SPN_CC_OUTPUT_EXE && link->subsystem == SPN_WIN_SUBSYSTEM_WINDOWS) {
    sp_da_push(linker, spn_arg_lit(sp_str_lit("/SUBSYSTEM:WINDOWS")));
  }

  if (!sp_da_empty(linker)) {
    spn_cc_push_c(mem, invocation, "/link");
    spn_cc_push_args(mem, invocation, linker);
  }
}

void spn_msvc_render_archive(sp_mem_t mem, const spn_cc_toolchain_t* toolchain, const spn_cc_archive_files_t* files, spn_invocation_t* invocation) {
  invocation->program = toolchain->archiver.program;
  spn_cc_push_strs(mem, invocation, toolchain->archiver.args);
  spn_cc_push_c(mem, invocation, "/nologo");
  spn_cc_push_glued(mem, invocation, "/OUT:", files->output);
  spn_cc_push_paths(mem, invocation, files->objects);
}
