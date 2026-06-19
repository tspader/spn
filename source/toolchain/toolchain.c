#include "toolchain/toolchain.h"

sp_str_t spn_toolchain_resolve_path(spn_toolchain_info_t* toolchain, sp_str_t path) {
  if (sp_str_empty(toolchain->sysroot)) {
    return sp_str_copy(spn_mem_todo, path);
  }

  return sp_fs_join_path(spn_mem_todo, toolchain->sysroot, path);
}

spn_toolchain_launcher_t spn_toolchain_get_linker_driver(spn_toolchain_info_t* toolchain) {
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_CLANG:
    case SPN_CC_DRIVER_GCC: return toolchain->compiler;
    case SPN_CC_DRIVER_MSVC: return toolchain->linker;
    case SPN_CC_DRIVER_NONE: return toolchain->linker;
  }

  spn_toolchain_launcher_t bad = sp_zero_initialize();
  sp_unreachable_return(bad);
}

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(spn_toolchain_launcher_t launcher, sp_str_t root) {
  spn_toolchain_launcher_t result = launcher;
  result.program = sp_fs_join_path(spn_mem_todo, root, launcher.program);
  return result;
}

sp_str_t spn_toolchain_launcher_to_str(spn_toolchain_launcher_t launcher) {
  if (sp_da_empty(launcher.args)) return launcher.program;

  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, launcher.program);
  sp_da_for(launcher.args, i) {
    sp_str_builder_append_c8(&b, ' ');
    sp_str_builder_append(&b, launcher.args[i]);
  }
  return sp_str_builder_to_str(&b);
}

