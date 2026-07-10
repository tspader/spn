#ifndef SPN_INDEX_CACHE_H
#define SPN_INDEX_CACHE_H

#include "index/types.h"

void spn_index_cache_init(spn_index_cache_t* cache, sp_mem_t mem, sp_intern_t* intern, spn_index_arr_t* indexes);
spn_index_pkg_t* spn_index_cache_get_package(spn_index_cache_t* cache, spn_pkg_name_t pkg);
spn_index_release_t* spn_index_cache_get_release(spn_index_cache_t* cache, spn_pkg_name_t id, spn_semver_t version);

#endif
