#include "../node.h"

#include "unit/types.h"

spn_user_node_t* spn_find_user_node(spn_node_t* node) {
  SP_ASSERT(node->index < sp_da_size(node->ctx->nodes.all));
  return &node->ctx->nodes.all[node->index];
}
