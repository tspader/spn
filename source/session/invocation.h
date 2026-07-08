#ifndef SPN_SESSION_INVOCATION_H
#define SPN_SESSION_INVOCATION_H

#include "sp.h"

#include "forward/types.h"
#include "unit/types.h"

typedef struct {
  sp_ps_output_t result;
  u64 elapsed;
} spn_invocation_result_t;

void                    spn_session_build_invocations(spn_session_t* session);
spn_err_t               spn_session_write_compile_commands(spn_session_t* session, sp_str_t path);
sp_str_t                spn_session_compile_commands_path(spn_session_t* session);
spn_invocation_result_t spn_invocation_run(spn_invocation_t* invocation);

#endif
