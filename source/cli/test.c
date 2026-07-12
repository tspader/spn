#include "cli/cli.h"

#include "ctx/types.h"
#include "task/task.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_cli_test_t* command = &spn.cli.test;

  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  if (!sp_str_empty(command->name)) {
    sp_da_push(names, command->name);
  }
  app.config.compile.targets = (spn_target_selection_t) {
    .kind = SPN_TARGET_SELECTION_EXPLICIT,
    .targets = {
      .test = {
        .kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED,
        .names = names,
      },
    },
  };
  app.config.action = (spn_action_t) {
    .kind = SPN_ACTION_RUN_ROOTS,
  };

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_PLAN,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH,
    SPN_TASK_RUN
  );
}
