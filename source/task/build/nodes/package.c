#include "graph/types.h"
#include "pkg/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "unit/package.h"

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* ctx = (spn_pkg_unit_t*)user_data;

  if (ctx->on_package) {
    spn_event_buffer_push_kind(spn.events, &ctx->ctx, SPN_EVENT_BUILD_SCRIPT_PACKAGE);

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(ctx, ctx->on_package));
    ctx->time.package = sp_tm_read_timer(&timer);

    spn_event_buffer_push_ctx(spn.events, &ctx->ctx, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
      .package_ok = { .time = ctx->time.package },
    });
  }

  spn_pkg_unit_write_stamp(ctx, ctx->paths.stamp.package);

  return SPN_OK;
}
