#ifndef SPN_TOOLCHAIN_SELECT_H
#define SPN_TOOLCHAIN_SELECT_H

#include "error/types.h"
#include "toolchain/types.h"

typedef struct {
  sp_str_t name;
  spn_triple_t target;
  spn_triple_t host;
  spn_toolchain_role_t role;
} spn_toolchain_query_t;

typedef struct {
  spn_toolchain_info_t* info;
  spn_opt_artifact_t artifact;
} spn_toolchain_resolution_t;

bool            spn_toolchain_supports(spn_toolchain_info_t* toolchain, spn_triple_t target, spn_triple_t host);
spn_err_union_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, spn_toolchain_resolution_t* resolution);

#endif
