#include "sp/macro.h"
#include "sp.h"
#include "spn.h"

#include "api/api.h"
#include "ctx/types.h"
#include "event/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "event/event.h"
#include "intern/intern.h"

spn_node_t* spn_add_node(spn_config_t* config, const c8* tag) {
  spn_pkg_unit_t* unit = spn_api_unit(config);
  SPN_API_LOG(unit, "spn_add_node", "{}", SP_FMT_CSTR(tag));

  sp_mem_t mem = spn.mem;
  u32 index = sp_da_size(unit->nodes.user);
  spn_user_node_t node = {
    .pkg = unit,
    .tag = spn_intern_cstr(tag),
  };
  sp_da_init(mem, node.inputs);
  sp_da_init(mem, node.outputs);
  sp_da_init(mem, node.deps);
  sp_da_push(unit->nodes.user, node);

  spn_node_t* out = sp_alloc_type(mem, spn_node_t);
  *out = (spn_node_t) {
    .ctx = unit,
    .index = index,
  };

  return out;
}

void spn_node_add_input(spn_node_t* node, const c8* input) {
  spn_user_node_t* info = spn_find_user_node(node);
  SPN_API_LOG(node->ctx, "spn_node_add_input", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(input));
  sp_da_push(info->inputs, spn_intern_cstr(input));
}

void spn_node_add_output(spn_node_t* node, const c8* output) {
  spn_user_node_t* info = spn_find_user_node(node);
  SPN_API_LOG(node->ctx, "spn_node_add_output", "{}, {}", SP_FMT_STR(info->tag), SP_FMT_CSTR(output));
  sp_da_push(info->outputs, spn_intern_cstr(output));
}

void spn_node_link(spn_node_t* from, spn_node_t* to) {
  spn_user_node_t* info = spn_find_user_node(to);
  SPN_API_LOG(to->ctx, "spn_node_link", "{} -> {}", SP_FMT_STR(spn_find_user_node(from)->tag), SP_FMT_STR(info->tag));
  sp_da_push(info->deps, from);
}

void spn_node_set_fn(spn_node_t* node, spn_node_fn_t fn) {
  spn_user_node_t* info = spn_find_user_node(node);
  SPN_API_LOG(node->ctx, "spn_node_set_fn", "{}", SP_FMT_STR(info->tag));
  info->fn = fn;
}

void spn_node_set_user_data(spn_node_t* node, void* user_data) {
  spn_find_user_node(node)->user_data = user_data;
}

void* spn_node_ctx_get_user_data(spn_node_ctx_t* ctx) {
  return ctx->user_data;
}
