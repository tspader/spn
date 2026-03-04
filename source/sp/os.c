#include "os.h"

sp_str_t sp_os_get_bin_path() {
  sp_str_t path = sp_os_env_get(sp_str_lit("HOME"));
  SP_ASSERT(!sp_str_empty(path));

  return sp_fs_join_path(path, sp_str_lit(".local/bin"));
}
