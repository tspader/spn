#include "sp.h"
#include "sp/macro.h"

sp_str_t test_repo_root(sp_mem_t mem) {
  sp_str_t path = sp_fs_get_exe_path(mem);
  while (true) {
    sp_assert(!sp_str_empty(path));
    if (sp_fs_exists(sp_fs_join_path(mem, path, strl("spn.toml")))) {
      return path;
    }
    path = sp_fs_parent_path(path);
  }
}

sp_str_t test_repo_path(sp_mem_t mem, sp_str_t rel) {
  return sp_fs_join_path(mem, test_repo_root(mem), rel);
}
