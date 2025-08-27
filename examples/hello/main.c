#define SP_IMPLEMENTATION
#include "sp.h"

s32 main(s32 num_args, c8** args) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  SP_LOG("Hello, world!");
}

