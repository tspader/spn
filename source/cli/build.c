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
  spn_cli_build_t* command = &spn.cli.build;

  spn.config.force = command->force;
  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  sp_for(it, cli->num_rest) {
    sp_da_push(names, sp_cstr_as_str(cli->rest[it]));
  }

  bool specific = command->only.bin || command->only.lib || command->only.test || command->only.script;
  if (specific || !sp_da_empty(names)) {
    spn.config.selection.kind = SPN_TARGET_SELECTION_EXPLICIT;
    bool all_kinds = !specific;
    set_rule(&spn.config.selection.bin, all_kinds || command->only.bin, names);
    set_rule(&spn.config.selection.lib, all_kinds || command->only.lib, names);
    set_rule(&spn.config.selection.test, all_kinds || command->only.test, names);
    set_rule(&spn.config.selection.script, all_kinds || command->only.script, names);
  }

  spn.exec.desc = (spn_op_desc_t) { .kind = SPN_OP_REACH, .reach = SPN_PHASE_BUILT };
  return SP_CLI_CONTINUE;
}
