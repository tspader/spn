#ifndef SPN_GRAPH_NODES_H
#define SPN_GRAPH_NODES_H

#include "compiler/types.h"
#include "dag/types.h"
#include "sp.h"
#include "spn/core.h"
#include "external/cc.h"
#include "core/types.h"
#include "unit/types.h"

spn_err_t spn_build_link_invocation(sp_mem_t mem, spn_target_unit_t* target, const spn_cc_link_files_t* files, spn_invocation_t* invocation);
s32 spn_compile_object_run(spn_compile_unit_t* unit, sp_str_t object, sp_str_t depfile);
spn_err_t spn_link_target_run(spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, sp_str_t exports);
spn_err_t spn_link_exports_run(spn_target_unit_t* target, sp_da(sp_str_t) objects, sp_str_t output);
s32 spn_embed_write(spn_target_unit_t* unit, sp_str_t obj, sp_str_t hdr, sp_mem_t obs_mem, sp_da(spn_dag_obs_t)* obs);

#endif
