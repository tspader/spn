#include "sp.h"
#include "sp/macro.h"
#include "toolchain/toolchain.h"

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, sp_str_t root) {
  if (sp_str_empty(root)) return launcher;

  spn_toolchain_launcher_t result = launcher;
  result.program = sp_fs_join_path(mem, root, launcher.program);
#if defined(SP_WIN32)
  result.program = sp_fmt(mem, "{}.exe", sp_fmt_str(result.program)).value;
#endif
  return result;
}

bool spn_toolchain_has_cxx(spn_toolchain_t* toolchain) {
  return !sp_str_empty(toolchain->cxx.program);
}

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
