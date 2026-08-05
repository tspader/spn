#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_which(sp_cli_t* cli) {
  spn.exec.desc = (spn_op_desc_t) { .kind = SPN_OP_REACH, .reach = SPN_PHASE_CONFIGURED };
  return SP_CLI_CONTINUE;
}
