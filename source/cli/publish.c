#include "sp.h"
#include "sp/macro.h"
#include "ctx/types.h"

#include "cli/cli.h"
#include "event/event.h"
#include "index/publish.h"

spn_task_result_t spn_task_publish(spn_app_t* app) {
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
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = {
        .kind = SPN_ERR_INDEX_UNKNOWN,
        .index = { .name = index_name },
      },
    });
    return SPN_TASK_ERROR;
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
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = result,
    });
    return SPN_TASK_ERROR;
  }

  SP_LOG("published successfully");
  return SPN_TASK_DONE;
}

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PUBLISH);
  return SP_CLI_CONTINUE;
}
