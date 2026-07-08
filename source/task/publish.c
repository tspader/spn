#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"

#include "index/publish.h"
#include "task/task.h"

spn_task_step_t spn_task_publish(spn_app_t* app) {
  spn_cli_publish_t* cmd = &spn.cli.publish;

  sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;

  spn_index_info_t* index = SP_NULLPTR;
  sp_da_for(spn.indexes, it) {
    if (sp_str_equal(spn.indexes[it].name, index_name)) {
      index = &spn.indexes[it];
      break;
    }
  }

  if (!index) {
    return spn_task_fail(SPN_ERR_INDEX_UNKNOWN, .index = { .name = index_name });
  }

  spn_publish_opts_t opts = {
    .mem = spn.mem,
    .intern = spn.intern,
    .cwd = spn.paths.cwd,
    .index = index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
  };

  spn_err_union_t result = spn_publish(&opts);
  if (result.kind) {
    return spn_task_fail_with(result);
  }

  SP_LOG("published successfully");
  return spn_task_done();
}
