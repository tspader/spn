#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  spn_cli_command(cli)->op = (spn_op_desc_t) {
    .kind = SPN_OP_CLEAN,
    .clean = { .whole = sp_str_empty(args.profile.name) },
  };
  return SP_CLI_CONTINUE;
}
