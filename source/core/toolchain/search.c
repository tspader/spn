#include "toolchain/search.h"

sp_da(sp_str_t) spn_search_split_path(sp_mem_t mem, sp_str_t path) {
  sp_da(sp_str_t) dirs = sp_da_new(mem, sp_str_t);
  sp_da(sp_str_t) parts = sp_str_split_c8(mem, path, SPN_SEARCH_PATH_SEP);
  sp_da_for(parts, it) {
    if (!sp_str_empty(parts[it])) {
      sp_da_push(dirs, parts[it]);
    }
  }
  return dirs;
}

static sp_str_t rooted(sp_mem_t mem, sp_str_t cwd, sp_str_t path) {
  if (sp_fs_is_absolute(path)) {
    return path;
  }
  return sp_fs_join_path(mem, cwd, path);
}

static sp_str_t search_file(sp_mem_t mem, sp_str_t candidate) {
  if (sp_fs_is_target_file(candidate)) {
    return candidate;
  }
#if defined(SP_WIN32)
  sp_str_t exe = sp_fmt(mem, "{}.exe", sp_fmt_str(candidate)).value;
  if (sp_fs_is_target_file(exe)) {
    return exe;
  }
#endif
  return sp_str_lit("");
}

static bool is_pathless(sp_str_t program) {
  sp_for(it, program.len) {
    if (sp_fs_is_sep(program.data[it])) {
      return false;
    }
  }
  return true;
}

sp_str_t spn_search_program(sp_mem_t mem, sp_str_t cwd, sp_str_t program, sp_da(sp_str_t) dirs) {
  if (!is_pathless(program)) {
    return search_file(mem, rooted(mem, cwd, program));
  }
  sp_da_for(dirs, it) {
    sp_str_t resolved = search_file(mem, rooted(mem, cwd, sp_fs_join_path(mem, dirs[it], program)));
    if (!sp_str_empty(resolved)) {
      return resolved;
    }
  }
  return sp_str_lit("");
}
