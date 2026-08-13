#ifndef SPN_INDEX_DIR_H
#define SPN_INDEX_DIR_H

#include "error/types.h"
#include "index/types.h"

spn_err_t spn_index_dir_get_package(spn_index_info_t* index, sp_mem_t mem, sp_intern_t* intern, spn_pkg_name_t id, spn_index_pkg_t** pkg, spn_index_diag_t* diag);

#endif
