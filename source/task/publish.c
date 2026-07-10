#include "sp.h"
#include "sp/macro.h"
#include "ctx/ctx.h"
#include "ctx/types.h"

#include "event/event.h"
#include "event/types.h"
#include "index/index.h"
#include "index/json.h"
#include "index/publish.h"
#include "pkg/id.h"
#include "semver/convert.h"
#include "task/task.h"

spn_task_step_t spn_task_publish(spn_app_t* app) {
  spn_cli_publish_t* cmd = &spn.cli.publish;

  sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;

  spn_index_info_t* index = spn_find_index(index_name);
  if (!index) {
    return spn_task_fail(SPN_ERR_INDEX_UNKNOWN, .index = { .name = index_name });
  }

  spn_publish_opts_t opts = {
    .mem = spn.mem,
    .intern = spn.intern,
    .cwd = spn.paths.project,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
  };

  spn_index_release_t release = SP_ZERO_INITIALIZE();
  spn_try_step(spn_publish_build(&opts, &release));

  if (cmd->dry) {
    sp_str_t json = spn_index_release_to_json(spn.mem, &release);
    sp_fmt_io(&spn.logger.out.base, "{}\n", sp_fmt_str(json));
    sp_fmt_io(&spn.logger.err.base, "{.cyan}: dry run, nothing published\n", sp_fmt_cstr("note"));
    return spn_task_done();
  }

  spn_evt_publish_t evt = {
    .name = spn_pkg_name_to_qualified(release.id),
    .version = spn_semver_to_str(spn.mem, release.version),
    .index = index->name,
    .url = sp_str_empty(index->publish_url) ? index->url : index->publish_url,
  };

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH,
    .publish = evt,
  });

  spn_try_step(spn_index_publish(index, &release));

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH_END,
    .publish = evt,
  });

  return spn_task_done();
}
