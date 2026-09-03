#ifndef SPN_TOOLCHAIN_SELECT_H
#define SPN_TOOLCHAIN_SELECT_H

#include "toolchain/types.h"

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_selection_t* selection);
spn_err_t spn_toolchain_incomplete(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query);

#endif
