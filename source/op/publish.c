#include "sp.h"
#include "sp/macro.h"
#include "ctx/ctx.h"
#include "ctx/types.h"

#include "error/error.h"
#include "event/event.h"
#include "event/types.h"
#include "index/index.h"
#include "index/publish.h"
#include "op/op.h"
#include "pkg/id.h"
#include "project/types.h"
#include "semver/convert.h"

static spn_err_union_t publish_build(spn_ctx_t* ctx, spn_publish_request_t request, spn_index_release_t* release) {
  spn_try_union(spn_ctx_require_project(ctx));

  if (!spn_find_index(request.index)) {
    return (spn_err_union_t) { .kind = SPN_ERR_INDEX_UNKNOWN, .index = { .name = request.index } };
  }

  spn_publish_opts_t opts = {
    .mem = ctx->mem,
    .intern = ctx->intern,
    .cwd = ctx->project->paths.root,
    .url = request.url,
    .revision = request.revision,
    .allow_dirty = request.allow_dirty,
  };
  return spn_publish_build(&opts, release);
}

static spn_err_union_t publish(spn_ctx_t* ctx, spn_publish_request_t request, spn_index_release_t* release) {
  spn_index_info_t* index = spn_find_index(request.index);

  spn_evt_publish_t evt = {
    .name = spn_pkg_name_to_qualified(release->id),
    .version = spn_semver_to_str(ctx->mem, release->version),
    .index = index->name,
    .url = spn_index_publish_target(index),
  };

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH,
    .publish = evt,
  });

  spn_try_union(spn_index_publish(index, ctx->mem, release));

  spn_event_buffer_push(ctx->events, (spn_build_event_t) {
    .kind = SPN_EVENT_PUBLISH_END,
    .publish = evt,
  });

  return spn_result(SPN_OK);
}

spn_err_t spn_op_publish_build(spn_ctx_t* ctx, spn_publish_request_t request, spn_index_release_t* release) {
  return spn_err_emit(ctx, publish_build(ctx, request, release));
}

spn_err_t spn_op_publish(spn_ctx_t* ctx, spn_publish_request_t request) {
  spn_index_release_t release = sp_zero;
  spn_try(spn_op_publish_build(ctx, request, &release));
  return spn_err_emit(ctx, publish(ctx, request, &release));
}
