#ifndef SPN_GRAPH_BUILD_H
#define SPN_GRAPH_BUILD_H

#include "unit/types.h"

spn_path_t spn_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);
spn_path_t spn_target_unit_staged_path(sp_mem_t mem, spn_target_unit_t* unit);
spn_path_t spn_target_exports_path(sp_mem_t mem, spn_target_unit_t* unit);
spn_path_t spn_target_exports_archive(sp_mem_t mem, spn_path_t exports);

#endif
