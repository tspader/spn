#ifndef SPN_CORE_TYPES_H
#define SPN_CORE_TYPES_H

#include "sp.h"
#include "spn/core.h"
#include "spn/types.h"

typedef struct spn_project_t spn_project_t;
typedef struct spn_ctx_t spn_ctx_t;
typedef struct spn_event_buffer_t spn_event_buffer_t;
typedef struct spn_index_info spn_index_info_t;
typedef struct spn_pkg_info spn_pkg_info_t;
typedef struct spn_resolved_pkg spn_resolved_pkg_t;
typedef struct spn_profile_info spn_profile_info_t;
typedef struct spn_target_info spn_target_info_t;
typedef struct spn_build_unit_t spn_build_unit_t;
typedef struct spn_pkg_unit_t spn_pkg_unit_t;
typedef struct spn_toolchain_unit_t spn_toolchain_unit_t;
typedef struct spn_toolchain_catalog_t spn_toolchain_catalog_t;
typedef struct spn_target_unit spn_target_unit_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_op_t spn_op_t;
typedef struct spn_user_node_t spn_user_node_t;
typedef struct spn_dag_build_t spn_dag_build_t;
typedef struct sp_intern_t sp_intern_t;

typedef sp_hash_t spn_build_id_t;

typedef struct {
  sp_str_t path;
  spn_tree_t tree;
} spn_tree_path_t;

typedef struct {
  sp_str_t recipe;
  sp_str_t source;
} spn_tree_roots_t;

typedef struct {
  spn_wake_fn_t fn;
  void* data;
  sp_atomic_u32_t signaled;
} spn_wake_t;

typedef struct toml_table_t toml_table_t;

#endif
