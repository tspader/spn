#ifndef SPN_INDEX_INDEX_H
#define SPN_INDEX_INDEX_H

#include "error/types.h"
#include "index/types.h"

void             spn_index_init(spn_index_info_t* index, sp_mem_t mem);
void             spn_index_deinit(spn_index_info_t* index);
spn_err_t        spn_index_sync(spn_index_info_t* index, bool force);
bool             spn_index_needs_fetch(spn_index_info_t* index);
spn_index_pkg_t* spn_index_get_package(spn_index_info_t* index, spn_pkg_name_t pkg);
spn_index_rel_t* spn_index_get_release(spn_index_info_t* index, spn_pkg_name_t pkg, spn_semver_t version);
spn_err_union_t  spn_index_publish(spn_index_info_t* index, spn_index_rel_t* rel);

#endif
