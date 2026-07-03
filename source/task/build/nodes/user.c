#include "graph/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "external/tcc/tcc.h"
#include "external/wasm/wasm.h"
#include "unit/package.h"

#include "libtcc.h"

// When compile_build_script is skipped as up to date, nothing has opened the
// module this process; open it here, under the unit mutex, since sibling
// nodes run concurrently
static spn_err_t open_build_script(spn_pkg_unit_t* pkg) {
  if (!sp_fs_is_file(pkg->paths.build)) return SPN_OK;

  sp_mutex_lock(&pkg->wasm.mutex);
  spn_err_t err = spn_wasm_script_open(&pkg->wasm.build, pkg, pkg->paths.wasm.build);
  sp_mutex_unlock(&pkg->wasm.mutex);
  return err;
}

static spn_err_t run_wasm_fn(spn_user_node_t* node) {
  spn_pkg_unit_t* pkg = node->pkg;
  spn_try(open_build_script(pkg));

  spn_wasm_script_t* script = pkg->wasm.configure;
  if (spn_wasm_script_has_node_fn(pkg->wasm.build, node)) {
    script = pkg->wasm.build;
  }
  return spn_wasm_script_call_node(script, node);
}

static spn_err_t run_native_fn(spn_user_node_t* node) {
  spn_pkg_unit_t* pkg = node->pkg;

  spn_node_fn_t fn = SP_NULLPTR;
  if (pkg->tcc) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    fn = (spn_node_fn_t)tcc_get_symbol(pkg->tcc->s, sp_str_to_cstr(scratch.mem, node->fn));
    sp_mem_end_scratch(scratch);
  }

  if (!fn) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .pkg = pkg->info,
      .io = &pkg->logs.io,
      .crashed.path = node->fn,
    });
    return SPN_ERROR;
  }

  spn_node_ctx_t ctx = {
    .user_data = node->user_data
  };
  spn_try(fn((spn_t*)pkg, &ctx));
  return SPN_OK;
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
    if (node->wasm) {
      spn_try(run_wasm_fn(node));
    }
    else {
      spn_try(run_native_fn(node));
    }
  }

  sp_str_t stamp = sp_fs_join_path(spn.mem, node->pkg->paths.stamp.dir, node->tag);
  sp_fs_create_file(stamp);

  return SPN_OK;
}
