#include "sp.h"
#include "sp/macro.h"
#include "ctx/ctx.h"
#include "ctx/types.h"

#include "event/event.h"
#include "event/types.h"
#include "index/index.h"
#include "index/json.h"
#include "index/publish.h"
#include "op/op.h"
#include "pkg/id.h"
#include "project/types.h"
#include "semver/convert.h"

spn_err_union_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t* request) {
  sp_str_t index_name = sp_str_empty(request->index) ? sp_str_lit("core") : request->index;

  spn_index_info_t* index = spn_find_index(index_name);
  if (!index) {
    return (spn_err_union_t) { .kind = SPN_ERR_INDEX_UNKNOWN, .index = { .name = index_name } };
  }

  spn_publish_opts_t opts = {
    .mem = ctx->mem,
    .intern = ctx->intern,
    .cwd = ctx->project->paths.root,
    .url = request->url,
    .revision = request->revision,
    .allow_dirty = request->allow_dirty,
  };

  spn_index_release_t release = sp_zero;
  try_union(spn_publish_build(&opts, &release));

  if (request->dry) {
    sp_str_t json = spn_index_release_to_json(ctx->mem, &release);
    sp_fmt_io(&ctx->logger.out.base, "{}\n", sp_fmt_str(json));
    sp_fmt_io(&ctx->logger.err.base, "{.cyan}: dry run, nothing published\n", sp_fmt_cstr("note"));
    return spn_result(SPN_OK);
  }

  spn_evt_publish_t evt = {
    .name = spn_pkg_name_to_qualified(release.id),
    .version = spn_semver_to_str(ctx->mem, release.version),
    .index = index->name,
    .url = spn_index_publish_target(index),
  };

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH,
    .publish = evt,
  });

  try_union(spn_index_publish(index, ctx->mem, &release));

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH_END,
    .publish = evt,
  });

  return spn_result(SPN_OK);
}
