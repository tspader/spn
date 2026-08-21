#ifndef SPN_CLI_UTIL_H
#define SPN_CLI_UTIL_H

#include "sp.h"
#include "sp/sp_cli.h"

#include "spn/host.h"

#include "tui/types.h"

#define try(expr) \
  do { \
    sp_cli_result_t __result = (expr); \
    if (__result != SP_CLI_OK) { \
      return __result; \
    } \
  } while (0)

typedef struct {
  sp_str_t name;
  sp_str_t toolchain;
  sp_str_t mode;
  sp_str_t opt;
  sp_str_t sanitize;
  sp_str_t target;
  sp_str_t os;
  sp_str_t arch;
  sp_str_t abi;
} spn_cli_profile_t;

typedef struct {
  struct {
    sp_str_t project_dir;
    sp_str_t project_file;
    sp_str_t output;
    bool verbose;
    bool quiet;
    u32 refresh;
    spn_cli_profile_t profile;
  } args;

  spn_ctx_t* ctx;
  spn_session_t* session;
  sp_mem_t mem;
  sp_sys_event_t doorbell;
  sp_atomic_s32_t interrupted;
} spn_cli_host_t;

extern spn_cli_host_t host;
extern spn_tui_t tui;

void spn_cli_boot();
s32 spn_cli_shutdown(bool ok);

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...);
sp_cli_result_t spn_cli_parse_profile(sp_cli_t* cli, spn_profile_override_t* profile);
spn_str_arr_t spn_cli_rest_names(sp_cli_t* cli);
sp_cli_result_t spn_cli_open(bool project_optional);
sp_cli_result_t spn_cli_session(sp_cli_t* cli, spn_session_config_t config);
sp_cli_result_t spn_cli_refresh_indexes();
sp_cli_result_t spn_cli_op(spn_op_t* op);
spn_err_t spn_cli_wait(spn_op_t* op);

#endif
