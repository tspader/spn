#include "graph/types.h"

#include "ctx/ctx.h"
#include "event/event.h"

s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .pkg = node->pkg->pkg,
    .io = &node->pkg->logs.io,
    .node = { .info = node }
  });

  if (!node->fn) return SPN_OK;

  spn_node_ctx_t ctx = {
    .user_data = node->user_data
  };

  sp_try(node->fn(&ctx));

  sp_str_t stamp = sp_fs_join_path(node->pkg->paths.stamp.dir, node->tag);
  sp_fs_create_file(stamp);

  return SPN_OK;
}

