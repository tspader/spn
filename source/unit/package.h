#ifndef SPN_UNIT_PACKAGE_H
#define SPN_UNIT_PACKAGE_H

#include "err.h"
#include "unit/types.h"

void spn_pkg_unit_init(spn_pkg_unit_t* ctx, spn_pkg_unit_config_t config);
void spn_pkg_unit_write_stamp(spn_pkg_unit_t* ctx, sp_str_t path);
spn_err_t spn_pkg_unit_sync_remote(spn_pkg_unit_t* dep);
spn_err_t spn_pkg_unit_sync_local(spn_pkg_unit_t* dep);
spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* ctx, spn_build_fn_t fn);
sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node);
void spn_pkg_unit_add_target(spn_pkg_unit_t* pkg, spn_target_t* target);
sp_str_t spn_pkg_unit_get_include_dir(spn_pkg_unit_t* unit);

#endif
