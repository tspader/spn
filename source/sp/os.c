#include "os.h"

#if defined(SP_WIN32)
sp_str_t sp_fs_get_home_path() {
  sp_str_t drive = sp_os_env_get(sp_str_lit("HOMEDRIVE"));
  SP_ASSERT(!sp_str_empty(drive));
  sp_str_t path = sp_os_env_get(sp_str_lit("HOMEPATH"));
  SP_ASSERT(!sp_str_empty(path));
  return sp_str_concat(drive, sp_fs_normalize_path(path));
}
#else
sp_str_t sp_fs_get_home_path() {
  sp_str_t path = sp_os_env_get(sp_str_lit("HOME"));
  SP_ASSERT(!sp_str_empty(path));
  return sp_fs_normalize_path(path);
}
#endif

sp_str_t sp_fs_get_bin_path() {
  sp_str_t home = sp_fs_get_home_path();
  return sp_fs_join_path(home, sp_str_lit(".local/bin"));
}
