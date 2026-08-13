#ifndef SPN_CLI_CLI_H
#define SPN_CLI_CLI_H

#include "cli/types.h"

#define try(expr) \
  do { \
    sp_cli_result_t __result = (expr); \
    if (__result != SP_CLI_OK) { \
      return __result; \
    } \
  } while (0)

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...);
sp_cli_result_t spn_cli_parse_profile(sp_cli_t* cli, spn_profile_override_t* profile);
spn_str_arr_t spn_cli_rest_names(sp_cli_t* cli);
sp_cli_result_t spn_cli_open(bool project_optional);
sp_cli_result_t spn_cli_session(sp_cli_t* cli, spn_session_config_t config);
sp_cli_result_t spn_cli_refresh_indexes();
sp_cli_result_t spn_cli_op(spn_op_t* op);
spn_err_t spn_cli_wait(spn_op_t* op);

sp_cli_result_t spn_cli_init(sp_cli_t* cli);
sp_cli_result_t spn_cli_add(sp_cli_t* cli);
sp_cli_result_t spn_cli_build(sp_cli_t* cli);
sp_cli_result_t spn_cli_test(sp_cli_t* cli);
sp_cli_result_t spn_cli_clean(sp_cli_t* cli);
sp_cli_result_t spn_cli_publish(sp_cli_t* cli);
sp_cli_result_t spn_cli_index(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_list(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_path(sp_cli_t* cli);
sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli);

sp_cli_cmd_t* spn_cli();

#endif
