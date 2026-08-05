#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "cli/types.h"
#include "error/types.h"
#include "shell/types.h"

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...);
spn_command_t* spn_cli_command(sp_cli_t* cli);
spn_err_union_t spn_cli_parse_profile(spn_profile_args_t* args, spn_profile_info_t* result);
spn_err_union_t spn_cli_run_roots(spn_shell_t* shell);

sp_cli_result_t spn_cli_init(sp_cli_t* cli);
sp_cli_result_t spn_cli_add(sp_cli_t* cli);
sp_cli_result_t spn_cli_build(sp_cli_t* cli);
sp_cli_result_t spn_cli_run(sp_cli_t* cli);
sp_cli_result_t spn_cli_test(sp_cli_t* cli);
sp_cli_result_t spn_cli_clean(sp_cli_t* cli);
sp_cli_result_t spn_cli_publish(sp_cli_t* cli);
sp_cli_result_t spn_cli_index(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_list(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_path(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli);

sp_cli_cmd_t* spn_cli();
bool spn_cli_requires_manifest(sp_cli_cmd_t* cmd);

#endif
