#ifndef SPN_NODE_H
#define SPN_NODE_H

#include "sp.h"
#include "spn.h"
#include "graph.h"

struct spn_node_t {
  spn_pkg_unit_t* ctx;
  u32 index;
};

struct spn_node_ctx_t {
  spn_build_ctx_t* build;
  void* user_data;
};

struct spn_user_node_t {
  spn_pkg_unit_t* ctx;
  sp_str_t tag;
  spn_node_fn_t fn;
  void* user_data;
  sp_da(sp_str_t) inputs;
  sp_da(sp_str_t) outputs;
  sp_da(spn_node_t*) deps;
  spn_bg_id_t id;
};

#endif
