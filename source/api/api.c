#include "sp.h"
#include "spn.h"

#include "api/api.h"
#include "api/core/types.h"
#include "abi.gen.h"
#include "ctx/types.h"
#include "event/event.h"
#include "target/types.h"
#include "unit/types.h"

s32 spn_run_ex(spn_t* ctx, spn_run_t run) {
  spn_pkg_unit_t* unit = sp_ptr_cast(spn_pkg_unit_t*, ctx);

  spn_event_buffer_push_ex(spn.events, unit->info, &unit->logs.io, (spn_build_event_t) {
    .kind = SPN_EVENT_API_CALL,
    .api_call = {
      .fn = sp_str_lit("spn_run_ex"),
      .args = run.target ? sp_str_copy(spn.mem, run.target->info->name) : sp_zero_s(sp_str_t),
    }
  });

  return SPN_OK;
}
