#ifndef SPN_EXECUTOR_H
#define SPN_EXECUTOR_H

#include "graph.h"

s32 spn_executor_sync_repo(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_configure_pkg(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_build_pkg(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_user_fn(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_compile_object(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_link_target(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_run_package_hook(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_write_enter_stamp(spn_bg_cmd_t* cmd, void* user_data);
s32 spn_executor_write_exit_stamp(spn_bg_cmd_t* cmd, void* user_data);

#endif
