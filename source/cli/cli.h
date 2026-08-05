#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "cli/types.h"
#include "error/types.h"
#include "session/types.h"

#define try_cli(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      return spn_cli_result(cli, __err); \
    } \
  } while (0)

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...);
sp_cli_result_t spn_cli_result(sp_cli_t* cli, spn_err_union_t err);
spn_cli_exec_t* spn_cli_exec(sp_cli_t* cli);
spn_err_union_t spn_cli_require_project();
spn_err_union_t spn_cli_open_session(spn_session_config_t config);
spn_err_union_t spn_cli_run_roots();

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
