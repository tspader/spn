#include "toolchain/search.h"

#include "paths/paths.h"

sp_da(sp_str_t) spn_search_split_path(sp_mem_t mem, sp_str_t path) {
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) parts = sp_str_split_c8(mem, path, SPN_SEARCH_PATH_SEP);
  sp_da_for(parts, it) {
    if (sp_str_empty(parts[it])) {
      continue;
    }
    sp_str_t dir = sp_fs_normalize_path(mem, parts[it]);
    if (sp_fs_is_absolute(dir) && spn_path_normal(dir)) {
      sp_da_push(dirs, dir);
    }
  }
  return dirs;
}

sp_str_t spn_search_file(sp_mem_t mem, sp_str_t path) {
  if (sp_fs_is_target_file(path)) {
    return path;
  }
#if defined(SP_WIN32)
  sp_str_t exe = sp_fmt(mem, "{}.exe", sp_fmt_str(path)).value;
  if (sp_fs_is_target_file(exe)) {
    return exe;
  }
#endif
  return sp_str_lit("");
}

sp_str_t spn_search_program(sp_mem_t mem, sp_str_t name, sp_da(sp_str_t) dirs) {
  sp_da_for(dirs, it) {
    sp_str_t found = spn_search_file(mem, sp_fs_join_path(mem, dirs[it], name));
    if (!sp_str_empty(found)) {
      return found;
    }
  }
  return sp_str_lit("");
}
