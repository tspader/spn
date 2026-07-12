#ifndef SPN_TASK_BUILD_TARGET_H
#define SPN_TASK_BUILD_TARGET_H

#include "graph/types.h"
#include "unit/types.h"

spn_err_t spn_build_add_object_nodes(spn_build_graph_t* graph, spn_target_unit_t* target);
spn_err_t spn_build_add_target_nodes(spn_build_graph_t* graph, spn_target_unit_t* target);

#endif
