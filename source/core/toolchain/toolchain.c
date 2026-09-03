#include "sp.h"
#include "macro/macro.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, spn_path_t root) {
  sp_str_t name = launcher.program.prefix;
#if defined(SP_WIN32)
  name = sp_fmt(mem, "{}.exe", sp_fmt_str(name)).value;
#endif

  spn_toolchain_launcher_t result = launcher;
  result.program = spn_arg_path(spn_path_join(mem, root, name));
  return result;
}

bool spn_toolchain_has_cxx(spn_toolchain_info_t* toolchain) {
  return !spn_arg_empty(toolchain->cxx.program);
}

spn_toolchain_ref_t spn_toolchain_ref_from_str(sp_str_t str) {
  if (sp_str_empty(str)) {
    return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_NONE };
  }
  if (sp_str_equal_cstr(str, "auto")) {
    return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_AUTO };
  }
  return (spn_toolchain_ref_t) { .kind = SPN_TOOLCHAIN_REF_NAMED, .name = str };
}

spn_toolchain_source_t spn_toolchain_source(sp_da(spn_toolchain_host_t) hosts) {
  bool local = false;
  bool distributed = false;
  sp_da_for(hosts, it) {
    if (sp_str_empty(hosts[it].artifact.url)) {
      local = true;
    }
    else {
      distributed = true;
    }
  }
  if (local && distributed) {
    return SPN_TOOLCHAIN_SOURCE_MIXED;
  }
  return distributed ? SPN_TOOLCHAIN_SOURCE_DISTRIBUTION : SPN_TOOLCHAIN_SOURCE_LOCAL;
}

spn_cc_cap_set_t spn_toolchain_driver_caps(spn_cc_driver_t driver) {
  switch (driver) {
    case SPN_CC_DRIVER_GCC: return SPN_CC_CAP_EXCLUDE_LIBS | SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FREESTANDING | SPN_CC_CAP_LINKER_SCRIPT;
    case SPN_CC_DRIVER_CLANG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_LLVM_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_EXCLUDE_LIBS | SPN_CC_CAP_NOLIBC | SPN_CC_CAP_FREESTANDING | SPN_CC_CAP_LINKER_SCRIPT;
    case SPN_CC_DRIVER_ZIG: return SPN_CC_CAP_TARGET_TRIPLE | SPN_CC_CAP_CLANG_FRONTEND | SPN_CC_CAP_FREESTANDING | SPN_CC_CAP_LINKER_SCRIPT;
    case SPN_CC_DRIVER_MSVC: return 0;
    case SPN_CC_DRIVER_NONE: sp_unreachable_case();
  }
  SP_UNREACHABLE_RETURN(0);
}

bool spn_toolchain_driver_reaches(spn_cc_driver_t driver, spn_triple_t target) {
  if (target.os == SPN_OS_FREESTANDING) {
    return spn_toolchain_driver_caps(driver) & SPN_CC_CAP_FREESTANDING;
  }
  return true;
}

sp_str_t spn_toolchain_launcher_to_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_toolchain_launcher_t launcher) {
  sp_str_t program = spn_arg_str(roots, mem, launcher.program);
  if (sp_da_empty(launcher.args)) {
    return program;
  }

  sp_io_dyn_mem_writer_t w;
  sp_io_dyn_mem_writer_init(mem, &w);
  sp_io_write_str(&w.base, program, SP_NULLPTR);
  sp_da_for(launcher.args, i) {
    sp_io_write_c8(&w.base, ' ');
    sp_io_write_str(&w.base, launcher.args[i], SP_NULLPTR);
  }
  return sp_io_dyn_mem_writer_take_str(&w);
}
