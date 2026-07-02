#define SP_IMPLEMENTATION
#include "sp.h"

#define UTEST_IMPLEMENTATION
#include "utest.h"

#include "test.h"

#include "ctx/types.h"
#include "event/event.h"
#include "intern/intern.h"

UTEST_STATE();

spn_ctx_t spn;

s32 main(s32 num_args, const c8** args) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx);

  spn.intern = sp_intern_new(sp_mem_os_new());
  spn.events = spn_event_buffer_new(sp_mem_os_new());

  s32 result = utest_main(num_args, args);

  ctx_deinit(ctx);
  return result;
}
