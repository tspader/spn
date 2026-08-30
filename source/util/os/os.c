#include "os/os.h"
#include "str/str.h"

#if defined(SP_WIN32)
sp_str_t sp_fs_get_home_path(sp_mem_t mem) {
  sp_str_t drive = sp_os_env_get(sp_str_lit("HOMEDRIVE"));
  SP_ASSERT(!sp_str_empty(drive));
  sp_str_t path = sp_os_env_get(sp_str_lit("HOMEPATH"));
  SP_ASSERT(!sp_str_empty(path));
  return sp_str_concat(mem, drive, sp_fs_normalize_path(mem, path));
}
#else
sp_str_t sp_fs_get_home_path(sp_mem_t mem) {
  sp_str_t path = sp_os_env_get(sp_str_lit("HOME"));
  SP_ASSERT(!sp_str_empty(path));
  return sp_fs_normalize_path(mem, path);
}
#endif

sp_str_t sp_env_get_path(sp_env_t* env) {
  sp_str_t exact = sp_env_get(env, sp_str_lit("PATH"));
  if (!sp_str_empty(exact)) {
    return exact;
  }
  if (!env->vars) {
    return sp_str_lit("");
  }
  sp_ht_for_kv(env->vars, it) {
    if (sp_str_iequal(*it.key, sp_str_lit("path"))) {
      return *it.val;
    }
  }
  return sp_str_lit("");
}

sp_err_t sp_fs_remove(sp_str_t path) {
  if (sp_fs_is_symlink(path)) {
    return sp_fs_remove_file(path);
  }
  if (sp_fs_is_dir(path)) {
    return sp_fs_remove_dir(path);
  }
  if (sp_fs_exists(path)) {
    return sp_fs_remove_file(path);
  }
  return SP_OK;
}
