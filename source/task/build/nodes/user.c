#include "graph/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "unit/package.h"

s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;

  spn_event_buffer_push_ctx(spn.events, &node->ctx->ctx, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .node = { .info = node }
  });

  spn_err_t err = SPN_OK;
  if (node->fn) {
    spn_node_ctx_t ctx = {
      .build = &node->ctx->ctx,
      .user_data = node->user_data
    };

    switch (node->fn(&ctx)) {
      case SPN_OK: {
        spn_pkg_unit_write_stamp(node->ctx, spn_pkg_unit_get_node_stamp_file(node->ctx, node));
        sp_str_t stamp = sp_fs_join_path(node->ctx->paths.stamp.dir, node->tag);
        sp_fs_create_file(stamp);
        break;
      }
      default: {
        break;
      }
    }
  }

  return err;
}

