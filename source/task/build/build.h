#ifndef SPN_BUILD_BUILD_H
#define SPN_BUILD_BUILD_H

#include "compiler/types.h"
#include "error/types.h"
#include "external/cc.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

void spn_target_resolve_link(spn_target_unit_t* target);
spn_err_union_t spn_build_render_target(sp_mem_t mem, spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, spn_invocation_t* invocation);
spn_err_union_t spn_build_validate_target(spn_target_unit_t* target);
sp_da(sp_str_t) spn_build_target_objects(sp_mem_t mem, spn_target_unit_t* target);
sp_str_t get_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* unit);

#endif
