#ifndef SPN_API_TYPES_H
#define SPN_API_TYPES_H

#include "sp.h"
#include "spn.h"

#include "core/types.h"

struct spn_target {
  spn_t* spn;
  spn_target_info_t* info;
};

#endif

