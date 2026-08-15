#ifndef SPN_GRAPH_IDENTITY_H
#define SPN_GRAPH_IDENTITY_H

#include "dag/types.h"
#include "pkg/types.h"
#include "unit/types.h"

typedef struct {
  spn_pkg_root_kind_t kind;
  sp_str_t rev;
  sp_str_t dir;
  sp_hash_t patches;
} spn_build_source_pin_t;

bool                   spn_build_copy_to_include(spn_publish_copy_t* copy, sp_str_t* rest);
spn_build_source_pin_t spn_build_source_pin(spn_pkg_unit_t* unit);
spn_dag_digest_t       spn_build_tree_identity(spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin);
spn_dag_digest_t       spn_build_package_identity(spn_pkg_unit_t* unit, const spn_build_source_pin_t* pin);
spn_dag_digest_t       spn_build_user_identity(spn_user_node_t* node, const spn_build_source_pin_t* pin);
spn_dag_digest_t       spn_build_compile_identity(const spn_compile_unit_t* unit);
spn_err_t              spn_build_link_identity(sp_mem_t mem, spn_target_unit_t* target, spn_path_t output, sp_da(spn_path_t) objects, spn_path_t exports, spn_dag_digest_t* identity);
spn_err_t              spn_build_exports_identity(sp_mem_t mem, spn_target_unit_t* target, spn_path_t output, sp_da(spn_path_t) objects, spn_dag_digest_t* identity);

#endif
