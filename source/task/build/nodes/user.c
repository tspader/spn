#include "graph/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "unit/package.h"

static spn_err_t open_build_script(spn_pkg_unit_t* pkg) {
  // @spader This is denormalized and a proxy for something else we should be doing
  if (!sp_fs_is_file(pkg->paths.build)) return SPN_OK;

  sp_mutex_lock(&pkg->wasm.mutex);
  spn_err_t err = spn_wasm_script_open(&pkg->wasm.build, pkg, pkg->paths.wasm.build);
  sp_mutex_unlock(&pkg->wasm.mutex);
  return err;
}

static spn_err_t run_wasm_fn(spn_user_node_t* node) {
  spn_pkg_unit_t* pkg = node->pkg;
  spn_try(open_build_script(pkg));

  // @spader This looks messy
  spn_wasm_script_t* script = pkg->wasm.configure;
  if (spn_wasm_script_has_node_fn(pkg->wasm.build, node)) {
    script = pkg->wasm.build;
  }
  return spn_wasm_script_call_node(script, node);
}

s32 run_user_fn(spn_bg_cmd_t* cmd, void* user_data) {
  spn_user_node_t* node = (spn_user_node_t*)user_data;

  spn_pkg_unit_announce_compile(node->pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_USER_FN,
    .pkg = node->pkg->info,
    .io = &node->pkg->logs.io,
    .node = { .info = node }
  });

  if (!sp_str_empty(node->fn)) {
    spn_try(run_wasm_fn(node));
  }

  sp_str_t stamp = sp_fs_join_path(spn.mem, node->pkg->paths.stamp.dir, node->tag);
  sp_fs_create_file(stamp);

  return SPN_OK;
}
