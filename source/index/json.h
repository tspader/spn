#ifndef SPN_INDEX_JSON_H
#define SPN_INDEX_JSON_H

#include "error/types.h"
#include "index/types.h"

spn_err_t spn_index_parse_pkg(sp_mem_t mem, spn_pkg_name_t id, sp_str_t blob, spn_index_pkg_t* pkg);
sp_str_t spn_index_release_to_json(sp_mem_t mem, spn_index_release_t* rel);

#endif
