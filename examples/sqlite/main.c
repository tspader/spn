#define SP_IMPLEMENTATION
#include "sp.h"

void sp_init() {
  static sp_allocator_malloc_t malloc_allocator = SP_ZERO_INITIALIZE();
  sp_allocator_t allocator = sp_allocator_malloc_init(&malloc_allocator);
  sp_context_push_allocator(&allocator);
}

s32 main(s32 num_args, c8** args) {
  sp_init();
  SP_LOG("hello, world!");
}

