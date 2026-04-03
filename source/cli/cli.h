#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "app/types.h"
#include "cli/types.h"
#include "err.h"

#define SPN_CLI_UNIMPLEMENTED() SP_LOG("unimplemented"); return SP_APP_ERR;

spn_cli_command_info_t spn_cli_command_info_from_usage(spn_cli_usage_t cmd);

sp_app_result_t spn_cli_help(spn_cli_parser_t* p);
sp_app_result_t spn_cli_root(spn_cli_t* cli);
sp_app_result_t spn_cli_init(spn_cli_t* cli);
sp_app_result_t spn_cli_add(spn_cli_t* cli);
sp_app_result_t spn_cli_build(spn_cli_t* cli);
sp_app_result_t spn_cli_run(spn_cli_t* cli);
sp_app_result_t spn_cli_test(spn_cli_t* cli);
sp_app_result_t spn_cli_clean(spn_cli_t* cli);
sp_app_result_t spn_cli_copy(spn_cli_t* cli);
sp_app_result_t spn_cli_generate(spn_cli_t* cli);
sp_app_result_t spn_cli_graph(spn_cli_t* cli);
sp_app_result_t spn_cli_list(spn_cli_t* cli);
sp_app_result_t spn_cli_ls(spn_cli_t* cli);
sp_app_result_t spn_cli_manifest(spn_cli_t* cli);
sp_app_result_t spn_cli_which(spn_cli_t* cli);
sp_app_result_t spn_cli_update(spn_cli_t* cli);
sp_app_result_t spn_cli_tool(spn_cli_t* cli);
sp_app_result_t spn_cli_tool_install(spn_cli_t* cli);
sp_app_result_t spn_cli_tool_uninstall(spn_cli_t* cli);
sp_app_result_t spn_cli_tool_run(spn_cli_t* cli);
sp_app_result_t spn_cli_publish(spn_cli_t* cli);
spn_cli_usage_t spn_cli();

#endif
