#ifndef SPN_BUILD_NODES_H
#define SPN_BUILD_NODES_H

#include "graph/types.h"

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data);
s32 compile_embed(spn_bg_cmd_t* cmd, void* user_data);
s32 compile_build_script(spn_bg_cmd_t* cmd, void* user_data);
s32 link_target(spn_bg_cmd_t* cmd, void* user_data);
s32 stamp_enter(spn_bg_cmd_t* cmd, void* user_data);
s32 stamp_exit(spn_bg_cmd_t* cmd, void* user_data);
s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data);
s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data);
s32 stage_targets(spn_bg_cmd_t* cmd, void* user_data);

#endif
