#include "graph/types.h"

#include "ctx/ctx.h"
#include "enum/enum.h"
#include "error/types.h"
#include "event/event.h"
#include "external/wasm/wasm.h"
#include "unit/package.h"

#include <setjmp.h>

static void emit_run(spn_pkg_unit_t* unit) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE,
    .pkg = unit->info,
    .io = &unit->logs.io,
  });
}

static void emit_crash(spn_pkg_unit_t* unit) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
    .pkg = unit->info,
    .io = &unit->logs.io,
    .crashed.path = sp_str_lit(""),
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

static spn_err_t run_wasm_package(spn_pkg_unit_t* unit) {
  spn_try(spn_wasm_script_open(&unit->wasm.build, unit, unit->paths.wasm.build));
  if (!spn_wasm_script_has(unit->wasm.build, "package")) return SPN_OK;

  emit_run(unit);

  sp_tm_timer_t timer = sp_tm_start_timer();
  spn_try(spn_wasm_script_call_hook(unit->wasm.build, unit, "package"));
  unit->time.package = sp_tm_read_timer(&timer);

  emit_success(unit);
  return SPN_OK;
}

static spn_err_t run_tcc_package(spn_pkg_unit_t* unit) {
  emit_run(unit);

  sp_tm_timer_t timer = sp_tm_start_timer();

  jmp_buf jump;
  s32 status = tcc_setjmp(unit->tcc->s, jump, unit->on_package);
  if (!status) {
    spn_t* spn = (spn_t*)unit;
    unit->on_package(spn);
  }
  else {
    emit_crash(unit);
    return SPN_ERROR;
  }

  unit->time.package = sp_tm_read_timer(&timer);

  emit_success(unit);
  return SPN_OK;
}

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  spn_try(publish_headers(unit));
  spn_try(publish_copies(unit));

  if (spn_wasm_enabled() && sp_fs_is_file(unit->paths.wasm.build)) {
    spn_try(run_wasm_package(unit));
  }
  else if (unit->on_package) {
    spn_try(run_tcc_package(unit));
  }

  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.package);

  return SPN_OK;
}
