#ifndef SPN_INDEX_JSONL_H
#define SPN_INDEX_JSONL_H

#include "error/types.h"
#include "index/types.h"

sp_str_t spn_index_jsonl_path(sp_mem_t mem, spn_index_info_t* index, spn_pkg_name_t id);
spn_err_t spn_index_jsonl_get_package(spn_index_info_t* index, sp_mem_t mem, spn_pkg_name_t id, spn_index_pkg_t** pkg, spn_index_diag_t* diag);

#endif
