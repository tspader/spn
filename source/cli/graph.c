#include "cli/cli.h"

#include "ctx/types.h"
#include "task/task.h"

sp_cli_result_t spn_cli_graph(sp_cli_t* cli) {
  app.config.compile.targets = (spn_target_selection_t) {
    .kind = SPN_TARGET_SELECTION_EXPLICIT,
    .targets = {
      .bin = { .kind = SPN_TARGET_RULE_ALL },
      .lib = { .kind = SPN_TARGET_RULE_ALL },
      .test = { .kind = SPN_TARGET_RULE_ALL },
      .script = { .kind = SPN_TARGET_RULE_ALL },
    },
  };

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_PLAN,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_RENDER_GRAPH
  );
}
