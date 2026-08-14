#include "commands/util/util.h"

static sp_cli_result_t test(sp_cli_t* cli) {
  try(spn_cli_open(false));

  spn_str_arr_t names = spn_cli_rest_names(cli);

  spn_session_config_t config = {
    .selection = {
      .test = {
        .kind = names.count ? SPN_TARGET_RULE_NAMED : SPN_TARGET_RULE_ALL,
        .names = names,
      },
    },
  };
  try(spn_cli_refresh_indexes());
  try(spn_cli_session(cli, config));
  try(spn_cli_op(spn_build(host.session)));
  return spn_cli_op(spn_run_tests(host.session));
}

sp_cli_cmd_t spn_cmd_test = {
  .name = "test",
  .summary = "Build and run tests",
  .opts = {
    {
      .brief = 'p',
      .name = "profile",
      .kind = SP_CLI_OPT_STR,
      .summary = "Profile to use for building",
      .placeholder = "PROFILE",
      .ptr = &host.args.profile.name,
    },
    {
      .name = "toolchain",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override toolchain",
      .placeholder = "NAME",
      .ptr = &host.args.profile.toolchain,
    },
    {
      .brief = 'm',
      .name = "mode",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override build mode (debug, release)",
      .placeholder = "MODE",
      .ptr = &host.args.profile.mode,
    },
    {
      .name = "opt",
      .kind = SP_CLI_OPT_STR,
      .summary = "Override optimization level (0, 1, 2, 3, s, z)",
      .placeholder = "LEVEL",
      .ptr = &host.args.profile.opt,
    },
    {
      .name = "sanitize",
      .kind = SP_CLI_OPT_STR,
      .summary = "Enable sanitizers (address, thread, undefined, memory, leak)",
      .placeholder = "LIST",
      .ptr = &host.args.profile.sanitize,
    },
  },
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_REST,
      .summary = "Test targets to build and run (default: all)",
    },
  },
  .handler = test,
};
