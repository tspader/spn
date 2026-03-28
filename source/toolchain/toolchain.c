#include "toolchain/toolchain.h"

sp_str_t spn_toolchain_resolve_path(spn_toolchain_info_t* toolchain, sp_str_t path) {
  if (sp_str_empty(toolchain->sysroot)) {
    return sp_str_copy(path);
  }

  return sp_fs_join_path(toolchain->sysroot, path);
}

sp_str_t spn_toolchain_get_linker_driver(spn_toolchain_info_t* toolchain) {
  switch (toolchain->driver) {
    case SPN_CC_DRIVER_GCC: return toolchain->compiler;
    case SPN_CC_DRIVER_MSVC: return toolchain->linker;
    case SPN_CC_DRIVER_NONE: return toolchain->linker;
  }
}

