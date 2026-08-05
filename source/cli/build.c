#include "ctx/types.h"

#include "cli/cli.h"

static void set_rule(spn_target_rule_t* rule, bool selected, spn_target_names_t names) {
  if (!selected) {
    rule->kind = SPN_TARGET_RULE_NONE;
    return;
  }

  rule->kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED;
  rule->names = names;
}

sp_cli_result_t spn_cli_build(sp_cli_t* cli) {
  spn_cli_build_t* cmd = &args.build;
  spn_command_t* command = spn_cli_command(cli);

  command->config.force = cmd->force;
  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  sp_for(it, cli->num_rest) {
    sp_da_push(names, sp_cstr_as_str(cli->rest[it]));
  }

  bool specific = cmd->only.bin || cmd->only.lib || cmd->only.test || cmd->only.script;
  if (specific || !sp_da_empty(names)) {
    command->config.selection.kind = SPN_TARGET_SELECTION_EXPLICIT;
    bool all_kinds = !specific;
    set_rule(&command->config.selection.bin, all_kinds || cmd->only.bin, names);
    set_rule(&command->config.selection.lib, all_kinds || cmd->only.lib, names);
    set_rule(&command->config.selection.test, all_kinds || cmd->only.test, names);
    set_rule(&command->config.selection.script, all_kinds || cmd->only.script, names);
  }

  command->op = (spn_op_desc_t) { .kind = SPN_OP_BUILD };
  return SP_CLI_CONTINUE;
}
