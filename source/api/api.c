#include "sp.h"

#include "abi.gen.h"
#include "api/api.h"
#include "ctx/types.h"
#include "event/event.h"
#include "target/types.h"
#include "unit/types.h"

s32 spn_run_ex(spn_t* ctx, spn_run_t run) {
  spn_pkg_unit_t* unit = spn_api_unit(ctx);
  spn_target_info_t* target = sp_ptr_cast(spn_target_info_t*, run.target);

  spn_event_buffer_push(
      spn.events,
      (spn_build_event_t){
          .kind = SPN_EVENT_API_CALL,
          .pkg = unit->info,
          .api_call = {
              .fn = sp_str_lit("spn_run_ex"),
              .args = target ? sp_str_copy(spn.mem, target->name)
                             : sp_zero_s(sp_str_t),
          }});

  return SPN_OK;
}
