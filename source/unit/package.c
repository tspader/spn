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
#include "target/target.h"
#include "unit/types.h"

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(spn.mem, ctx->paths.stamp.dir, node->tag);
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

