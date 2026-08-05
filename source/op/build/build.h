#ifndef SPN_OP_BUILD_BUILD_H
#define SPN_OP_BUILD_BUILD_H

#include "unit/types.h"

sp_str_t spn_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t spn_target_staged_path(sp_mem_t mem, spn_target_unit_t* unit);

#endif
