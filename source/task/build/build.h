#ifndef SPN_BUILD_BUILD_H
#define SPN_BUILD_BUILD_H

#include "external/cc.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

void add_deps_to_cc_target(spn_cc_link_t* link, spn_target_unit_t* target);
spn_err_union_t spn_build_link_invocations(spn_session_t* session);
sp_str_t get_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* unit);

#endif
