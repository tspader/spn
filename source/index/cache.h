#ifndef SPN_INDEX_CACHE_H
#define SPN_INDEX_CACHE_H

#include "error/types.h"
#include "index/types.h"

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, sp_da(spn_index_info_t)* indexes);
spn_err_union_t spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t id, spn_index_pkg_t** pkg);

#endif
