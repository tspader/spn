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

#include <setjmp.h>

sp_str_t spn_pkg_unit_get_node_stamp_file(spn_pkg_unit_t* ctx, spn_user_node_t* node) {
  return sp_fs_join_path(ctx->paths.stamp.dir, node->tag);
}

void spn_pkg_unit_write_stamp(spn_pkg_unit_t* unit, sp_str_t path) {
  sp_fs_create_file_str(path, unit->pkg->name);
}

spn_err_t spn_pkg_unit_call_hook(spn_pkg_unit_t* unit, spn_configure_fn_t fn) {
  jmp_buf jump;
  int status = tcc_setjmp(unit->tcc, jump, fn);
  if (!status) {
    // fn(&unit->ctx);
  }
  else {
    // @spader @log
    // What else can we get from TCC here?
    spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs.io, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_CRASHED,
      .crashed.path = sp_str_lit("")
    });
    return SPN_ERROR;
  }

  return SPN_OK;
}
