#include "ctx/types.h"

#include "cli/cli.h"
#include "task/task.h"

static void spn_cli_build_set_rule(spn_target_rule_t* rule, bool selected, spn_target_names_t names) {
  if (!selected) {
    rule->kind = SPN_TARGET_RULE_NONE;
    return;
  }

  rule->kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED;
  rule->names = names;
}

sp_cli_result_t spn_cli_build(sp_cli_t* cli) {
  spn_cli_build_t* command = &spn.cli.build;

  app.config.force = command->force;
  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  sp_for(it, cli->num_rest) {
    sp_da_push(names, sp_cstr_as_str(cli->rest[it]));
  }

  bool specific = command->only.bin || command->only.lib || command->only.test || command->only.script;
  if (specific || !sp_da_empty(names)) {
    app.config.compile.targets.kind = SPN_TARGET_SELECTION_EXPLICIT;
    bool all_kinds = !specific;
    spn_cli_build_set_rule(&app.config.compile.targets.targets.bin, all_kinds || command->only.bin, names);
    spn_cli_build_set_rule(&app.config.compile.targets.targets.lib, all_kinds || command->only.lib, names);
    spn_cli_build_set_rule(&app.config.compile.targets.targets.test, all_kinds || command->only.test, names);
    spn_cli_build_set_rule(&app.config.compile.targets.targets.script, all_kinds || command->only.script, names);
  }

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH
  );
}
