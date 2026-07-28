#ifndef SPN_GRAPH_TYPES_H
#define SPN_GRAPH_TYPES_H

#include "sp/sp_graph.h"

typedef struct {
  spn_build_graph_t graph;
  spn_bg_dirty_t *dirty;
  spn_bg_executor_t *executor;
} spn_bg_ctx_t;

#endif
