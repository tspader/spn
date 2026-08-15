#ifndef SPN_GRAPH_NODES_H
#define SPN_GRAPH_NODES_H

#include "compiler/types.h"
#include "dag/types.h"
#include "sp.h"
#include "spn/core.h"
#include "external/cc.h"
#include "core/types.h"
#include "unit/types.h"

s32 spn_compile_object_run(spn_compile_unit_t* unit, spn_path_t object, spn_path_t depfile);
spn_err_t spn_link_target_run(spn_target_unit_t* target, spn_path_t output, sp_da(spn_path_t) objects, spn_path_t exports);
spn_err_t spn_link_exports_run(spn_target_unit_t* target, sp_da(spn_path_t) objects, spn_path_t output);
s32 spn_embed_write(spn_target_unit_t* unit, spn_path_t obj, spn_path_t hdr, sp_mem_t obs_mem, sp_da(spn_dag_obs_t)* obs);

#endif
