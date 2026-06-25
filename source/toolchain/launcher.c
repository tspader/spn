#include "toolchain/launcher.h"

sp_str_t spn_toolchain_launcher_to_str(spn_toolchain_launcher_t launcher) {
  if (sp_da_empty(launcher.args)) {
    return launcher.program;
  }

  sp_str_builder_t b = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&b, launcher.program);
  sp_da_for(launcher.args, i) {
    sp_str_builder_append_c8(&b, ' ');
    sp_str_builder_append(&b, launcher.args[i]);
  }
  return sp_str_builder_to_str(&b);
}
