#ifndef SPN_INDEX_RELEASE_H
#define SPN_INDEX_RELEASE_H

#include "error/types.h"
#include "index/types.h"

spn_err_union_t spn_index_release_from_pkg(sp_mem_t mem, spn_pkg_info_t* info, spn_index_release_t* release);

#endif
