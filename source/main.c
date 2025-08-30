#define SPN_IMPLEMENTATION
#include "spn.h"

s32 main(s32 num_args, const c8** args) {
  sp_init((sp_config_t) {
    .allocator = sp_allocator_default()
  });

  app = SP_ZERO_STRUCT(spn_app_t);
  spn_app_init(&app, num_args, args);
  spn_app_run(&app);

  sp_context_pop();
  return 0;
}
