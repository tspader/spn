#ifndef SPN_UNIT_PACKAGE_H
#define SPN_UNIT_PACKAGE_H

#include "error/types.h"
#include "unit/types.h"

void               spn_pkg_unit_write_stamp(spn_pkg_unit_t* ctx, sp_str_t path);
sp_str_t           spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node);
void               spn_pkg_unit_announce_compile(spn_pkg_unit_t* ctx);
spn_err_t          spn_pkg_unit_publish_headers(spn_pkg_unit_t* ctx, bool strict);

#endif
