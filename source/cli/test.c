#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_cli_test_t* command = &spn.cli.test;

  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  if (!sp_str_empty(command->name)) {
    sp_da_push(names, command->name);
  }
  spn.config.selection = (spn_target_selection_t) {
    .kind = SPN_TARGET_SELECTION_EXPLICIT,
    .test = {
      .kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED,
      .names = names,
    },
  };

  spn.exec.desc = (spn_op_desc_t) { .kind = SPN_OP_REACH, .reach = SPN_PHASE_BUILT };
  spn.exec.finish = spn_cli_run_roots;
  return SP_CLI_CONTINUE;
}
