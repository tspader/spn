#ifndef SPN_API_TYPES_H
#define SPN_API_TYPES_H

#include "sp.h"
#include "spn.h"
#include "unit/types.h"

struct spn_node {
  spn_node_ref_t ref;
};

struct spn_target {
  spn_pkg_unit_t* unit;
  spn_target_info_t* info;
};

#endif
