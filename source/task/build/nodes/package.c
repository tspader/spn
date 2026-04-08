#include "graph/types.h"
#include "pkg/types.h"

#include "ctx/ctx.h"
#include "event/event.h"
#include "unit/package.h"

s32 run_package_hook(spn_bg_cmd_t* cmd, void* user_data) {
  spn_pkg_unit_t* unit = (spn_pkg_unit_t*)user_data;

  if (unit->on_package) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE,
      .pkg = unit->pkg,
      .io = &unit->logs.io,
    });

    sp_tm_timer_t timer = sp_tm_start_timer();
    spn_try(spn_pkg_unit_call_hook(unit, unit->on_package));
    unit->time.package = sp_tm_read_timer(&timer);

    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_BUILD_SCRIPT_PACKAGE_OK,
      .pkg = unit->pkg,
      .io = &unit->logs.io,
      .package_ok = {
        .time = unit->time.package
      }
    });
  }

  spn_pkg_unit_write_stamp(unit, unit->paths.stamp.package);

  return SPN_OK;
}
