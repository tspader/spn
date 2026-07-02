#include "toolchain/launcher.h"

sp_str_t spn_toolchain_launcher_to_str(sp_mem_t mem, spn_toolchain_launcher_t launcher) {
  if (sp_da_empty(launcher.args)) {
    return sp_str_copy(mem, launcher.program);
  }

  sp_io_dyn_mem_writer_t w;
  sp_io_dyn_mem_writer_init(mem, &w);
  sp_io_write_str(&w.base, launcher.program, SP_NULLPTR);
  sp_da_for(launcher.args, i) {
    sp_io_write_c8(&w.base, ' ');
    sp_io_write_str(&w.base, launcher.args[i], SP_NULLPTR);
  }
  return sp_io_dyn_mem_writer_take_str(&w);
}
