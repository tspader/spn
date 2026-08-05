#include "sp.h"
#include "sp/macro.h"
#include "cli/cli.h"

#include "app/types.h"
#include "ctx/types.h"

sp_cli_result_t spn_cli_generate(sp_cli_t* cli) {
  spn_cli_generate_t* command = &spn.cli.generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    return cli_error(cli,
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {.yellow}",
      sp_fmt_str(command->path),
      sp_fmt_cstr("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    return cli_error(cli, "no lock file found; run {.yellow} first", sp_fmt_cstr("spn build"));
  }

  spn.exec.desc = (spn_op_desc_t) { .kind = SPN_OP_REACH, .reach = SPN_PHASE_CONFIGURED };
  return SP_CLI_CONTINUE;
}
