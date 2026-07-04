#include "graph/types.h"

#include "ctx/ctx.h"
#include "enum/enum.h"
#include "error/types.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "unit/package.h"

static void emit_run(spn_pkg_unit_t* unit) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE,
    .pkg = unit->info,
    .io = &unit->logs.io,
  });
}

static void emit_success(spn_pkg_unit_t* unit) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .package_ok = {
      .time = unit->time.package
    }
  });
}

static spn_err_t publish_copies(spn_pkg_unit_t* unit) {
  sp_da_for(unit->info->publish.copy, it) {
    spn_publish_copy_t* copy = &unit->info->publish.copy[it];
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

    sp_str_pair_t from = sp_str_cleave_c8(copy->from, '/');
    sp_str_pair_t to = sp_str_cleave_c8(copy->to, '/');
    s32 err = spn_copy(
      (spn_t*)unit,
      spn_cache_dir_kind_from_str(from.first), sp_str_to_cstr(scratch.mem, from.second),
      spn_cache_dir_kind_from_str(to.first), sp_str_to_cstr(scratch.mem, to.second)
    );

    sp_mem_end_scratch(scratch);
    spn_try_as(err, SPN_ERROR);
  }

  return SPN_OK;
}

static spn_err_t publish_headers(spn_pkg_unit_t* unit) {
  sp_da_for(unit->targets, t) {
    spn_target_unit_t* target = unit->targets[t];

    sp_da_for(target->info->headers, h) {
      sp_str_t header = target->info->headers[h];
      sp_str_t from = sp_fs_join_path(spn.mem, unit->paths.source, header);
      sp_str_t to = sp_fs_join_path(spn.mem, unit->paths.include, header);

      sp_fs_create_dir(sp_fs_parent_path(to));
      spn_try_as(sp_fs_copy(from, to), SPN_ERROR);
    }
  }

  return SPN_OK;
}

static spn_err_t run_wasm_package(spn_pkg_unit_t* unit, spn_wasm_script_t* script) {
  emit_run(unit);

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_node_ctx_t ctx = sp_zero;
  spn_try(spn_wasm_script_call(script, unit, sp_str_lit("package"), SPN_ABI_KIND_NODE_CTX, &ctx));
  unit->time.package = sp_tm_read_timer(&timer);

  emit_success(unit);
  return SPN_OK;
}

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  // @spader These should be merged somehow?
  spn_try(publish_headers(unit));
  spn_try(publish_copies(unit));

  spn_wasm_script_t* script = SP_NULLPTR;
  spn_try(spn_wasm_find_export(unit, sp_str_lit("package"), &script));
  if (script) {
    spn_try(run_wasm_package(unit, script));
  }

  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.package);

  return SPN_OK;
}
