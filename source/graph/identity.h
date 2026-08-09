#ifndef SPN_GRAPH_IDENTITY_H
#define SPN_GRAPH_IDENTITY_H

#include "dag/types.h"
#include "pkg/types.h"
#include "unit/types.h"

typedef struct {
  spn_pkg_tree_kind_t kind;
  sp_str_t rev;
  sp_str_t dir;
  sp_hash_t patches;
} spn_build_source_pin_t;

bool                   spn_build_copy_to_include(spn_publish_copy_t* copy, sp_str_t* rest);
bool                   spn_build_path_within(sp_str_t path, sp_str_t dir);
spn_build_source_pin_t spn_build_source_pin(spn_pkg_unit_t* unit);
spn_dag_digest_t       spn_build_tree_identity(const spn_dag_roots_t* roots, spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin);
spn_dag_digest_t       spn_build_package_identity(spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin);
spn_dag_digest_t       spn_build_user_identity(const spn_dag_roots_t* roots, spn_user_node_t* node, const spn_build_source_pin_t* pin);

#endif
