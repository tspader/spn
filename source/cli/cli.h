#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "app/types.h"
#include "cli/types.h"
#include "error/types.h"

#define SPN_CLI_UNIMPLEMENTED() return sp_cli_set_error_c(cli, "unimplemented")

sp_cli_result_t spn_cli_errf(sp_cli_t* cli, const c8* fmt, ...);

sp_cli_result_t spn_cli_init(sp_cli_t* cli);
sp_cli_result_t spn_cli_add(sp_cli_t* cli);
sp_cli_result_t spn_cli_build(sp_cli_t* cli);
sp_cli_result_t spn_cli_run(sp_cli_t* cli);
sp_cli_result_t spn_cli_test(sp_cli_t* cli);
sp_cli_result_t spn_cli_clean(sp_cli_t* cli);
sp_cli_result_t spn_cli_copy(sp_cli_t* cli);
sp_cli_result_t spn_cli_generate(sp_cli_t* cli);
sp_cli_result_t spn_cli_graph(sp_cli_t* cli);
sp_cli_result_t spn_cli_list(sp_cli_t* cli);
sp_cli_result_t spn_cli_ls(sp_cli_t* cli);
sp_cli_result_t spn_cli_manifest(sp_cli_t* cli);
sp_cli_result_t spn_cli_which(sp_cli_t* cli);
sp_cli_result_t spn_cli_update(sp_cli_t* cli);
sp_cli_result_t spn_cli_tool(sp_cli_t* cli);
sp_cli_result_t spn_cli_tool_install(sp_cli_t* cli);
sp_cli_result_t spn_cli_tool_uninstall(sp_cli_t* cli);
sp_cli_result_t spn_cli_tool_run(sp_cli_t* cli);
sp_cli_result_t spn_cli_publish(sp_cli_t* cli);
sp_cli_result_t spn_cli_index(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_list(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_path(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli);
sp_cli_result_t spn_cli_completions(sp_cli_t* cli);

bool spn_complete_intercept();

sp_cli_cmd_t* spn_cli(void);
bool spn_cli_requires_manifest(sp_cli_cmd_t* cmd);
void spn_cli_commit(void);

#endif
