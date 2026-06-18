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

int main(int argc, const char *const argv[]) {
  ctx_t* ctx = ctx_get();
  ctx_init(ctx);

  spn.intern = sp_intern_new(spn_allocator);
  spn.arena = sp_mem_arena_new_ex(spn_allocator, 256, 1);
  spn.events = spn_event_buffer_new();

  s32 result = utest_main(argc, argv);

  ctx_deinit(ctx);
  return result;
}
