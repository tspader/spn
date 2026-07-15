#ifndef SPN_FORWARD_H
#define SPN_FORWARD_H

#include "sp.h"

typedef struct spn spn_t;
typedef struct spn_app_t spn_app_t;
typedef struct spn_event_buffer_t spn_event_buffer_t;
typedef struct spn_index_info spn_index_info_t;
typedef struct spn_pkg_info spn_pkg_info_t;
typedef struct spn_resolved_pkg spn_resolved_pkg_t;
typedef struct spn_profile_info spn_profile_info_t;
typedef struct spn_target_info spn_target_info_t;
typedef struct spn_autoconf spn_autoconf_t;
typedef struct spn_make spn_make_t;
typedef struct spn_cmake spn_cmake_t;
typedef struct spn_node_t spn_node_t;
typedef struct spn_build_unit_t spn_build_unit_t;
typedef struct spn_pkg_unit_t spn_pkg_unit_t;
typedef struct spn_node_ctx_t spn_node_ctx_t;
typedef struct spn_toolchain_unit_t spn_toolchain_unit_t;
typedef struct spn_toolchain_catalog_t spn_toolchain_catalog_t;
typedef struct spn_target_unit spn_target_unit_t;
typedef struct spn_session_t spn_session_t;
typedef struct spn_user_node_t spn_user_node_t;
typedef struct spn_dag_build_t spn_dag_build_t;
typedef struct sp_intern_t sp_intern_t;

typedef struct toml_table_t toml_table_t;

typedef s32  (*spn_node_fn_t)(spn_t*, spn_node_ctx_t*);

#endif
