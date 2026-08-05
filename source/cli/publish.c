#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  spn_cli_publish_t* cmd = &spn.cli.publish;
  spn.exec.desc = (spn_op_desc_t) {
    .kind = SPN_OP_PUBLISH,
    .publish = {
      .index = cmd->index,
      .url = cmd->source_url,
      .revision = cmd->source_rev,
      .dry = cmd->dry,
      .allow_dirty = cmd->allow_dirty,
    },
  };
  return SP_CLI_CONTINUE;
}
