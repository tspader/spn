#include "commands/commands.h"

#include "commands/util/util.h"

extern sp_cli_cmd_t spn_cmd_init;
extern sp_cli_cmd_t spn_cmd_add;
extern sp_cli_cmd_t spn_cmd_clean;
extern sp_cli_cmd_t spn_cmd_build;
extern sp_cli_cmd_t spn_cmd_test;
extern sp_cli_cmd_t spn_cmd_publish;
extern sp_cli_cmd_t spn_cmd_index;

static sp_cli_cmd_t root = {
  .name = "spn",
  .summary = "A package manager and build tool for C",
  .opts = {
    {
      .brief = 'C',
      .name = "project-dir",
      .kind = SP_CLI_OPT_STR,
      .summary = "Specify the directory containing project file",
      .placeholder = "DIR",
      .ptr = &host.args.project_dir,
    },
    {
      .brief = 'f',
      .name = "file",
      .kind = SP_CLI_OPT_STR,
      .summary = "Specify the project file path",
      .placeholder = "FILE",
      .ptr = &host.args.project_file,
    },
    {
      .brief = 'o',
      .name = "output",
      .kind = SP_CLI_OPT_STR,
      .summary = "Output mode: interactive, noninteractive, quiet, none, json",
      .placeholder = "MODE",
      .ptr = &host.args.output,
    },
    {
      .brief = 'v',
      .name = "verbose",
      .summary = "Show verbose output",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &host.args.verbose,
    },
    {
      .brief = 'q',
      .name = "quiet",
      .summary = "Only show errors",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &host.args.quiet,
    },
  },
  .env = {
    {
      .name = "SPN_INDEX_REFRESH_SECONDS",
      .kind = SP_CLI_OPT_U32,
      .summary = "Number of seconds which must elapse before the index gets refreshed",
      .ptr = &host.args.refresh
    },
  },
  .commands = {
    &spn_cmd_init,
    &spn_cmd_add,
    &spn_cmd_clean,
    &spn_cmd_build,
    &spn_cmd_test,
    &spn_cmd_publish,
    &spn_cmd_index,
  },
};

sp_cli_cmd_t* spn_cli() {
  return &root;
}
