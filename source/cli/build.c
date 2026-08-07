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

  spn_session_config_t config = { .force = cmd->force };
  spn_target_names_t names = sp_da_new(host.mem, sp_str_t);
  for (const c8** it = cli->rest; *it; it++) {
    sp_da_push(names, sp_cstr_as_str(*it));
  }

  bool specific = cmd->only.bin || cmd->only.lib || cmd->only.test || cmd->only.script;
  if (specific || !sp_da_empty(names)) {
    config.selection.kind = SPN_TARGET_SELECTION_EXPLICIT;
    bool all_kinds = !specific;
    set_rule(&config.selection.bin, all_kinds || cmd->only.bin, names);
    set_rule(&config.selection.lib, all_kinds || cmd->only.lib, names);
    set_rule(&config.selection.test, all_kinds || cmd->only.test, names);
    set_rule(&config.selection.script, all_kinds || cmd->only.script, names);
  }

  sp_cli_result_t session = spn_cli_session(cli, config);
  if (session != SP_CLI_OK) {
    return session;
  }
  return spn_op_build(host.session) ? SP_CLI_ERR : SP_CLI_OK;
}
