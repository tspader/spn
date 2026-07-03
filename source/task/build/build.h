#ifndef SPN_BUILD_BUILD_H
#define SPN_BUILD_BUILD_H

#include "external/cc.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

void add_deps_to_cc_target(spn_cc_target_t* cc, spn_target_unit_t* target);
void spn_cc_target_add_build_deps(spn_cc_target_t* target, spn_pkg_unit_t* unit);
spn_err_t spn_compile_script_module(spn_pkg_unit_t* unit, spn_target_info_t* script, sp_str_t output);
sp_str_t get_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);

#endif
