#ifndef SPN_FORWARD_H
#define SPN_FORWARD_H

#include "sp.h"

typedef struct spn_app_t spn_app_t;
typedef struct spn_event_buffer_t spn_event_buffer_t;
typedef struct spn_index_info spn_index_info_t;
typedef struct spn_pkg_info spn_pkg_info_t;
typedef struct spn_profile_info spn_profile_info_t;
typedef struct spn_target_info spn_target_info_t;
typedef struct spn_autoconf spn_autoconf_t;
typedef struct spn_make spn_make_t;
typedef struct spn_cmake spn_cmake_t;
typedef struct spn_cc spn_cc_t;
typedef struct spn_node_t spn_node_t;
typedef struct spn_pkg_unit_t spn_pkg_unit_t;
typedef struct spn_node_ctx_t spn_node_ctx_t;

typedef struct toml_table_t toml_table_t;

typedef s32  (*spn_node_fn_t)(spn_node_ctx_t*);

#endif
