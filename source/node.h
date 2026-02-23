#ifndef SPN_NODE_H
#define SPN_NODE_H

#include "sp.h"
#include "spn.h"
#include "graph.h"

typedef struct spn_pkg_unit_t spn_pkg_unit_t;

typedef struct spn_user_node_t {
  spn_pkg_unit_t* ctx;
  sp_str_t tag;
  spn_node_fn_t fn;
  void* user_data;
  sp_da(sp_str_t) inputs;
  sp_da(sp_str_t) outputs;
  sp_da(spn_node_t) deps;
  spn_bg_id_t id;
} spn_user_node_t;

typedef struct {
  spn_bg_id_t build;
  spn_bg_id_t bin;
} spn_target_nodes_t;

typedef struct {
  spn_bg_id_t manifest;
  spn_bg_id_t script;
  spn_bg_id_t package;
  spn_bg_id_t main;
  spn_bg_id_t exit;
  struct {
    spn_bg_id_t package;
    spn_bg_id_t main;
    spn_bg_id_t exit;
  } stamp;
  sp_da(spn_bg_id_t) user;
} spn_pkg_nodes_t;

#endif
