#ifndef SPN_BUILD_NODES_H
#define SPN_BUILD_NODES_H

#include "graph/types.h"
#include "dag/types.h"
#include "forward/types.h"
#include "unit/types.h"

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data);
s32 link_target(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_compile_object_run(spn_compile_unit_t* unit, sp_str_t object, sp_str_t depfile);
s32 spn_link_target_run(spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects);
s32 spn_embed_write(spn_target_unit_t* unit, sp_str_t obj, sp_str_t hdr, sp_mem_t obs_mem, sp_da(spn_dag_obs_t)* obs);
spn_err_t emit_link_passed(spn_target_unit_t* unit, sp_str_t output, sp_str_t out, u64 elapsed);
spn_err_t emit_link_failed(spn_target_unit_t* unit, s32 rc, sp_str_t out, sp_str_t err);

#endif
