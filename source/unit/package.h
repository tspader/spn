#ifndef SPN_UNIT_PACKAGE_H
#define SPN_UNIT_PACKAGE_H

#include "error/types.h"
#include "unit/types.h"

typedef struct {
  spn_target_map_t maps [4];
  u32 count;
} spn_pkg_unit_header_maps_t;

static inline spn_pkg_unit_header_maps_t spn_pkg_unit_header_maps(spn_pkg_unit_t* unit) {
  return (spn_pkg_unit_header_maps_t) {
    .maps = { unit->info->libs, unit->info->exes, unit->info->scripts, unit->info->tests },
    .count = unit->source == SPN_PKG_SOURCE_ROOT ? 4 : 1,
  };
}

void               spn_pkg_unit_write_stamp(spn_pkg_unit_t* ctx, sp_str_t path);
sp_str_t           spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node);
void               spn_pkg_unit_announce_compile(spn_pkg_unit_t* ctx);
void               spn_pkg_unit_create_layout(spn_pkg_unit_t* unit);
spn_err_t          spn_pkg_unit_publish_headers(spn_pkg_unit_t* ctx, sp_str_t root, spn_publish_t publish);

#endif
