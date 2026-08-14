#ifndef SPN_INDEX_H
#define SPN_INDEX_H

#include "sp.h"
#include "spn/core.h"
#include "index/types.h"

#define SPN_INDEX_DEFAULT_REFRESH 600

spn_err_t spn_index_sync(spn_index_info_t* index, bool force);
bool      spn_index_needs_fetch(spn_index_info_t* index);
sp_str_t  spn_index_location(spn_index_info_t* index, sp_mem_t mem, sp_str_t root);
void      spn_index_assemble(sp_mem_t mem, spn_index_map_t* workspace, sp_da(spn_index_info_t) user, sp_da(spn_index_info_t)* indexes);
spn_err_t spn_index_get_package(spn_index_info_t* index, sp_mem_t mem, sp_intern_t* intern, spn_pkg_name_t id, spn_index_pkg_t** pkg, spn_index_diag_t* diag);
spn_err_t spn_index_publish(spn_index_info_t* index, sp_mem_t mem, spn_index_release_t* rel);
sp_str_t  spn_index_source(spn_index_info_t* index);
sp_str_t  spn_index_publish_target(spn_index_info_t* index);

#endif
