#ifndef SPN_OP_BUILD_BUILD_H
#define SPN_OP_BUILD_BUILD_H

#include "compiler/types.h"
#include "error/types.h"
#include "external/cc.h"
#include "pkg/types.h"
#include "session/types.h"
#include "target/types.h"
#include "unit/types.h"

spn_err_union_t spn_build_link_invocation(sp_mem_t mem, spn_target_unit_t* target, const spn_cc_link_files_t* files, spn_invocation_t* invocation);
sp_str_t get_embed_object_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_embed_header_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_output_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_exports_path(sp_mem_t mem, spn_target_unit_t* unit);
sp_str_t get_target_staged_path(sp_mem_t mem, spn_target_unit_t* unit);

#endif
