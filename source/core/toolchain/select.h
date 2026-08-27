#ifndef SPN_TOOLCHAIN_SELECT_H
#define SPN_TOOLCHAIN_SELECT_H

#include "sp.h"
#include "spn/core.h"
#include "toolchain/types.h"

typedef struct {
  sp_str_t name;
  spn_triple_t target;
  spn_triple_t host;
  spn_toolchain_role_t role;
  bool shared;
} spn_toolchain_query_t;

typedef struct {
  spn_toolchain_info_t* info;
  spn_opt_artifact_t artifact;
  spn_triple_t triple;
} spn_toolchain_resolution_t;

spn_err_t spn_toolchain_select(spn_toolchain_catalog_t* catalog, spn_toolchain_query_t query, sp_mem_t mem, spn_toolchain_resolution_t* resolution);

#endif
