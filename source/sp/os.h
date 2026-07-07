#ifndef SPN_SP_OS_H
#define SPN_SP_OS_H

#include "sp.h"

sp_str_t sp_fs_get_home_path(sp_mem_t mem);
sp_str_t sp_fs_get_bin_path(sp_mem_t mem);
sp_err_t sp_fs_remove(sp_str_t path);

#endif
