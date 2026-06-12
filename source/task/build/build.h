#ifndef SPN_BUILD_BUILD_H
#define SPN_BUILD_BUILD_H

#include "external/cc.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

void add_pkg_to_cc(spn_cc_t* cc, spn_pkg_unit_t* pkg);
void add_pkg_to_cc_target(spn_cc_target_t* target, spn_pkg_unit_t* pkg, spn_target_info_t* info);
void add_deps_to_cc_target(spn_cc_target_t* cc, spn_target_unit_t* target);
sp_str_t get_embed_object_path(spn_target_unit_t* unit);
sp_str_t get_embed_header_path(spn_target_unit_t* unit);
sp_str_t get_target_output_path(spn_target_unit_t* unit);

#endif
