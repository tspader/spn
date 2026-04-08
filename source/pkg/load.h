#ifndef SPN_PKG_LOAD_H
#define SPN_PKG_LOAD_H

#include "spn.h"
#include "error/types.h"
#include "forward/types.h"

spn_err_union_t spn_pkg_load(spn_pkg_info_t* pkg, sp_str_t manifest_path);
spn_err_union_t spn_index_load(toml_table_t* toml, sp_str_t parent, u32 index, spn_index_info_t* result);

#endif
