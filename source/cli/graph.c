#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_graph(sp_cli_t* cli) {
  spn.config.selection = (spn_target_selection_t) {
    .kind = SPN_TARGET_SELECTION_EXPLICIT,
    .bin = { .kind = SPN_TARGET_RULE_ALL },
    .lib = { .kind = SPN_TARGET_RULE_ALL },
    .test = { .kind = SPN_TARGET_RULE_ALL },
    .script = { .kind = SPN_TARGET_RULE_ALL },
  };

  spn.exec.desc = (spn_op_desc_t) { .kind = SPN_OP_REACH, .reach = SPN_PHASE_UNITS };
  return SP_CLI_CONTINUE;
}
