#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "cli/types.h"

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...);
sp_cli_result_t spn_cli_parse_profile(sp_cli_t* cli, spn_profile_override_t* profile);
sp_cli_result_t spn_cli_session(sp_cli_t* cli, spn_session_config_t config);

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

#endif
