#include "graph/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "unit/package.h"

s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;
  spn_pkg_unit_t* pkg = node->pkg;

  spn_pkg_unit_announce_compile(pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .pkg = pkg->info,
    .io = &pkg->logs.io,
    .node = { .info = node }
  });

  if (!sp_str_empty(node->fn)) {
    spn_node_ctx_t ctx = { .user_data = node->user_data };
    spn_try(spn_wasm_call_export(pkg, node->fn, SPN_ABI_KIND_NODE_CTX, &ctx));
  }

  sp_str_t stamp = sp_fs_join_path(spn.mem, pkg->paths.stamp.dir, node->tag);
  sp_fs_create_file(stamp);

  return SPN_OK;
}
