#include "unit/package.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "error/types.h"
#include "event/event.h"
#include "external/cc.h"
#include "external/git.h"
#include "external/tom.h"
#include "log/log.h"
#include "pkg/pkg.h"
#include "session/session.h"
#include "target/target.h"
#include "unit/types.h"

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(spn.mem, ctx->paths.stamp.dir, node->tag);
}

static spn_err_t publish_target_headers(spn_pkg_unit_t* unit, spn_target_map_t targets, bool strict) {
  sp_om_for(targets, it) {
    spn_target_info_t* target = sp_str_om_at(targets, it);

    sp_da_for(target->headers, h) {
      sp_str_t header = target->headers[h];

      sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
      sp_str_t from = sp_fs_join_path(scratch.mem, unit->paths.source, header);
      sp_str_t to = sp_fs_join_path(scratch.mem, unit->paths.include, header);

      spn_err_t err = SPN_OK;
      if (strict || sp_fs_exists(from)) {
        sp_fs_create_dir(sp_fs_parent_path(to));
        err = sp_fs_copy(from, to) ? SPN_ERROR : SPN_OK;
      }
      sp_mem_end_scratch(scratch);
      spn_try(err);
    }
  }

  return SPN_OK;
}

spn_err_t spn_pkg_unit_publish_headers(spn_pkg_unit_t* unit, bool strict) {
  spn_try(publish_target_headers(unit, unit->info->libs, strict));
  if (unit->source == SPN_PKG_SOURCE_ROOT) {
    spn_try(publish_target_headers(unit, unit->info->exes, strict));
    spn_try(publish_target_headers(unit, unit->info->scripts, strict));
    spn_try(publish_target_headers(unit, unit->info->tests, strict));
  }
  return SPN_OK;
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* unit, sp_str_t path) {
  sp_fs_create_file_str(path, unit->info->name);
}

// @spader I think this is wrong; it's called in four places and deduplicated with an atomic,
// but really we just want to add one graph node to log before anything in a package is compiled.
// I think of of the existing nodes would even suffice for this.
void spn_pkg_unit_announce_compile(spn_pkg_unit_t* unit) {
  if (!sp_atomic_s32_cas(&unit->compile_announced, 0, 1)) return;

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_COMPILE_START,
    .pkg = unit->info,
    .io = &unit->logs.io,
  });
}
