#include "sp.h"
#include "macro/macro.h"
#include "paths/paths.h"
#include "toolchain/toolchain.h"

spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, spn_path_t root) {
  if (spn_path_empty(root) || spn_arg_empty(launcher.program)) return launcher;

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
