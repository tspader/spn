#ifndef SPN_INDEX_RELEASE_H
#define SPN_INDEX_RELEASE_H

#include "sp.h"
#include "spn/core.h"
#include "index/types.h"

spn_err_t spn_index_release_from_pkg(sp_mem_t mem, spn_pkg_info_t* info, spn_index_release_t* release, sp_str_t* dep);

#endif
