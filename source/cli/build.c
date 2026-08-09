#include "cli/cli.h"

static void set_rule(spn_target_rule_t* rule, bool selected, spn_str_arr_t names) {
  if (!selected) {
    rule->kind = SPN_TARGET_RULE_NONE;
    return;
  }

  rule->kind = names.count ? SPN_TARGET_RULE_NAMED : SPN_TARGET_RULE_ALL;
  rule->names = names;
}

sp_cli_result_t spn_cli_build(sp_cli_t* cli) {
  try(spn_cli_open(false));

  spn_cli_build_t* cmd = &args.build;

  spn_session_config_t config = { .force = cmd->force };
  spn_str_arr_t names = spn_cli_rest_names(cli);

  bool specific = cmd->only.bin || cmd->only.lib || cmd->only.test || cmd->only.script;
  if (specific || names.count) {
    bool all_kinds = !specific;
    set_rule(&config.selection.bin, all_kinds || cmd->only.bin, names);
    set_rule(&config.selection.lib, all_kinds || cmd->only.lib, names);
    set_rule(&config.selection.test, all_kinds || cmd->only.test, names);
    set_rule(&config.selection.script, all_kinds || cmd->only.script, names);
  }

  try(spn_cli_refresh_indexes());
  try(spn_cli_session(cli, config));
  return spn_cli_op(spn_build(host.session));
}
